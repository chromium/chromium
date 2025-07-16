// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {IS_HIDPI} from './constants.js';
import {Runner} from './offline.js';
import type {SpritePosition} from './sprite_position.js';
import {getRandomNum} from './utils.js';

export class Cloud {
  gap: number;
  xPos: number;
  remove: boolean = false;
  private yPos: number = 0;
  private canvasCtx: CanvasRenderingContext2D;
  private spritePos: SpritePosition;

  /**
   * Cloud background item.
   * Similar to an obstacle object but without collision boxes.
   */
  constructor(
      canvas: HTMLCanvasElement, spritePos: SpritePosition,
      containerWidth: number) {
    const canvasContext = canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.xPos = containerWidth;
    this.spritePos = spritePos;
    this.gap = getRandomNum(Config.MIN_CLOUD_GAP, Config.MAX_CLOUD_GAP);

    this.init();
  }

  /**
   * Initialise the cloud. Sets the Cloud height.
   */
  init() {
    this.yPos = getRandomNum(Config.MAX_SKY_LEVEL, Config.MIN_SKY_LEVEL);
    this.draw();
  }

  /**
   * Draw the cloud.
   */
  draw() {
    const runnerImageSprite = Runner.getInstance().getRunnerImageSprite();

    this.canvasCtx.save();
    let sourceWidth = Config.WIDTH;
    let sourceHeight = Config.HEIGHT;
    const outputWidth = sourceWidth;
    const outputHeight = sourceHeight;
    if (IS_HIDPI) {
      sourceWidth = sourceWidth * 2;
      sourceHeight = sourceHeight * 2;
    }

    this.canvasCtx.drawImage(
        runnerImageSprite, this.spritePos.x, this.spritePos.y, sourceWidth,
        sourceHeight, this.xPos, this.yPos, outputWidth, outputHeight);

    this.canvasCtx.restore();
  }

  /**
   * Update the cloud position.
   */
  update(speed: number) {
    if (!this.remove) {
      this.xPos -= Math.ceil(speed);
      this.draw();

      // Mark as removable if no longer in the canvas.
      if (!this.isVisible()) {
        this.remove = true;
      }
    }
  }

  /**
   * Check if the cloud is visible on the stage.
   */
  isVisible(): boolean {
    return this.xPos + Config.WIDTH > 0;
  }
}

/**
 * Cloud object config.
 */
enum Config {
  HEIGHT = 14,
  MAX_CLOUD_GAP = 400,
  MAX_SKY_LEVEL = 30,
  MIN_CLOUD_GAP = 100,
  MIN_SKY_LEVEL = 71,
  WIDTH = 46,
}
