// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IS_HIDPI} from './constants.js';
import {Runner} from './offline.js';
import {getRandomNum} from './utils.js';


export class BackgroundEl {
  /**
   * Background item.
   * Similar to cloud, without random y position.
   * @param {HTMLCanvasElement} canvas Canvas element.
   * @param {Object} spritePos Position of image in sprite.
   * @param {number} containerWidth
   * @param {string} type Element type.
   */
  constructor(canvas, spritePos, containerWidth, type) {
    this.canvas = canvas;
    this.canvasCtx =
        /** @type {CanvasRenderingContext2D} */ (this.canvas.getContext('2d'));
    this.spritePos = spritePos;
    this.containerWidth = containerWidth;
    this.xPos = containerWidth;
    this.yPos = 0;
    this.remove = false;
    this.type = type;
    this.gap =
        getRandomNum(BackgroundEl.config.MIN_GAP, BackgroundEl.config.MAX_GAP);
    this.animTimer = 0;
    this.switchFrames = false;

    this.spriteConfig = {};
    this.init();
  }


  /**
   * Initialise the element setting the y position.
   */
  init() {
    this.spriteConfig = Runner.spriteDefinition.BACKGROUND_EL[this.type];
    if (this.spriteConfig.FIXED) {
      this.xPos = this.spriteConfig.FIXED_X_POS;
    }
    this.yPos = BackgroundEl.config.Y_POS - this.spriteConfig.HEIGHT +
        this.spriteConfig.OFFSET;
    this.draw();
  }

  /**
   * Draw the element.
   */
  draw() {
    this.canvasCtx.save();
    let sourceWidth = this.spriteConfig.WIDTH;
    let sourceHeight = this.spriteConfig.HEIGHT;
    let sourceX = this.spriteConfig.X_POS;
    const outputWidth = sourceWidth;
    const outputHeight = sourceHeight;

    if (IS_HIDPI) {
      sourceWidth *= 2;
      sourceHeight *= 2;
      sourceX *= 2;
    }

    this.canvasCtx.drawImage(
        Runner.imageSprite, sourceX, this.spritePos.y, sourceWidth,
        sourceHeight, this.xPos, this.yPos, outputWidth, outputHeight);

    this.canvasCtx.restore();
  }

  /**
   * Update the background element position.
   * @param {number} speed
   */
  update(speed) {
    if (!this.remove) {
      if (this.spriteConfig.FIXED) {
        this.animTimer += speed;
        if (this.animTimer > BackgroundEl.config.MS_PER_FRAME) {
          this.animTimer = 0;
          this.switchFrames = !this.switchFrames;
        }

        if (this.spriteConfig.FIXED_Y_POS_1 &&
            this.spriteConfig.FIXED_Y_POS_2) {
          this.yPos = this.switchFrames ? this.spriteConfig.FIXED_Y_POS_1 :
                                          this.spriteConfig.FIXED_Y_POS_2;
        }
      } else {
        // Fixed speed, regardless of actual game speed.
        this.xPos -= BackgroundEl.config.SPEED;
      }
      this.draw();

      // Mark as removable if no longer in the canvas.
      if (!this.isVisible()) {
        this.remove = true;
      }
    }
  }

  /**
   * Check if the element is visible on the stage.
   * @return {boolean}
   */
  isVisible() {
    return this.xPos + this.spriteConfig.WIDTH > 0;
  }
}

/**
 * Background element object config.
 * Real values assigned when game type changes.
 * @enum {number}
 */
BackgroundEl.config = {
  MAX_BG_ELS: 0,
  MAX_GAP: 0,
  MIN_GAP: 0,
  POS: 0,
  SPEED: 0,
  Y_POS: 0,
  MS_PER_FRAME: 0,  // only needed when BACKGROUND_EL.FIXED is true
};