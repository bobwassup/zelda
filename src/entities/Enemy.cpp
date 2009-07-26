/*
 * Copyright (C) 2009 Christopho, Zelda Solarus - http://www.zelda-solarus.com
 * 
 * Zelda: Mystery of Solarus DX is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Zelda: Mystery of Solarus DX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "entities/Enemy.h"
#include "entities/Hero.h"
#include "entities/MapEntities.h"
#include "entities/PickableItem.h"
#include "entities/CarriedItem.h"
#include "ZSDX.h"
#include "Game.h"
#include "Savegame.h"
#include "Equipment.h"
#include "Sprite.h"
#include "SpriteAnimationSet.h"
#include "ResourceManager.h"
#include "Sound.h"
#include "Map.h"
#include "Geometry.h"
#include "MapScript.h"
#include "movements/StraightMovement.h"
#include "movements/FallingHeight.h"
#include "enemies/SimpleGreenSoldier.h"
#include "enemies/Bubble.h"
#include "enemies/Tentacle.h"

/**
 * Creates an enemy.
 * This constructor can only be called by the subclasses. Use the static method create() to create enemies.
 * @param params the name and position of the enemy
 */
Enemy::Enemy(const ConstructionParameters &params):
  Detector(COLLISION_RECTANGLE | COLLISION_SPRITE, params.name, params.layer, params.x, params.y, 0, 0),
  being_hurt(false), normal_movement(NULL), invulnerable(false), vulnerable_again_date(0),
  can_attack(true), can_attack_again_date(0), immobilized(false),
  start_shaking_date(0), end_shaking_date(0) {

}

/**
 * Destructor.
 */
Enemy::~Enemy(void) {

}

/**
 * Returns the type of entity.
 * @return the type of entity
 */
EntityType Enemy::get_type() {
  return ENEMY;
}

/**
 * Creates an enemy with the specified type.
 * This method acts like a constructor, except that it can return NULL if the enemy
 * is already dead and cannot be killed again (e.g. a boss).
 * @param type type of enemy to create
 * @param name a name identifying the enemy
 * @param rank rank of the enemy: normal, miniboss or boss
 * @param savegame_variable index of the boolean variable indicating that
 * @param layer layer of the enemy on the map
 * @param x x position of the enemy on the map
 * @param y y position of the enemy on the map
 * @param direction initial direction of the enemy on the map (0 to 3)
 * this enemy is killed, or -1 if this enemy is not saved
 * @param pickable_item_subtype subtype of pickable item the enemy drops
 * @param pickable_item_savegame_variable index of the boolean variable
 */
Enemy * Enemy::create(EnemyType type, Rank rank, int savegame_variable,
		      const std::string &name, Layer layer, int x, int y, int direction,
		      PickableItem::Subtype pickable_item_subtype, int pickable_item_savegame_variable) {

  // see if the enemy is alive
  if (savegame_variable != -1 && zsdx->game->get_savegame()->get_boolean(savegame_variable)) {
    return NULL;
  }

  // create the enemy
  Enemy *enemy;
  const ConstructionParameters params = {name, layer, x, y};

  switch (type) {
    
  case SIMPLE_GREEN_SOLDIER: enemy = new SimpleGreenSoldier(params); break;
  case BUBBLE:               enemy = new Bubble(params);             break;
  case TENTACLE:             enemy = new Tentacle(params);           break;

  default:
    DIE("Unknown enemy type '" << type << "'");
  }

  // initialize the fields
  enemy->set_direction(direction);
  enemy->rank = rank;
  enemy->savegame_variable = savegame_variable;
  enemy->pickable_item_subtype = pickable_item_subtype;
  enemy->pickable_item_savegame_variable = pickable_item_savegame_variable;

  // set the default enemy features
  enemy->damage_on_hero = 1;
  enemy->magic_damage_on_hero = 0;
  enemy->life = 1;
  enemy->hurt_sound_style = HURT_SOUND_NORMAL;
  enemy->pushed_back_when_hurt = true;
  enemy->push_back_hero_on_sword = false;
  enemy->minimum_shield_needed = 0;

  for (int i = 0; i < ATTACK_NUMBER; i++) {
    enemy->vulnerabilities[i] = 1;
  }
  enemy->vulnerabilities[ATTACK_HOOKSHOT] = -2;
  enemy->vulnerabilities[ATTACK_BOOMERANG] = -2;

  return enemy;
}

/**
 * Sets the map.
 * @param map the map
 */
void Enemy::set_map(Map *map) {

  MapEntity::set_map(map);

  // let the subclass initialize the enemy
  initialize();

  for (unsigned int i = 0; i < sprites.size(); i++) {
    get_sprite(i)->get_animation_set()->enable_pixel_collisions();
  }

  restart();
}

/**
 * Returns whether this entity is an obstacle for another one.
 * @param other another entity
 * @return true if this entity is an obstacle for the other one
 */
bool Enemy::is_obstacle_for(MapEntity *other) {

  return other->get_type() == BLOCK || other->get_type() == INTERACTIVE_ENTITY;
}

/**
 * Returns whether a sensor is currently considered as an obstacle for this entity.
 * @param sensor a sensor
 * @return true if the sensor is currently an obstacle this entity
 */
bool Enemy::is_sensor_obstacle(Sensor *sensor) {
  return true;
}

/**
 * Sets the amount of damage this kind of enemy can make to the hero.
 * @param damage_on_hero number of heart quarters the player loses
 */
void Enemy::set_damage(int damage_on_hero) {
  this->damage_on_hero = damage_on_hero;
}

/**
 * Sets the amount of damage this kind of enemy can make to the hero.
 * @param damage_on_hero number of heart quarters the player loses
 * @param magic_damage_on_hero number of magic points the player loses
 */
void Enemy::set_damage(int damage_on_hero, int magic_damage_on_hero) {
  set_damage(damage_on_hero);
  this->magic_damage_on_hero = magic_damage_on_hero;
}

/**
 * Sets the number of health points of the enemy.
 * @param life number of health points of the enemy
 */
void Enemy::set_life(int life) {
  this->life = life;
}

/**
 * Sets some features of this type of enemy.
 * @param damage_on_hero number of heart quarters the player loses
 * @param life number of health points of the enemy
 */
void Enemy::set_features(int damage_on_hero, int life) {
  set_damage(damage_on_hero);
  set_life(life);
}

/**
 * Sets some features of this type of enemy.
 * @param damage_on_hero number of heart quarters the player loses
 * @param life number of health points of the enemy
 * @param hurt_sound_style the sound played when this kind of enemy gets hurt by the hero
 */
void Enemy::set_features(int damage_on_hero, int life, HurtSoundStyle hurt_sound_style) {
  set_features(damage_on_hero, life);
  this->hurt_sound_style = hurt_sound_style;
}

/**
 * Sets all features of this type of enemy.
 * @param damage_on_hero number of heart quarters the player loses
 * @param life number of health points of the enemy
 * @param hurt_sound_style the sound played when this kind of enemy gets hurt by the hero
 * @param pushed_back_when_hurt indicates whether the enemy is pushed back when it gets hurt by the hero
 * @param push_back_hero_on_sword indicates whether the hero is pushed back when he hurts the enemy with his sword
 * @param minimum_shield_needed shield number needed by the hero to avoid the attack of this enemy,
 * or 0 to make the attack unavoidable
 */
void Enemy::set_features(int damage_on_hero, int life, HurtSoundStyle hurt_sound_style,
			 bool pushed_back_when_hurt, bool push_back_hero_on_sword, int minimum_shield_needed) {
  set_features(damage_on_hero, life, hurt_sound_style);
  this->pushed_back_when_hurt = pushed_back_when_hurt;
  this->push_back_hero_on_sword = push_back_hero_on_sword;
  this->minimum_shield_needed = minimum_shield_needed;
}

/**
 * Sets whether the enemy is vulnerable to a specified attack.
 * @param attack an attack
 * @param reaction how the enemy will react
 */
void Enemy::set_vulnerability(EnemyAttack attack, int reaction) {
  vulnerabilities[attack] = reaction;
}

/**
 * Updates the enemy.
 */
void Enemy::update(void) {
  MapEntity::update();

  if (suspended) {
    return;
  }

  Uint32 now = SDL_GetTicks();

  if (being_hurt) {
    
    if (pushed_back_when_hurt) {
      StraightMovement *hurt_movement = (StraightMovement*) get_movement();
      if (hurt_movement->is_finished()) {

	being_hurt = false;

	if (life <= 0) {
	  kill();
	}
	else if (is_immobilized()) {
	  clear_movement();
	  get_sprite()->set_current_animation("immobilized");
	}
	else {
	  set_movement(normal_movement); // restore the previous movement
	  delete hurt_movement;
	  restart();
	}
      }
    }
  }

  if (life > 0 && invulnerable && now >= vulnerable_again_date) {
    invulnerable = false;
  }

  if (life > 0 && !can_attack && !is_immobilized() && now >= can_attack_again_date) {
    can_attack = true;
  }

  if (is_immobilized() && !is_killed() && now >= end_shaking_date &&
      get_sprite()->get_current_animation() == "shaking") {

    stop_immobilized();
  }

  if (is_immobilized() && !is_killed() && !is_being_hurt() && now >= start_shaking_date &&
      get_sprite()->get_current_animation() != "shaking") {

    end_shaking_date = now + 2000;
    get_sprite()->set_current_animation("shaking");
  }

  if (is_killed() && get_sprite()->is_animation_finished()) {

    // create the pickable item
    if (pickable_item_subtype != PickableItem::NONE) {
      bool will_disappear = PickableItem::can_disappear(pickable_item_subtype);
      map->get_entities()->add_entity(PickableItem::create(get_layer(), get_x(), get_y(), pickable_item_subtype,
							   pickable_item_savegame_variable, FALLING_HIGH,
							   will_disappear));
    }

    // remove the enemy
    map->get_entities()->remove_entity(this);

    // notify the script
    map->get_script()->event_enemy_dead(get_name());
  }
}

/**
 * Suspends or resumes the entity.
 * @param suspended true to suspend the entity, false to resume it
 */
void Enemy::set_suspended(bool suspended) {

  MapEntity::set_suspended(suspended);

  if (!suspended) {
    Uint32 diff = SDL_GetTicks() - when_suspended;
    vulnerable_again_date += diff;
    can_attack_again_date += diff;
    start_shaking_date += diff;
    end_shaking_date += diff;
  }
}

/**
 * Returns whether this enemy is in a normal state, i.e.
 * it is not being hurt and not immobilized.
 * When this method returns false, the subclasses of Enemy
 * should not change the enemy properties.
 */
bool Enemy::is_in_normal_state(void) {
  return !is_being_hurt() && !is_killed() && !is_immobilized();
}

/**
 * This function is called when the enemy need to restart its movement
 * because something happened (for example the enemy has just been created,
 * or it was just hurt).
 */
void Enemy::restart(void) {
  get_sprite()->set_current_animation("walking");
}

/**
 * This function is called when the enemy collides with another entity.
 * @param entity_overlapping the other entity
 * @param collision_mode the collision mode that detected the collision
 */
void Enemy::collision(MapEntity *entity_overlapping, CollisionMode collision_mode) {
  entity_overlapping->collision_with_enemy(this);
}

/**
 * This function is called when the enemy's sprite collides with another
 * entity's sprite.
 * @param entity the other entity
 * @param sprite_overlapping the sprite of this entity that is overlapping the enemy
 */
void Enemy::collision(MapEntity *entity, Sprite *sprite_overlapping) {
  entity->collision_with_enemy(this, sprite_overlapping);
}

/**
 * Attacks the hero if possible.
 * This function is called when there is a collision between the enemy and the hero.
 * @param hero the hero
 */
void Enemy::attack_hero(Hero *hero) {

  if (can_attack) {

    bool hero_protected = false;
    if (minimum_shield_needed != 0 &&
	zsdx->game->get_equipment()->get_shield() >= minimum_shield_needed) {

      double angle = hero->get_vector_angle(this);
      int protected_direction = (int) ((angle + Geometry::PI_OVER_2 / 2.0) * 4 / Geometry::TWO_PI);
      protected_direction = (protected_direction + 4) % 4;

      hero_protected = (hero->get_animation_direction() == protected_direction);
    }

    if (hero_protected) {
      attack_stopped_by_hero_shield();
    }
    else {
      hero->hurt(this, damage_on_hero, magic_damage_on_hero);
    }
  }
}

/**
 * This function is called when the hero stops an attack with his shield.
 * By default, the shield sound is played and the enemy cannot attack again for a while.
 */
void Enemy::attack_stopped_by_hero_shield(void) {
  ResourceManager::get_sound("shield")->play();
  can_attack = false;
  can_attack_again_date = SDL_GetTicks() + 1000;
}

/**
 * Returns the sound to play when the enemy is hurt.
 * @return the sound to play when the enemy is hurt
 */
Sound * Enemy::get_hurt_sound(void) {

  static const SoundId sound_ids[] = {"enemy_hurt", "monster_hurt", "boss_hurt"};
  return ResourceManager::get_sound(sound_ids[hurt_sound_style]);
}

/**
 * Makes the enemy subject to an attack.
 * @param attack type of attack
 * @param source the entity attacking the enemy (often the hero)
 */
void Enemy::try_hurt(EnemyAttack attack, MapEntity *source) {

  int result;
  if (invulnerable || vulnerabilities[attack] == 0) {
    // ignore the attack
    result = 0;
  }

  else if (vulnerabilities[attack] == -1) {
    // shield sound
    ResourceManager::get_sound("shield")->play();
    result = -1;
  }
  else if (vulnerabilities[attack] == -2) {
    hurt(source);
    immobilize();
    result = -2;
  }
  else {
    // hurt the enemy

    if (is_immobilized() && get_sprite()->get_current_animation() == "shaking") {
      stop_immobilized();
    }

    hurt(source);

    // compute the number of health points lost by the enemy
    int life_lost = vulnerabilities[attack];

    if (attack == ATTACK_SWORD) {

      // for a sword attack, the damage depends on the sword

      static const int sword_factors[] = {0, 1, 2, 4, 8};
      int sword = zsdx->game->get_equipment()->get_sword();
      life_lost *= sword_factors[sword];
      if (((Hero*) source)->get_state() == Hero::SPIN_ATTACK) {
	life_lost *= 2; // muliply by 2 if this is a spin attack
      }
    }
    else if (attack == ATTACK_THROWN_ITEM) {
      life_lost *= ((CarriedItem*) source)->get_damage_on_enemies();
    }
    life -= life_lost;

    result = life_lost;
  }

  // notify the source
  source->just_attacked_enemy(attack, this, result);
}

/**
 * Hurts the enemy.
 * Updates its state, its sprite and plays the sound.
 * @param source the entity attacking the enemy (often the hero)
 */
void Enemy::hurt(MapEntity *source) {

  // update the enemy state
  being_hurt = true;
  invulnerable = true;
  vulnerable_again_date = SDL_GetTicks() + 700;

  //can_attack = false;
  //can_attack_again_date = vulnerable_again_date;

  // graphics and sounds
  get_sprite()->set_current_animation("hurt");
  get_hurt_sound()->play();

  if (get_movement() != NULL) {
    normal_movement = get_movement();
  }

  // push the enemy back
  if (pushed_back_when_hurt) {
    double angle = source->get_vector_angle(this);
    set_movement(new StraightMovement(12, angle, 200));
  }
}

/**
 * Kills the enemy.
 * This function is called when the enemy has no more health points.
 */
void Enemy::kill(void) {

  // if the enemy is immobilized, give a rupee
  if (is_immobilized() && pickable_item_savegame_variable == -1) {
    pickable_item_subtype = PickableItem::RUPEE_1;
  }

  // stop any movement and disable attacks
  set_collision_modes(COLLISION_NONE);
  if (pushed_back_when_hurt) {
    delete normal_movement;
  }
  clear_movement();
  invulnerable = true;
  can_attack = false;

  // replace the enemy sprite
  for (unsigned int i = 0; i < sprites.size(); i++) {
    delete sprites[i];
  }
  sprites.clear();
  create_sprite("enemies/enemy_killed");
  ResourceManager::get_sound("enemy_killed")->play();

  // save the enemy state if required
  if (savegame_variable != -1) {
    zsdx->game->get_savegame()->set_boolean(savegame_variable, true);
  }
}

/**
 * Returns whether the enemy is being hurt.
 * @return true if the enemy is being hurt
 */
bool Enemy::is_being_hurt(void) {
  return being_hurt;
}

/**
 * Returns whether the enemy is killed.
 * @return true if the enemy is killed
 */
bool Enemy::is_killed(void) {
  return life <= 0 && get_sprite()->get_animation_set_id() == "enemies/enemy_killed";
}

/**
 * Immobilizes this enemy.
 */
void Enemy::immobilize(void) {
  immobilized = true;
//  start_shaking_date = SDL_GetTicks() + 5000; 
  start_shaking_date = SDL_GetTicks() + 1000; 
}

/**
 * Stops immobilizing the enemy.
 */
void Enemy::stop_immobilized(void) {
  immobilized = false;
  end_shaking_date = 0;
  set_movement(normal_movement);
  restart();
}

/**
 * Returns whether this enemy is immobilized. 
 * @return true if this enemy is immobilized 
 */
bool Enemy::is_immobilized(void) {
  return immobilized;
}

