// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FPS, IS_HIDPI} from './constants.js';
import {Runner} from './offline.js';

export class HorizonLine {
  /**
   * Horizon Line.
   * Consists of two connecting lines. Randomly assigns a flat / bumpy horizon.
   * @param {HTMLCanvasElement} canvas
   * @param {Object} lineConfig Configuration object.
   */
  constructor(canvas, lineConfig) {
    let sourceX = lineConfig.SOURCE_X;
    let sourceY = lineConfig.SOURCE_Y;

    if (IS_HIDPI) {
      sourceX *= 2;
      sourceY *= 2;
    }

    this.spritePos = {x: sourceX, y: sourceY};
    this.canvas = canvas;
    this.canvasCtx =
        /** @type {CanvasRenderingContext2D} */ (canvas.getContext('2d'));
    this.sourceDimensions = {};
    this.dimensions = lineConfig;

    this.sourceXPos =
        [this.spritePos.x, this.spritePos.x + this.dimensions.WIDTH];
    this.xPos = [];
    this.yPos = 0;
    this.bumpThreshold = 0.5;

    this.setSourceDimensions(lineConfig);
    this.draw();
  }


  /**
   * Set the source dimensions of the horizon line.
   */
  setSourceDimensions(newDimensions) {
    for (const dimension in newDimensions) {
      if (dimension !== 'SOURCE_X' && dimension !== 'SOURCE_Y') {
        if (IS_HIDPI) {
          if (dimension !== 'YPOS') {
            this.sourceDimensions[dimension] = newDimensions[dimension] * 2;
          }
        } else {
          this.sourceDimensions[dimension] = newDimensions[dimension];
        }
        this.dimensions[dimension] = newDimensions[dimension];
      }
    }

    this.xPos = [0, newDimensions.WIDTH];
    this.yPos = newDimensions.YPOS;
  }

  /**
   * Return the crop x position of a type.
   */
  getRandomType() {
    return Math.random() > this.bumpThreshold ? this.dimensions.WIDTH : 0;
  }

  /**
   * Draw the horizon line.
   */
  draw() {
    this.canvasCtx.drawImage(
        Runner.imageSprite, this.sourceXPos[0], this.spritePos.y,
        this.sourceDimensions.WIDTH, this.sourceDimensions.HEIGHT, this.xPos[0],
        this.yPos, this.dimensions.WIDTH, this.dimensions.HEIGHT);

    this.canvasCtx.drawImage(
        Runner.imageSprite, this.sourceXPos[1], this.spritePos.y,
        this.sourceDimensions.WIDTH, this.sourceDimensions.HEIGHT, this.xPos[1],
        this.yPos, this.dimensions.WIDTH, this.dimensions.HEIGHT);
  }

  /**
   * Update the x position of an individual piece of the line.
   * @param {number} pos Line position.
   * @param {number} increment
   */
  updateXPos(pos, increment) {
    const line1 = pos;
    const line2 = pos === 0 ? 1 : 0;

    this.xPos[line1] -= increment;
    this.xPos[line2] = this.xPos[line1] + this.dimensions.WIDTH;

    if (this.xPos[line1] <= -this.dimensions.WIDTH) {
      this.xPos[line1] += this.dimensions.WIDTH * 2;
      this.xPos[line2] = this.xPos[line1] - this.dimensions.WIDTH;
      this.sourceXPos[line1] = this.getRandomType() + this.spritePos.x;
    }
  }

  /**
   * Update the horizon line.
   * @param {number} deltaTime
   * @param {number} speed
   */
  update(deltaTime, speed) {
    const increment = Math.floor(speed * (FPS / 1000) * deltaTime);

    if (this.xPos[0] <= 0) {
      this.updateXPos(0, increment);
    } else {
      this.updateXPos(1, increment);
    }
    this.draw();
  }

  /**
   * Reset horizon to the starting position.
   */
  reset() {
    this.xPos[0] = 0;
    this.xPos[1] = this.dimensions.WIDTH;
  }
}