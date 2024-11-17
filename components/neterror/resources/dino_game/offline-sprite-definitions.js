// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* @const
 * Add matching sprite definition and config to spriteDefinitionByType.
 */
export const GAME_TYPE = ['altgame'];

//******************************************************************************

/**
 * Collision box object.
 * @param {number} x X position.
 * @param {number} y Y Position.
 * @param {number} w Width.
 * @param {number} h Height.
 * @constructor
 */
export function CollisionBox(x, y, w, h) {
  this.x = x;
  this.y = y;
  this.width = w;
  this.height = h;
}

/**
 * Obstacle definitions.
 * minGap: minimum pixel space between obstacles.
 * multipleSpeed: Speed at which multiples are allowed.
 * speedOffset: speed faster / slower than the horizon.
 * minSpeed: Minimum speed which the obstacle can make an appearance.
 *
 * @typedef {{
 *   type: string,
 *   width: number,
 *   height: number,
 *   yPos: number,
 *   multipleSpeed: number,
 *   minGap: number,
 *   minSpeed: number,
 *   collisionBoxes: Array<CollisionBox>,
 * }}
 */
let ObstacleType;

/**
 * T-Rex runner sprite definitions.
 */
export const spriteDefinitionByType = {
  original: {
    LDPI: {
      BACKGROUND_EL: {x: 86, y: 2},
      CACTUS_LARGE: {x: 332, y: 2},
      CACTUS_SMALL: {x: 228, y: 2},
      OBSTACLE_2: {x: 332, y: 2},
      OBSTACLE: {x: 228, y: 2},
      CLOUD: {x: 86, y: 2},
      HORIZON: {x: 2, y: 54},
      MOON: {x: 484, y: 2},
      PTERODACTYL: {x: 134, y: 2},
      RESTART: {x: 2, y: 68},
      TEXT_SPRITE: {x: 655, y: 2},
      TREX: {x: 848, y: 2},
      STAR: {x: 645, y: 2},
      COLLECTABLE: {x: 0, y: 0},
      ALT_GAME_END: {x: 32, y: 0},
    },
    HDPI: {
      BACKGROUND_EL: {x: 166, y: 2},
      CACTUS_LARGE: {x: 652, y: 2},
      CACTUS_SMALL: {x: 446, y: 2},
      OBSTACLE_2: {x: 652, y: 2},
      OBSTACLE: {x: 446, y: 2},
      CLOUD: {x: 166, y: 2},
      HORIZON: {x: 2, y: 104},
      MOON: {x: 954, y: 2},
      PTERODACTYL: {x: 260, y: 2},
      RESTART: {x: 2, y: 130},
      TEXT_SPRITE: {x: 1294, y: 2},
      TREX: {x: 1678, y: 2},
      STAR: {x: 1276, y: 2},
      COLLECTABLE: {x: 0, y: 0},
      ALT_GAME_END: {x: 64, y: 0},
    },
    MAX_GAP_COEFFICIENT: 1.5,
    MAX_OBSTACLE_LENGTH: 3,
    HAS_CLOUDS: 1,
    BOTTOM_PAD: 10,
    TREX: {
      WAITING_1: {x: 44, w: 44, h: 47, xOffset: 0},
      WAITING_2: {x: 0, w: 44, h: 47, xOffset: 0},
      RUNNING_1: {x: 88, w: 44, h: 47, xOffset: 0},
      RUNNING_2: {x: 132, w: 44, h: 47, xOffset: 0},
      JUMPING: {x: 0, w: 44, h: 47, xOffset: 0},
      CRASHED: {x: 220, w: 44, h: 47, xOffset: 0},
      COLLISION_BOXES: [
        new CollisionBox(22, 0, 17, 16),
        new CollisionBox(1, 18, 30, 9),
        new CollisionBox(10, 35, 14, 8),
        new CollisionBox(1, 24, 29, 5),
        new CollisionBox(5, 30, 21, 4),
        new CollisionBox(9, 34, 15, 4),
      ],
    },
    /** @type {Array<ObstacleType>} */
    OBSTACLES: [
      {
        type: 'CACTUS_SMALL',
        width: 17,
        height: 35,
        yPos: 105,
        multipleSpeed: 4,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 7, 5, 27),
          new CollisionBox(4, 0, 6, 34),
          new CollisionBox(10, 4, 7, 14),
        ],
      },
      {
        type: 'CACTUS_LARGE',
        width: 25,
        height: 50,
        yPos: 90,
        multipleSpeed: 7,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 12, 7, 38),
          new CollisionBox(8, 0, 7, 49),
          new CollisionBox(13, 10, 10, 38),
        ],
      },
      {
        type: 'PTERODACTYL',
        width: 46,
        height: 40,
        yPos: [100, 75, 50],    // Variable height.
        yPosMobile: [100, 50],  // Variable height mobile.
        multipleSpeed: 999,
        minSpeed: 8.5,
        minGap: 150,
        collisionBoxes: [
          new CollisionBox(15, 15, 16, 5),
          new CollisionBox(18, 21, 24, 6),
          new CollisionBox(2, 14, 4, 3),
          new CollisionBox(6, 10, 4, 7),
          new CollisionBox(10, 8, 6, 9),
        ],
        numFrames: 2,
        frameRate: 1000 / 6,
        speedOffset: .8,
      },
      {
        type: 'COLLECTABLE',
        width: 31,
        height: 24,
        yPos: 104,
        multipleSpeed: 1000,
        minGap: 9999,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 0, 32, 25),
        ],
      },
    ],
    BACKGROUND_EL: {
      'CLOUD': {
        HEIGHT: 14,
        MAX_CLOUD_GAP: 400,
        MAX_SKY_LEVEL: 30,
        MIN_CLOUD_GAP: 100,
        MIN_SKY_LEVEL: 71,
        OFFSET: 4,
        WIDTH: 46,
        X_POS: 1,
        Y_POS: 120,
      },
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 1,
      MAX_GAP: 400,
      MIN_GAP: 100,
      POS: 0,
      SPEED: 0.5,
      Y_POS: 125,
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 52, WIDTH: 600, HEIGHT: 12, YPOS: 127},
    ],
    ALT_GAME_OVER_TEXT_CONFIG: {
      TEXT_X: 32,
      TEXT_Y: 0,
      TEXT_WIDTH: 246,
      TEXT_HEIGHT: 17,
      FLASH_DURATION: 1500,
      FLASHING: false,
    },
  },
  altgame: {
    LDPI: {
      BACKGROUND_EL: {x: 260, y: 19},
      OBSTACLE1: {x: 152, y: 65},
      OBSTACLE2: {x: 188, y: 65},
      OBSTACLE3: {x: 152, y: 65},
      OBSTACLE4: {x: 188, y: 65},
      OBSTACLE5: {x: 0, y: 60},
      OBSTACLE6: {x: 42, y: 58},
      OBSTACLE7: {x: 98, y: 58},
      OBSTACLE8: {x: 96, y: 19},
      HORIZON: {x: 0, y: 3},
      TREX: {x: 557, y: 63},
      COLLECTABLE: {x: 193, y: 19},
    },
    HDPI: {
      BACKGROUND_EL: {x: 520, y: 38},
      OBSTACLE1: {x: 304, y: 130},
      OBSTACLE2: {x: 376, y: 130},
      OBSTACLE3: {x: 304, y: 130},
      OBSTACLE4: {x: 376, y: 130},
      OBSTACLE5: {x: 0, y: 120},
      OBSTACLE6: {x: 84, y: 116},
      OBSTACLE7: {x: 196, y: 116},
      OBSTACLE8: {x: 192, y: 38},
      HORIZON: {x: 0, y: 6},
      TREX: {x: 1114, y: 126},
      COLLECTABLE: {x: 386, y: 38},
    },
    MAX_GAP_COEFFICIENT: 1.5,
    MAX_OBSTACLE_LENGTH: 2,
    HAS_CLOUDS: 0,
    BOTTOM_PAD: 10,
    TREX: {
      MAX_JUMP_HEIGHT: 50,
      MIN_JUMP_HEIGHT: 40,
      INITIAL_JUMP_VELOCITY: -10,
      RUNNING_1: {x: 96, w: 49, h: 47, xOffset: 0},
      RUNNING_2: {x: 145, w: 49, h: 47, xOffset: 0},
      JUMPING: {x: 47, w: 49, h: 47, xOffset: 0},
      CRASHED: {x: 194, w: 61, h: 47, xOffset: 0},
      DUCKING_1: {x: 257, w: 55, h: 26, xOffset: 0},
      DUCKING_2: {x: 316, w: 55, h: 26, xOffset: 0},
      COLLISION_BOXES: [
        new CollisionBox(22, 0, 17, 16),
        new CollisionBox(1, 18, 30, 9),
        new CollisionBox(10, 35, 14, 8),
        new CollisionBox(1, 24, 29, 5),
        new CollisionBox(5, 30, 21, 4),
        new CollisionBox(9, 34, 15, 4),
      ],
    },
    /** @type {Array<ObstacleType>} */
    OBSTACLES: [
      {
        type: 'OBSTACLE1',
        width: 36,
        height: 45,
        yPos: 95,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 17, 8, 28),
          new CollisionBox(6, 3, 24, 42),
          new CollisionBox(28, 17, 8, 28),
        ],
      },
      {
        type: 'OBSTACLE2',
        width: 36,
        height: 45,
        yPos: 95,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 17, 8, 28),
          new CollisionBox(6, 3, 24, 42),
          new CollisionBox(28, 17, 8, 28),
        ],
      },
      {
        type: 'OBSTACLE3',
        width: 72,
        height: 45,
        yPos: 95,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 8,
        collisionBoxes: [
          new CollisionBox(0, 17, 8, 28),
          new CollisionBox(6, 3, 24, 42),
          new CollisionBox(28, 17, 8, 28),
          new CollisionBox(36, 17, 8, 28),
          new CollisionBox(42, 3, 24, 42),
          new CollisionBox(64, 17, 8, 28),
        ],
      },
      {
        type: 'OBSTACLE4',
        width: 72,
        height: 45,
        yPos: 95,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 8,
        collisionBoxes: [
          new CollisionBox(0, 17, 8, 28),
          new CollisionBox(6, 3, 24, 42),
          new CollisionBox(28, 17, 8, 28),
          new CollisionBox(36, 17, 8, 28),
          new CollisionBox(42, 3, 24, 42),
          new CollisionBox(64, 17, 8, 28),
        ],
      },
      {
        type: 'OBSTACLE5',
        width: 42,
        height: 50,
        yPos: 95,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 5,
        collisionBoxes: [
          new CollisionBox(0, 0, 42, 50),
        ],
      },
      {
        type: 'OBSTACLE6',
        width: 56,
        height: 52,
        yPos: 93,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 7,
        collisionBoxes: [
          new CollisionBox(0, 11, 8, 40),
          new CollisionBox(8, 0, 19, 51),
          new CollisionBox(27, 11, 28, 40),
        ],
      },
      {
        type: 'OBSTACLE7',
        width: 54,
        height: 52,
        yPos: 93,
        multipleSpeed: 999,
        minGap: 120,
        minSpeed: 6,
        collisionBoxes: [
          new CollisionBox(0, 11, 19, 40),
          new CollisionBox(19, 0, 19, 51),
          new CollisionBox(38, 14, 15, 37),
        ],
      },
      {
        type: 'OBSTACLE8',
        width: 49,
        height: 20,
        yPos: [100, 75, 50],    // Variable height.
        yPosMobile: [100, 50],  // Variable height mobile.
        multipleSpeed: 999,
        minSpeed: 8.5,
        minGap: 150,
        collisionBoxes: [
          new CollisionBox(15, 15, 16, 5),
          new CollisionBox(18, 21, 24, 6),
          new CollisionBox(2, 14, 4, 3),
          new CollisionBox(6, 10, 4, 7),
          new CollisionBox(10, 8, 6, 9),
        ],
        numFrames: 2,
        frameRate: 1000 / 6,
        speedOffset: .8,
      },
    ],
    BACKGROUND_EL: {
      'GROUP1': {
        HEIGHT: 91,
        MAX_CLOUD_GAP: 600,
        MAX_SKY_LEVEL: 0,
        MIN_CLOUD_GAP: 300,
        MIN_SKY_LEVEL: 0,
        OFFSET: 11,
        WIDTH: 131,
        X_POS: 260,
      },
      'GROUP2': {
        HEIGHT: 91,
        MAX_CLOUD_GAP: 600,
        MAX_SKY_LEVEL: 0,
        MIN_CLOUD_GAP: 300,
        MIN_SKY_LEVEL: 0,
        OFFSET: 11,
        WIDTH: 166,
        X_POS: 391,
      },
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 8,
      MAX_GAP: 600,
      MIN_GAP: 300,
      POS: 0,
      SPEED: 0.8,
      Y_POS: 122,
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 3, WIDTH: 600, HEIGHT: 12, YPOS: 128},
    ],
  },
};
