// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Runner} from '../offline.js';

import {IS_HIDPI} from './constants.js';
import {getRandomNum} from './utils.js';

export class Cloud {
  /**
   * Cloud background item.
   * Similar to an obstacle object but without collision boxes.
   * @param {HTMLCanvasElement} canvas Canvas element.
   * @param {Object} spritePos Position of image in sprite.
   * @param {number} containerWidth
   */
  constructor(canvas, spritePos, containerWidth) {
    this.canvas = canvas;
    this.canvasCtx =
        /** @type {CanvasRenderingContext2D} */ (this.canvas.getContext('2d'));
    this.spritePos = spritePos;
    this.containerWidth = containerWidth;
    this.xPos = containerWidth;
    this.yPos = 0;
    this.remove = false;
    this.gap =
        getRandomNum(Cloud.config.MIN_CLOUD_GAP, Cloud.config.MAX_CLOUD_GAP);

    this.init();
  }

  /**
   * Initialise the cloud. Sets the Cloud height.
   */
  init() {
    this.yPos =
        getRandomNum(Cloud.config.MAX_SKY_LEVEL, Cloud.config.MIN_SKY_LEVEL);
    this.draw();
  }

  /**
   * Draw the cloud.
   */
  draw() {
    this.canvasCtx.save();
    let sourceWidth = Cloud.config.WIDTH;
    let sourceHeight = Cloud.config.HEIGHT;
    const outputWidth = sourceWidth;
    const outputHeight = sourceHeight;
    if (IS_HIDPI) {
      sourceWidth = sourceWidth * 2;
      sourceHeight = sourceHeight * 2;
    }

    this.canvasCtx.drawImage(
        Runner.imageSprite, this.spritePos.x, this.spritePos.y, sourceWidth,
        sourceHeight, this.xPos, this.yPos, outputWidth, outputHeight);

    this.canvasCtx.restore();
  }

  /**
   * Update the cloud position.
   * @param {number} speed
   */
  update(speed) {
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
   * @return {boolean}
   */
  isVisible() {
    return this.xPos + Cloud.config.WIDTH > 0;
  }
}

/**
 * Cloud object config.
 * @enum {number}
 */
Cloud.config = {
  HEIGHT: 14,
  MAX_CLOUD_GAP: 400,
  MAX_SKY_LEVEL: 30,
  MIN_CLOUD_GAP: 100,
  MIN_SKY_LEVEL: 71,
  WIDTH: 46,
};