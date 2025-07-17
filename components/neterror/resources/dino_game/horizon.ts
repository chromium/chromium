// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/js/assert.js';

import {BackgroundEl, getGlobalConfig as getBackgroundElGlobalConfig, setGlobalConfig as setBackgroundElGlobalConfig} from './background_el.js';
import {Cloud} from './cloud.js';
import type {Dimensions} from './dimensions.js';
import {HorizonLine} from './horizon_line.js';
import {NightMode} from './night_mode.js';
import {Obstacle, setMaxGapCoefficient as setMaxObstacleGapCoefficient, setMaxObstacleLength} from './obstacle.js';
import {Runner} from './offline.js';
import type {ObstacleType, SpritePositions} from './offline_sprite_definitions.js';
import {spriteDefinitionByType} from './offline_sprite_definitions.js';
import {getRandomNum} from './utils.js';

/**
 * Horizon background class.
 */
export class Horizon {
  obstacles: Obstacle[] = [];

  private canvas: HTMLCanvasElement;
  private canvasCtx: CanvasRenderingContext2D;
  private config: HorizonConfig = horizonConfig;
  private dimensions: Dimensions;
  private gapCoefficient: number;
  private obstacleHistory: Array<keyof SpritePositions> = [];
  private cloudFrequency: number;
  private spritePos: SpritePositions;
  private nightMode: NightMode;
  private altGameModeActive: boolean = false;
  private obstacleTypes: ObstacleType[] = [];


  // Cloud
  private clouds: Cloud[] = [];
  private cloudSpeed: number;

  // Background elements
  private backgroundEls: BackgroundEl[] = [];
  private lastEl: string|null = null;

  // Horizon
  private horizonLines: HorizonLine[] = [];

  constructor(
      canvas: HTMLCanvasElement, spritePos: SpritePositions,
      dimensions: Dimensions, gapCoefficient: number) {
    this.canvas = canvas;
    const canvasContext = canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.dimensions = dimensions;
    this.gapCoefficient = gapCoefficient;
    this.cloudFrequency = this.config.CLOUD_FREQUENCY;
    this.spritePos = spritePos;
    this.cloudSpeed = this.config.BG_CLOUD_SPEED;

    // Initialise the horizon. Just add the line and a cloud. No obstacles.
    this.obstacleTypes = spriteDefinitionByType.original.obstacles;
    this.addCloud();
    const runnerSpriteDefinition = Runner.getInstance().getSpriteDefinition();
    assert(runnerSpriteDefinition);

    // Multiple Horizon lines
    for (let i = 0; i < runnerSpriteDefinition.lines.length; i++) {
      this.horizonLines.push(
          new HorizonLine(this.canvas, runnerSpriteDefinition.lines[i]!));
    }

    this.nightMode =
        new NightMode(this.canvas, this.spritePos.moon, this.dimensions.height);
  }

  /**
   * Update obstacle definitions based on the speed of the game.
   */
  adjustObstacleSpeed() {
    for (let i = 0; i < this.obstacleTypes.length; i++) {
      if (Runner.getInstance().hasSlowdown) {
        this.obstacleTypes[i]!.multipleSpeed =
            this.obstacleTypes[i]!.multipleSpeed / 2;
        this.obstacleTypes[i]!.minGap *= 1.5;
        this.obstacleTypes[i]!.minSpeed = this.obstacleTypes[i]!.minSpeed / 2;

        // Convert variable y position obstacles to fixed.
        const obstacleYpos = this.obstacleTypes[i]!.yPos;
        if (Array.isArray(obstacleYpos) && obstacleYpos.length > 1) {
          this.obstacleTypes[i]!.yPos = obstacleYpos[0]!;
        }
      }
    }
  }

  /**
   * Update sprites to correspond to change in sprite sheet.
   */
  enableAltGameMode(spritePos: SpritePositions) {
    const runnerSpriteDefinition = Runner.getInstance().getSpriteDefinition();
    assert(runnerSpriteDefinition);

    // Clear existing horizon objects.
    this.clouds = [];
    this.backgroundEls = [];

    this.altGameModeActive = true;
    this.spritePos = spritePos;

    this.obstacleTypes = runnerSpriteDefinition.obstacles;
    this.adjustObstacleSpeed();

    setMaxObstacleGapCoefficient(runnerSpriteDefinition.maxGapCoefficient);
    setMaxObstacleLength(runnerSpriteDefinition.maxObstacleLength);

    setBackgroundElGlobalConfig(runnerSpriteDefinition.backgroundElConfig);

    this.horizonLines = [];
    for (let i = 0; i < runnerSpriteDefinition.lines.length; i++) {
      this.horizonLines.push(
          new HorizonLine(this.canvas, runnerSpriteDefinition.lines[i]!));
    }
    this.reset();
  }

  /**
   * @param updateObstacles Used as an override to prevent
   *     the obstacles from being updated / added. This happens in the
   *     ease in section.
   * @param showNightMode Night mode activated.
   */
  update(
      deltaTime: number, currentSpeed: number, updateObstacles: boolean,
      showNightMode: boolean) {
    const runnerSpriteDefinition = Runner.getInstance().getSpriteDefinition();
    assert(runnerSpriteDefinition);
    if (this.altGameModeActive) {
      this.updateBackgroundEls(deltaTime);
    }

    for (const line of this.horizonLines) {
      line.update(deltaTime, currentSpeed);
    }

    if (!this.altGameModeActive || runnerSpriteDefinition.hasClouds) {
      this.nightMode.update(showNightMode);
      this.updateClouds(deltaTime, currentSpeed);
    }

    if (updateObstacles) {
      this.updateObstacles(deltaTime, currentSpeed);
    }
  }

  /**
   * Update background element positions. Also handles creating new elements.
   */
  private updateBackgroundEl(
      elSpeed: number, bgElArray: Array<Cloud|BackgroundEl>, maxBgEl: number,
      bgElAddFunction: () => void, frequency: number) {
    const numElements = bgElArray.length;

    if (!numElements) {
      bgElAddFunction();
      return;
    }

    for (let i = numElements - 1; i >= 0; i--) {
      bgElArray[i]!.update(elSpeed);
    }

    const lastEl = bgElArray.at(-1)!;

    // Check for adding a new element.
    if (numElements < maxBgEl &&
        (this.dimensions.width - lastEl.xPos) > lastEl.gap &&
        frequency > Math.random()) {
      bgElAddFunction();
    }
  }

  /**
   * Update the cloud positions.
   */
  private updateClouds(deltaTime: number, speed: number) {
    const elSpeed = this.cloudSpeed / 1000 * deltaTime * speed;
    this.updateBackgroundEl(
        elSpeed, this.clouds, this.config.MAX_CLOUDS, this.addCloud.bind(this),
        this.cloudFrequency);

    // Remove expired elements.
    this.clouds = this.clouds.filter(obj => !obj.remove);
  }

  /**
   * Update the background element positions.
   */
  private updateBackgroundEls(deltaTime: number) {
    this.updateBackgroundEl(
        deltaTime, this.backgroundEls, getBackgroundElGlobalConfig().maxBgEls,
        this.addBackgroundEl.bind(this), this.cloudFrequency);

    // Remove expired elements.
    this.backgroundEls = this.backgroundEls.filter(obj => !obj.remove);
  }

  /**
   * Update the obstacle positions.
   */
  private updateObstacles(deltaTime: number, currentSpeed: number) {
    const updatedObstacles = this.obstacles.slice(0);

    for (const obstacle of this.obstacles) {
      obstacle.update(deltaTime, currentSpeed);

      // Clean up existing obstacles.
      if (obstacle.remove) {
        updatedObstacles.shift();
      }
    }
    this.obstacles = updatedObstacles;

    if (this.obstacles.length > 0) {
      const lastObstacle = this.obstacles.at(-1);

      if (lastObstacle && !lastObstacle.followingObstacleCreated &&
          lastObstacle.isVisible() &&
          (lastObstacle.xPos + lastObstacle.width + lastObstacle.gap) <
              this.dimensions.width) {
        this.addNewObstacle(currentSpeed);
        lastObstacle.followingObstacleCreated = true;
      }
    } else {
      // Create new obstacles.
      this.addNewObstacle(currentSpeed);
    }
  }

  removeFirstObstacle() {
    this.obstacles.shift();
  }

  /**
   * Add a new obstacle.
   */
  addNewObstacle(currentSpeed: number) {
    const obstacleCount =
        this.obstacleTypes[this.obstacleTypes.length - 1]!.type !==
                'collectable' ||
            (Runner.getInstance().isAltGameModeEnabled() &&
                 !this.altGameModeActive ||
             this.altGameModeActive) ?
        this.obstacleTypes.length - 1 :
        this.obstacleTypes.length - 2;
    const obstacleTypeIndex =
        obstacleCount > 0 ? getRandomNum(0, obstacleCount) : 0;
    const obstacleType = this.obstacleTypes[obstacleTypeIndex]!;

    // Check for multiples of the same type of obstacle.
    // Also check obstacle is available at current speed.
    if ((obstacleCount > 0 && this.duplicateObstacleCheck(obstacleType.type)) ||
        currentSpeed < obstacleType.minSpeed) {
      this.addNewObstacle(currentSpeed);
    } else {
      const obstacleSpritePos = this.spritePos[obstacleType.type];

      this.obstacles.push(new Obstacle(
          this.canvasCtx, obstacleType, obstacleSpritePos, this.dimensions,
          this.gapCoefficient, currentSpeed, obstacleType.width,
          this.altGameModeActive));

      this.obstacleHistory.unshift(obstacleType.type);

      if (this.obstacleHistory.length > 1) {
        const maxObstacleDuplicationValue =
            Runner.getInstance().getConfig().maxObstacleDuplication;
        assert(maxObstacleDuplicationValue);
        this.obstacleHistory.splice(maxObstacleDuplicationValue);
      }
    }
  }

  /**
   * Returns whether the previous two obstacles are the same as the next one.
   * Maximum duplication is set in config value MAX_OBSTACLE_DUPLICATION.
   */
  duplicateObstacleCheck(nextObstacleType: keyof SpritePositions): boolean {
    let duplicateCount = 0;

    for (const obstacle of this.obstacleHistory) {
      duplicateCount = obstacle === nextObstacleType ? duplicateCount + 1 : 0;
    }
    const maxObstacleDuplicationValue =
        Runner.getInstance().getConfig().maxObstacleDuplication;
    assert(maxObstacleDuplicationValue);
    return duplicateCount >= maxObstacleDuplicationValue;
  }

  /**
   * Reset the horizon layer.
   * Remove existing obstacles and reposition the horizon line.
   */
  reset() {
    this.obstacles = [];
    for (let l = 0; l < this.horizonLines.length; l++) {
      this.horizonLines[l]!.reset();
    }

    this.nightMode.reset();
  }

  /**
   * Update the canvas width and scaling.
   */
  resize(width: number, height: number) {
    this.canvas.width = width;
    this.canvas.height = height;
  }

  /**
   * Add a new cloud to the horizon.
   */
  addCloud() {
    this.clouds.push(
        new Cloud(this.canvas, this.spritePos.cloud, this.dimensions.width));
  }

  /**
   * Add a random background element to the horizon.
   */
  addBackgroundEl() {
    const runnerSpriteDefinition = Runner.getInstance().getSpriteDefinition();
    assert(runnerSpriteDefinition);
    const backgroundElTypes = Object.keys(runnerSpriteDefinition.backgroundEl);

    if (backgroundElTypes.length > 0) {
      let index = getRandomNum(0, backgroundElTypes.length - 1);
      let type = backgroundElTypes[index]!;

      // Add variation if available.
      while (type === this.lastEl && backgroundElTypes.length > 1) {
        index = getRandomNum(0, backgroundElTypes.length - 1);
        type = backgroundElTypes[index]!;
      }

      this.lastEl = type;
      this.backgroundEls.push(new BackgroundEl(
          this.canvas, this.spritePos.backgroundEl, this.dimensions.width,
          type));
    }
  }
}

interface HorizonConfig {
  BG_CLOUD_SPEED: number;
  BUMPY_THRESHOLD: number;
  CLOUD_FREQUENCY: number;
  HORIZON_HEIGHT: number;
  MAX_CLOUDS: number;
}

/**
 * Horizon config.
 */
const horizonConfig: HorizonConfig = {
  BG_CLOUD_SPEED: 0.2,
  BUMPY_THRESHOLD: .3,
  CLOUD_FREQUENCY: .5,
  HORIZON_HEIGHT: 16,
  MAX_CLOUDS: 6,
};
