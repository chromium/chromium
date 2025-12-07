// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {BackgroundElConfig, BackgroundElSpriteConfig} from './background_el.js';
import type {AltGameEndConfig, AltGameModePanelDimensions} from './game_over_panel.js';
import type {HorizonLineConfig} from './horizon_line.js';
import type {SpritePosition} from './sprite_position.js';
import type {AltGameModeSpriteConfig as AltTrexSpriteDefinition} from './trex.js';

/*
 * List of alternative game types defined in spriteDefinitionByType.
 */
export const GAME_TYPE: Array<keyof SpriteDefinitionByType> = [];

//******************************************************************************

/**
 * Collision box object.
 */
export class CollisionBox {
  x: number;
  y: number;
  width: number;
  height: number;

  constructor(x: number, y: number, width: number, height: number) {
    this.x = x;
    this.y = y;
    this.width = width;
    this.height = height;
  }
}

/**
 * Obstacle definitions.
 * minGap: minimum pixel space between obstacles.
 * multipleSpeed: Speed at which multiples are allowed.
 * speedOffset: speed faster / slower than the horizon.
 * minSpeed: Minimum speed which the obstacle can make an appearance.
 */
export interface ObstacleType {
  type: keyof SpritePositions;
  width: number;
  height: number;
  yPos: number|number[];
  // Only set whenever yPos is an array.
  yPosMobile?: number[];
  multipleSpeed: number;
  minGap: number;
  minSpeed: number;
  speedOffset?: number;
  collisionBoxes: CollisionBox[];
  numFrames?: number;
  // Only set whenever numFrames is set.
  frameRate?: number;
}

export interface SpritePositions {
  backgroundEl: SpritePosition;
  cactusLarge: SpritePosition;
  cactusSmall: SpritePosition;
  obstacle2: SpritePosition;
  obstacle: SpritePosition;
  cloud: SpritePosition;
  horizon: SpritePosition;
  moon: SpritePosition;
  pterodactyl: SpritePosition;
  restart: SpritePosition;
  textSprite: SpritePosition;
  tRex: SpritePosition;
  star: SpritePosition;
  collectable: SpritePosition;
  altGameEnd: SpritePosition;
}

export interface SpriteDefinition {
  ldpi: SpritePositions;
  hdpi: SpritePositions;
  maxGapCoefficient: number;
  maxObstacleLength: number;
  hasClouds: boolean;
  bottomPad: number;
  tRex?: AltTrexSpriteDefinition;
  obstacles: ObstacleType[];
  backgroundEl: {
    [key: string]: BackgroundElSpriteConfig,
  };
  backgroundElConfig: BackgroundElConfig;
  lines: HorizonLineConfig[];
  altGameOverTextConfig: AltGameModePanelDimensions;
  altGameEndConfig?: AltGameEndConfig;
}

export interface SpriteDefinitionByType {
  original: SpriteDefinition;
}

/**
 * T-Rex runner sprite definitions.
 */
export const spriteDefinitionByType: SpriteDefinitionByType = {
  original: {
    ldpi: {
      backgroundEl: {x: 86, y: 2},
      cactusLarge: {x: 332, y: 2},
      cactusSmall: {x: 228, y: 2},
      obstacle2: {x: 332, y: 2},
      obstacle: {x: 228, y: 2},
      cloud: {x: 86, y: 2},
      horizon: {x: 2, y: 54},
      moon: {x: 484, y: 2},
      pterodactyl: {x: 134, y: 2},
      restart: {x: 2, y: 68},
      textSprite: {x: 655, y: 2},
      tRex: {x: 848, y: 2},
      star: {x: 645, y: 2},
      collectable: {x: 0, y: 0},
      altGameEnd: {x: 32, y: 0},
    },
    hdpi: {
      backgroundEl: {x: 166, y: 2},
      cactusLarge: {x: 652, y: 2},
      cactusSmall: {x: 446, y: 2},
      obstacle2: {x: 652, y: 2},
      obstacle: {x: 446, y: 2},
      cloud: {x: 166, y: 2},
      horizon: {x: 2, y: 104},
      moon: {x: 954, y: 2},
      pterodactyl: {x: 260, y: 2},
      restart: {x: 2, y: 130},
      textSprite: {x: 1294, y: 2},
      tRex: {x: 1678, y: 2},
      star: {x: 1276, y: 2},
      collectable: {x: 0, y: 0},
      altGameEnd: {x: 64, y: 0},
    },
    maxGapCoefficient: 1.5,
    maxObstacleLength: 3,
    hasClouds: true,
    bottomPad: 10,
    obstacles: [
      {
        type: 'cactusSmall',
        width: 17,
        height: 35,
        yPos: 105,
        multipleSpeed: 4,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          {x: 0, y: 7, width: 5, height: 27},
          {x: 4, y: 0, width: 6, height: 34},
          {x: 10, y: 4, width: 7, height: 14},
        ],
      },
      {
        type: 'cactusLarge',
        width: 25,
        height: 50,
        yPos: 90,
        multipleSpeed: 7,
        minGap: 120,
        minSpeed: 0,
        collisionBoxes: [
          {x: 0, y: 12, width: 7, height: 38},
          {x: 8, y: 0, width: 7, height: 49},
          {x: 13, y: 10, width: 10, height: 38},
        ],
      },
      {
        type: 'pterodactyl',
        width: 46,
        height: 40,
        yPos: [100, 75, 50],    // Variable height.
        yPosMobile: [100, 50],  // Variable height mobile.
        multipleSpeed: 999,
        minSpeed: 8.5,
        minGap: 150,
        collisionBoxes: [
          {x: 15, y: 15, width: 16, height: 5},
          {x: 18, y: 21, width: 24, height: 6},
          {x: 2, y: 14, width: 4, height: 3},
          {x: 6, y: 10, width: 4, height: 7},
          {x: 10, y: 8, width: 6, height: 9},
        ],
        numFrames: 2,
        frameRate: 1000 / 6,
        speedOffset: .8,
      },
      {
        type: 'collectable',
        width: 31,
        height: 24,
        yPos: 104,
        multipleSpeed: 1000,
        minGap: 9999,
        minSpeed: 0,
        collisionBoxes: [
          {x: 0, y: 0, width: 32, height: 25},
        ],
      },
    ],
    backgroundEl: {
      'CLOUD': {
        height: 14,
        offset: 4,
        width: 46,
        xPos: 1,
        fixed: false,
      },
    },
    backgroundElConfig: {
      maxBgEls: 1,
      maxGap: 400,
      minGap: 100,
      pos: 0,
      speed: 0.5,
      yPos: 125,
    },
    lines: [
      {sourceX: 2, sourceY: 52, width: 600, height: 12, yPos: 127},
    ],
    altGameOverTextConfig: {
      textX: 32,
      textY: 0,
      textWidth: 246,
      textHeight: 17,
      flashDuration: 1500,
      flashing: false,
    },
  },
};
