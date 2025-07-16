// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {FPS, IS_HIDPI, IS_MOBILE} from './constants.js';
import type {Dimensions} from './dimensions.js';
import {Runner} from './offline.js';
import type {ObstacleType} from './offline_sprite_definitions.js';
import {CollisionBox} from './offline_sprite_definitions.js';
import type {SpritePosition} from './sprite_position.js';
import {getRandomNum} from './utils.js';

/**
 * Coefficient for calculating the maximum gap.
 */
let maxGapCoefficient: number = 1.5;

/**
 * Maximum obstacle grouping count.
 */
let maxObstacleLength: number = 3;

export function setMaxGapCoefficient(coefficient: number) {
  maxGapCoefficient = coefficient;
}

export function setMaxObstacleLength(length: number) {
  maxObstacleLength = length;
}

export class Obstacle {
  collisionBoxes: CollisionBox[] = [];
  followingObstacleCreated: boolean = false;
  gap: number = 0;
  jumpAlerted: boolean = false;
  remove: boolean = false;
  size: number;
  width: number = 0;
  xPos: number;
  yPos: number = 0;
  typeConfig: ObstacleType;

  private canvasCtx: CanvasRenderingContext2D;
  private spritePos: SpritePosition;
  private gapCoefficient: number;
  private speedOffset: number = 0;
  private altGameModeActive: boolean;
  private imageSprite: CanvasImageSource;
  // For animated obstacles.
  private currentFrame: number = 0;
  private timer: number = 0;

  /**
   * Obstacle.
   */
  constructor(
      canvasCtx: CanvasRenderingContext2D, type: ObstacleType,
      spriteImgPos: SpritePosition, dimensions: Dimensions,
      gapCoefficient: number, speed: number, xOffset: number = 0,
      isAltGameMode: boolean = false) {
    const runner = Runner.getInstance();
    this.canvasCtx = canvasCtx;
    this.spritePos = spriteImgPos;
    this.typeConfig = type;
    this.gapCoefficient =
        runner.hasSlowdown ? gapCoefficient * 2 : gapCoefficient;
    this.size = getRandomNum(1, maxObstacleLength);
    this.xPos = dimensions.width + xOffset;
    this.altGameModeActive = isAltGameMode;
    const imageSprite = this.typeConfig.type === 'collectable' ?
        runner.getAltCommonImageSprite() :
        this.altGameModeActive ? runner.getRunnerAltGameImageSprite() :
                                 runner.getRunnerImageSprite();
    assert(imageSprite);
    this.imageSprite = imageSprite;

    this.init(speed);
  }


  /**
   * Initialise the DOM for the obstacle.
   */
  private init(speed: number) {
    this.cloneCollisionBoxes();

    // Only allow sizing if we're at the right speed.
    if (this.size > 1 && this.typeConfig.multipleSpeed > speed) {
      this.size = 1;
    }

    this.width = this.typeConfig.width * this.size;

    // Check if obstacle can be positioned at various heights.
    if (Array.isArray(this.typeConfig.yPos)) {
      assert(Array.isArray(this.typeConfig.yPosMobile));
      const yPosConfig =
          IS_MOBILE ? this.typeConfig.yPosMobile : this.typeConfig.yPos;
      const randomYPos = yPosConfig[getRandomNum(0, yPosConfig.length - 1)];
      assert(randomYPos);
      this.yPos = randomYPos;
    } else {
      this.yPos = this.typeConfig.yPos;
    }

    this.draw();

    // Make collision box adjustments,
    // Central box is adjusted to the size as one box.
    //      ____        ______        ________
    //    _|   |-|    _|     |-|    _|       |-|
    //   | |<->| |   | |<--->| |   | |<----->| |
    //   | | 1 | |   | |  2  | |   | |   3   | |
    //   |_|___|_|   |_|_____|_|   |_|_______|_|
    //
    if (this.size > 1) {
      assert(this.collisionBoxes.length >= 3);
      this.collisionBoxes[1]!.width = this.width -
          this.collisionBoxes[0]!.width - this.collisionBoxes[2]!.width;
      this.collisionBoxes[2]!.x = this.width - this.collisionBoxes[2]!.width;
    }

    // For obstacles that go at a different speed from the horizon.
    if (this.typeConfig.speedOffset) {
      this.speedOffset = Math.random() > 0.5 ? this.typeConfig.speedOffset :
                                               -this.typeConfig.speedOffset;
    }

    this.gap = this.getGap(this.gapCoefficient, speed);

    // Increase gap for audio cues enabled.
    if (Runner.getInstance().hasAudioCues) {
      this.gap *= 2;
    }
  }

  /**
   * Draw and crop based on size.
   */
  private draw() {
    let sourceWidth = this.typeConfig.width;
    let sourceHeight = this.typeConfig.height;

    if (IS_HIDPI) {
      sourceWidth = sourceWidth * 2;
      sourceHeight = sourceHeight * 2;
    }

    // X position in sprite.
    let sourceX =
        (sourceWidth * this.size) * (0.5 * (this.size - 1)) + this.spritePos.x;

    // Animation frames.
    if (this.currentFrame > 0) {
      sourceX += sourceWidth * this.currentFrame;
    }

    this.canvasCtx.drawImage(
        this.imageSprite, sourceX, this.spritePos.y, sourceWidth * this.size,
        sourceHeight, this.xPos, this.yPos, this.typeConfig.width * this.size,
        this.typeConfig.height);
  }

  /**
   * Obstacle frame update.
   */
  update(deltaTime: number, speed: number) {
    if (!this.remove) {
      if (this.typeConfig.speedOffset) {
        speed += this.speedOffset;
      }
      this.xPos -= Math.floor((speed * FPS / 1000) * deltaTime);

      // Update frame
      if (this.typeConfig.numFrames) {
        assert(this.typeConfig.frameRate);
        this.timer += deltaTime;
        if (this.timer >= this.typeConfig.frameRate) {
          this.currentFrame =
              this.currentFrame === this.typeConfig.numFrames - 1 ?
              0 :
              this.currentFrame + 1;
          this.timer = 0;
        }
      }
      this.draw();

      if (!this.isVisible()) {
        this.remove = true;
      }
    }
  }

  /**
   * Calculate a random gap size.
   * - Minimum gap gets wider as speed increases
   */
  getGap(gapCoefficient: number, speed: number): number {
    const minGap = Math.round(
        this.width * speed + this.typeConfig.minGap * gapCoefficient);
    const maxGap = Math.round(minGap * maxGapCoefficient);
    return getRandomNum(minGap, maxGap);
  }

  /**
   * Check if obstacle is visible.
   */
  isVisible() {
    return this.xPos + this.width > 0;
  }

  /**
   * Make a copy of the collision boxes, since these will change based on
   * obstacle type and size.
   */
  cloneCollisionBoxes() {
    const collisionBoxes = this.typeConfig.collisionBoxes;

    for (let i = collisionBoxes.length - 1; i >= 0; i--) {
      this.collisionBoxes[i] = new CollisionBox(
          collisionBoxes[i]!.x, collisionBoxes[i]!.y, collisionBoxes[i]!.width,
          collisionBoxes[i]!.height);
    }
  }
}
