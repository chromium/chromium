// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* @const */
const GAME_TYPE = ['type_1', 'type_2', 'type_3', 'type_4', 'type_5'];

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
Runner.spriteDefinitionByType = {
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
      COLLECTABLE: {x: 2, y: 2},
      ALT_GAME_END: {x: 121, y: 2}
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
      COLLECTABLE: {x: 4, y: 4},
      ALT_GAME_END: {x: 242, y: 4}
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
        new CollisionBox(22, 0, 17, 16), new CollisionBox(1, 18, 30, 9),
        new CollisionBox(10, 35, 14, 8), new CollisionBox(1, 24, 29, 5),
        new CollisionBox(5, 30, 21, 4), new CollisionBox(9, 34, 15, 4)
      ]
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
          new CollisionBox(0, 7, 5, 27), new CollisionBox(4, 0, 6, 34),
          new CollisionBox(10, 4, 7, 14)
        ]
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
          new CollisionBox(0, 12, 7, 38), new CollisionBox(8, 0, 7, 49),
          new CollisionBox(13, 10, 10, 38)
        ]
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
          new CollisionBox(15, 15, 16, 5), new CollisionBox(18, 21, 24, 6),
          new CollisionBox(2, 14, 4, 3), new CollisionBox(6, 10, 4, 7),
          new CollisionBox(10, 8, 6, 9)
        ],
        numFrames: 2,
        frameRate: 1000 / 6,
        speedOffset: .8
      },
      {
        type: 'COLLECTABLE',
        width: 12,
        height: 38,
        yPos: 90,
        multipleSpeed: 999,
        minGap: 999,
        minSpeed: 0,
        collisionBoxes: [new CollisionBox(0, 0, 12, 38)]
      }
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
        Y_POS: 120
      }
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 1,
      MAX_GAP: 400,
      MIN_GAP: 100,
      POS: 0,
      SPEED: 0.5,
      Y_POS: 125
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 52, WIDTH: 600, HEIGHT: 12, YPOS: 127},
    ],
    ALT_GAME_END_CONFIG: {
      WIDTH: 15,
      HEIGHT: 17,
      X_OFFSET: 0,
      Y_OFFSET: 0,
    },
    ALT_GAME_OVER_TEXT_CONFIG: {
      TEXT_X: 14,
      TEXT_Y: 2,
      TEXT_WIDTH: 108,
      TEXT_HEIGHT: 15,
      FLASH_DURATION: 1500
    }
  },
  type_1: {
    LDPI: {
      OBSTACLE_1: {x: 631, y: 2},
      OBSTACLE_2: {x: 656, y: 2},
      OBSTACLE_3: {x: 697, y: 2},
      OBSTACLE_4: {x: 754, y: 2},
      OBSTACLE_5: {x: 781, y: 2},
      OBSTACLE_6: {x: 826, y: 2},
      BACKGROUND_EL: {x: 0, y: 120},
      CLOUD: {x: 890, y: 2},
      HORIZON: {x: 2, y: 54},
      TREX: {x: 252, y: 2}
    },
    HDPI: {
      OBSTACLE_1: {x: 1262, y: 2},
      OBSTACLE_2: {x: 1312, y: 2},
      OBSTACLE_3: {x: 1394, y: 2},
      OBSTACLE_4: {x: 1508, y: 2},
      OBSTACLE_5: {x: 1562, y: 2},
      OBSTACLE_6: {x: 1652, y: 2},
      BACKGROUND_EL: {x: 0, y: 240},
      CLOUD: {x: 1780, y: 3},
      HORIZON: {x: 4, y: 108},
      TREX: {x: 504, y: 2}
    },
    ALT_GAME_END_CONFIG: {WIDTH: 15, HEIGHT: 17, X_OFFSET: 19, Y_OFFSET: 17},
    MAX_GAP_COEFFICIENT: 0.56,
    MAX_OBSTACLE_LENGTH: 1,
    HAS_CLOUDS: 1,
    BOTTOM_PAD: 10,
    TREX: {
      MAX_JUMP_HEIGHT: 50,
      MIN_JUMP_HEIGHT: 50,
      INITIAL_JUMP_VELOCITY: -10,
      RUNNING_1: {x: 137, w: 44, h: 49, xOffset: 0},
      RUNNING_2: {x: 183, w: 44, h: 47, xOffset: 0},
      CRASHED: {x: 335, w: 44, h: 47, xOffset: 0},
      JUMPING: {x: 230, w: 59, h: 49, xOffset: 6},
      COLLISION_BOXES: [
        new CollisionBox(22, 0, 17, 16), new CollisionBox(0, 16, 32, 9),
        new CollisionBox(3, 24, 27, 6), new CollisionBox(5, 30, 21, 4)
      ]
    },
    OBSTACLES: [
      {
        type: 'OBSTACLE_1',
        width: 24,
        height: 60,
        yPos: 106,
        multipleSpeed: 4,
        minGap: 70,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 0, 3, 26), new CollisionBox(3, 5, 8, 31),
          new CollisionBox(11, 24, 11, 10)
        ]
      },
      {
        type: 'OBSTACLE_2',
        width: 40,
        height: 60,
        yPos: 106,
        multipleSpeed: 4,
        minGap: 90,
        minSpeed: 5,
        collisionBoxes: [
          new CollisionBox(0, 0, 3, 26), new CollisionBox(3, 5, 24, 31),
          new CollisionBox(27, 24, 11, 10)
        ]
      },
      {
        type: 'OBSTACLE_3',
        width: 57,
        height: 60,
        yPos: 106,
        multipleSpeed: 4,
        minGap: 100,
        minSpeed: 7,
        collisionBoxes: [
          new CollisionBox(0, 0, 3, 26), new CollisionBox(3, 5, 40, 31),
          new CollisionBox(27, 43, 11, 10)
        ]
      },
      {
        type: 'OBSTACLE_4',
        width: 27,
        height: 44,
        yPos: 102,
        multipleSpeed: 7,
        minGap: 110,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(0, 0, 3, 26), new CollisionBox(3, 3, 8, 31),
          new CollisionBox(11, 24, 11, 10)
        ]
      },
      {
        type: 'OBSTACLE_5',
        width: 45,
        height: 44,
        yPos: 102,
        multipleSpeed: 7,
        minGap: 120,
        minSpeed: 7.5,
        collisionBoxes: [
          new CollisionBox(0, 0, 4, 26), new CollisionBox(4, 3, 26, 31),
          new CollisionBox(30, 30, 12, 11)
        ]
      },
      {
        type: 'OBSTACLE_6',
        width: 63,
        height: 44,
        yPos: 102,
        multipleSpeed: 7,
        minGap: 140,
        minSpeed: 7.5,
        collisionBoxes: [
          new CollisionBox(0, 0, 3, 26), new CollisionBox(4, 3, 44, 39),
          new CollisionBox(48, 24, 12, 11)
        ]
      }
    ],
    BACKGROUND_EL: {
      'BACKGROUND_0': {HEIGHT: 93, WIDTH: 423, Y_POS: 120, X_POS: 1, OFFSET: 4}
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 1,
      MAX_GAP: 600,
      MIN_GAP: 600,
      POS: 0,
      SPEED: 0.2,
      Y_POS: 125
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 54, WIDTH: 600, HEIGHT: 12, YPOS: 125},
      {SOURCE_X: 2, SOURCE_Y: 84, WIDTH: 600, HEIGHT: 12, YPOS: 138}
    ]
  },
  type_2: {
    LDPI: {
      OBSTACLE_1: {x: 655, y: 2},
      BACKGROUND_EL: {x: 0, y: 60},
      CLOUD: {x: 963, y: 3},
      HORIZON: {x: 2, y: 54},
      TREX: {x: 252, y: 2}
    },
    HDPI: {
      OBSTACLE_1: {x: 1310, y: 2},
      BACKGROUND_EL: {x: 0, y: 120},
      CLOUD: {x: 1926, y: 3},
      HORIZON: {x: 4, y: 108},
      TREX: {x: 504, y: 2},
    },
    ALT_GAME_END_CONFIG: {WIDTH: 15, HEIGHT: 17, X_OFFSET: 19, Y_OFFSET: 18},
    MAX_GAP_COEFFICIENT: 0.56,
    MAX_OBSTACLE_LENGTH: 1,
    HAS_CLOUDS: 0,
    BOTTOM_PAD: 10,
    TREX: {
      MAX_JUMP_HEIGHT: 30,
      MIN_JUMP_HEIGHT: 30,
      INITIAL_JUMP_VELOCITY: -19,
      RUNNING_1: {x: 137, w: 44, h: 49, xOffset: 0},
      RUNNING_2: {x: 183, w: 44, h: 49, xOffset: 0},
      CRASHED: {x: 359, w: 44, h: 43, xOffset: 0},
      JUMPING: {x: 228, w: 43, h: 44, xOffset: 2.5},
      COLLISION_BOXES: [
        new CollisionBox(22, 0, 17, 16), new CollisionBox(17, 37, 7, 7),
        new CollisionBox(10, 17, 19, 20)
      ]
    },
    OBSTACLES: [{
      type: 'OBSTACLE_1',
      width: 54,
      height: 54,
      yPos: 90,
      multipleSpeed: 4,
      minGap: 70,
      minSpeed: 0,
      collisionBoxes: [
        new CollisionBox(0, 8, 20, 43), new CollisionBox(21, 6, 8, 42),
        new CollisionBox(32, 2, 18, 49)
      ]
    }],
    BACKGROUND_EL: {
      'BACKGROUND_0': {HEIGHT: 120, WIDTH: 89, Y_POS: 40, X_POS: 1, OFFSET: 4},
      'BACKGROUND_1':
          {HEIGHT: 108, WIDTH: 130, Y_POS: 40, X_POS: 92, OFFSET: 4},
      'BACKGROUND_2':
          {HEIGHT: 28, WIDTH: 204, Y_POS: 40, X_POS: 223, OFFSET: 4},
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 2,
      MAX_GAP: 550,
      MIN_GAP: 400,
      POS: 0,
      SPEED: 0.5,
      Y_POS: 125
    },
    LINES: [{SOURCE_X: 2, SOURCE_Y: 54, WIDTH: 600, HEIGHT: 5, YPOS: 125}]
  },
  type_3: {
    LDPI: {
      OBSTACLE_1: {x: 611, y: 2},
      OBSTACLE_2: {x: 634, y: 2},
      OBSTACLE_3: {x: 671, y: 2},
      OBSTACLE_4: {x: 722, y: 2},
      OBSTACLE_5: {x: 762, y: 2},
      OBSTACLE_6: {x: 806, y: 2},
      BACKGROUND_EL: {x: 0, y: 65},
      CLOUD: {x: 888, y: 2},
      HORIZON: {x: 2, y: 58},
      TREX: {x: 252, y: 2}
    },
    HDPI: {
      OBSTACLE_1: {x: 1222, y: 2},
      OBSTACLE_2: {x: 1268, y: 2},
      OBSTACLE_3: {x: 1342, y: 2},
      OBSTACLE_4: {x: 1444, y: 2},
      OBSTACLE_5: {x: 1524, y: 2},
      OBSTACLE_6: {x: 1612, y: 2},
      BACKGROUND_EL: {x: 0, y: 130},
      CLOUD: {x: 1776, y: 3},
      HORIZON: {x: 4, y: 116},
      TREX: {x: 504, y: 2}
    },
    ALT_GAME_END_CONFIG: {WIDTH: 15, HEIGHT: 17, X_OFFSET: 23, Y_OFFSET: 17},
    MAX_GAP_COEFFICIENT: 0.56,
    MAX_OBSTACLE_LENGTH: 1,
    BOTTOM_PAD: 10,
    HAS_CLOUDS: 1,
    TREX: {
      MAX_JUMP_HEIGHT: 45,
      MIN_JUMP_HEIGHT: 30,
      INITIAL_JUMP_VELOCITY: -10,
      RUNNING_1: {x: 104, w: 51, h: 57, xOffset: 0},
      RUNNING_2: {x: 156, w: 51, h: 57, xOffset: 0},
      CRASHED: {x: 309, w: 51, h: 57, xOffset: 0},
      JUMPING: {x: 208, w: 51, h: 57, xOffset: 0},
      COLLISION_BOXES:
          [new CollisionBox(28, 35, 19, 11), new CollisionBox(3, 44, 26, 4)]
    },
    OBSTACLES: [
      {
        type: 'OBSTACLE_1',
        width: 24,
        height: 18,
        yPos: 117,
        multipleSpeed: 4,
        minGap: 50,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(11, 2, 3, 2), new CollisionBox(7, 4, 11, 10),
          new CollisionBox(2, 9, 5, 6)
        ]
      },
      {
        type: 'OBSTACLE_2',
        width: 40,
        height: 22,
        yPos: 117,
        multipleSpeed: 4,
        minGap: 60,
        minSpeed: 4.5,
        collisionBoxes: [
          new CollisionBox(11, 2, 3, 2), new CollisionBox(7, 5, 23, 10),
          new CollisionBox(2, 9, 5, 6)
        ]
      },
      {
        type: 'OBSTACLE_3',
        width: 49,
        height: 22,
        yPos: 117,
        multipleSpeed: 4,
        minGap: 80,
        minSpeed: 7,
        collisionBoxes: [
          new CollisionBox(11, 2, 3, 2), new CollisionBox(8, 5, 39, 10),
          new CollisionBox(2, 9, 5, 6)
        ]
      },
      {
        type: 'OBSTACLE_4',
        width: 37,
        height: 26,
        yPos: 113,
        multipleSpeed: 7,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          new CollisionBox(4, 16, 5, 8), new CollisionBox(9, 12, 7, 12),
          new CollisionBox(16, 5, 10, 19)
        ]
      },
      {
        type: 'OBSTACLE_5',
        width: 45,
        height: 30,
        yPos: 113,
        multipleSpeed: 7,
        minGap: 120,
        minSpeed: 5.5,
        collisionBoxes: [
          new CollisionBox(4, 16, 5, 8), new CollisionBox(9, 12, 7, 12),
          new CollisionBox(16, 5, 10, 19), new CollisionBox(26, 14, 13, 11)
        ]
      },
      {
        type: 'OBSTACLE_6',
        width: 79,
        height: 30,
        yPos: 113,
        multipleSpeed: 7,
        minGap: 150,
        minSpeed: 7,
        collisionBoxes: [
          new CollisionBox(4, 16, 5, 8), new CollisionBox(9, 12, 7, 12),
          new CollisionBox(16, 5, 10, 19), new CollisionBox(26, 14, 13, 11),
          new CollisionBox(40, 18, 10, 6), new CollisionBox(50, 12, 10, 12),
          new CollisionBox(57, 5, 10, 19)
        ]
      }
    ],
    BACKGROUND_EL: {
      'BACKGROUND_0': {
        HEIGHT: 78,
        OFFSET: 6,
        WIDTH: 105,
        X_POS: 425,
        FIXED_X_POS: 0,
        FIXED_Y_POS_1: 54,
        FIXED_Y_POS_2: 51,
        FIXED: true
      }
    },
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 1,
      MAX_GAP: 550,
      MIN_GAP: 400,
      POS: 0,
      SPEED: 0.2,
      Y_POS: 125,
      MS_PER_FRAME: 250
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 58, WIDTH: 600, HEIGHT: 8, YPOS: 125},
    ]
  },
  type_4: {
    LDPI: {
      OBSTACLE_1: {x: 514, y: 2},
      OBSTACLE_2: {x: 543, y: 2},
      OBSTACLE_3: {x: 599, y: 2},
      OBSTACLE_4: {x: 643, y: 2},
      BACKGROUND_EL: {x: 811, y: 2},
      CLOUD: {x: 888, y: 2},
      WALL: {x: 2, y: 54},
      HORIZON: {x: 2, y: 81},
      TREX: {x: 252, y: 2}
    },
    HDPI: {
      OBSTACLE_1: {x: 1028, y: 2},
      OBSTACLE_2: {x: 1086, y: 2},
      OBSTACLE_3: {x: 1198, y: 2},
      OBSTACLE_4: {x: 1286, y: 2},
      BACKGROUND_EL: {x: 1622, y: 4},
      CLOUD: {x: 1776, y: 3},
      WALL: {x: 2, y: 108},
      HORIZON: {x: 4, y: 162},
      TREX: {x: 504, y: 2}
    },
    ALT_GAME_END_CONFIG: {WIDTH: 15, HEIGHT: 17, X_OFFSET: 38, Y_OFFSET: 16},
    MAX_GAP_COEFFICIENT: 0.56,
    MAX_OBSTACLE_LENGTH: 1,
    BOTTOM_PAD: 43,
    HAS_CLOUDS: 0,
    TREX: {
      GRAVITY: 0.36,
      MAX_JUMP_HEIGHT: 20,
      MIN_JUMP_HEIGHT: 18,
      INITIAL_JUMP_VELOCITY: -20,
      INVERT_JUMP: 1,
      RUNNING_1: {x: 0, w: 65, h: 30, xOffset: 0},
      RUNNING_2: {x: 67, w: 65, h: 30, xOffset: 0},
      CRASHED: {x: 196, w: 65, h: 30, xOffset: 0},
      JUMPING: {x: 133.5, w: 65, h: 30, xOffset: 0},
      COLLISION_BOXES: [
        new CollisionBox(17, 4, 49, 9), new CollisionBox(20, 17, 23, 4),
        new CollisionBox(19, 20, 10, 7), new CollisionBox(17, 13, 42, 4)
      ]
    },
    OBSTACLES: [
      {
        type: 'OBSTACLE_1',
        width: 27,
        height: 11,
        yPos: 80,
        multipleSpeed: 4,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [new CollisionBox(0, 2, 27, 8)]
      },
      {
        type: 'OBSTACLE_2',
        width: 54,
        height: 11,
        yPos: 80,
        multipleSpeed: 4,
        minGap: 140,
        minSpeed: 7,
        collisionBoxes: [new CollisionBox(0, 2, 52, 8)]
      },
      {
        type: 'OBSTACLE_3',
        width: 42,
        height: 16,
        yPos: 76,
        multipleSpeed: 4,
        minGap: 170,
        minSpeed: 3,
        collisionBoxes: [new CollisionBox(0, 2, 40, 14)]
      }
    ],
    BACKGROUND_EL_CONFIG: {
      SPEED: 0.5,
      POS: 0,
      MAX_BG_ELS: 3,
      MIN_GAP: 100,
      MAX_GAP: 400,
      Y_POS: 100
    },
    BACKGROUND_EL: {
      'BACKGROUND_0':
          {HEIGHT: 32, WIDTH: 30, Y_POS: 2, X_POS: 811, OFFSET: -65},
      'BACKGROUND_1':
          {HEIGHT: 37, WIDTH: 40, Y_POS: 2, X_POS: 842, OFFSET: -13},
      'BACKGROUND_2': {HEIGHT: 33, WIDTH: 82, Y_POS: 2, X_POS: 727, OFFSET: -40}
    },
    LINES: [
      {SOURCE_X: 2, SOURCE_Y: 81, WIDTH: 600, HEIGHT: 12, YPOS: 78},
      {SOURCE_X: 2, SOURCE_Y: 54, WIDTH: 600, HEIGHT: 12, YPOS: 56}
    ]
  },
  type_5: {
    LDPI: {
      OBSTACLE_1: {x: 458, y: 2},
      OBSTACLE_2: {x: 458, y: 2},
      BACKGROUND_EL: {x: 0, y: 0},
      CLOUD: {x: 482, y: 2},
      WALL: {x: 2, y: 54},
      HORIZON: {x: 2, y: 77},
      TREX: {x: 252, y: 2}
    },
    HDPI: {
      OBSTACLE_1: {x: 916, y: 2},
      OBSTACLE_2: {x: 916, y: 2},
      BACKGROUND_EL: {x: 0, y: 0},
      CLOUD: {x: 963, y: 3},
      WALL: {x: 2, y: 108},
      HORIZON: {x: 4, y: 154},
      TREX: {x: 504, y: 2}
    },
    ALT_GAME_END_CONFIG: {WIDTH: 15, HEIGHT: 17, X_OFFSET: 24, Y_OFFSET: 23},
    MAX_GAP_COEFFICIENT: 2.5,
    MAX_OBSTACLE_LENGTH: 1,
    BOTTOM_PAD: 12,
    HAS_CLOUDS: 1,
    TREX: {
      MAX_JUMP_HEIGHT: 30,
      MIN_JUMP_HEIGHT: 30,
      INITIAL_JUMP_VELOCITY: -10,
      RUNNING_1: {x: 0, w: 51, h: 67, xOffset: 0},
      RUNNING_2: {x: 50, w: 51, h: 67, xOffset: 0},
      CRASHED: {x: 156, w: 51, h: 67, xOffset: 0},
      JUMPING: {x: 103, w: 54, h: 67, xOffset: 0},
      COLLISION_BOXES: [
        new CollisionBox(35, 30, 13, 9), new CollisionBox(19, 51, 22, 9),
        new CollisionBox(9, 51, 9, 13), new CollisionBox(4, 27, 31, 28)
      ]
    },
    OBSTACLES: [{
      type: 'OBSTACLE_1',
      width: 21,
      height: 57,
      yPos: 93,
      multipleSpeed: 999,
      minGap: 40,
      minSpeed: 0,
      collisionBoxes: [
        new CollisionBox(0, 0, 3, 41), new CollisionBox(3, 5, 14, 39),
        new CollisionBox(16, 7, 4, 43)
      ]
    }],
    BACKGROUND_EL_CONFIG: {
      MAX_BG_ELS: 4,
      MAX_GAP: 420,
      MIN_GAP: 320,
      POS: 0,
      SPEED: 0.3,
      Y_POS: 125
    },
    BACKGROUND_EL: {
      'BACKGROUND_0': {HEIGHT: 40, WIDTH: 170, Y_POS: 100, X_POS: 0, OFFSET: 10}
    },
    LINES: [{SOURCE_X: 2, SOURCE_Y: 71, WIDTH: 600, HEIGHT: 12, YPOS: 123}]
  }
};
