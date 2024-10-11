// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IS_HIDPI} from './constants.js';
import {spriteDefinitionByType} from './offline-sprite-definitions.js';
import {Runner} from './offline.js';
import {getRandomNum} from './utils.js';

export class NightMode {
  /**
   * Nightmode shows a moon and stars on the horizon.
   * @param {HTMLCanvasElement} canvas
   * @param {number} spritePos
   * @param {number} containerWidth
   */
  constructor(canvas, spritePos, containerWidth) {
    this.spritePos = spritePos;
    this.canvas = canvas;
    this.canvasCtx =
        /** @type {CanvasRenderingContext2D} */ (canvas.getContext('2d'));
    this.xPos = containerWidth - 50;
    this.yPos = 30;
    this.currentPhase = 0;
    this.opacity = 0;
    this.containerWidth = containerWidth;
    this.stars = [];
    this.drawStars = false;
    this.placeStars();
  }


  /**
   * Update moving moon, changing phases.
   * @param {boolean} activated Whether night mode is activated.
   */
  update(activated) {
    // Moon phase.
    if (activated && this.opacity === 0) {
      this.currentPhase++;

      if (this.currentPhase >= NightMode.phases.length) {
        this.currentPhase = 0;
      }
    }

    // Fade in / out.
    if (activated && (this.opacity < 1 || this.opacity === 0)) {
      this.opacity += NightMode.config.FADE_SPEED;
    } else if (this.opacity > 0) {
      this.opacity -= NightMode.config.FADE_SPEED;
    }

    // Set moon positioning.
    if (this.opacity > 0) {
      this.xPos = this.updateXPos(this.xPos, NightMode.config.MOON_SPEED);

      // Update stars.
      if (this.drawStars) {
        for (let i = 0; i < NightMode.config.NUM_STARS; i++) {
          this.stars[i].x =
              this.updateXPos(this.stars[i].x, NightMode.config.STAR_SPEED);
        }
      }
      this.draw();
    } else {
      this.opacity = 0;
      this.placeStars();
    }
    this.drawStars = true;
  }

  updateXPos(currentPos, speed) {
    if (currentPos < -NightMode.config.WIDTH) {
      currentPos = this.containerWidth;
    } else {
      currentPos -= speed;
    }
    return currentPos;
  }

  draw() {
    let moonSourceWidth = this.currentPhase === 3 ? NightMode.config.WIDTH * 2 :
                                                    NightMode.config.WIDTH;
    let moonSourceHeight = NightMode.config.HEIGHT;
    let moonSourceX = this.spritePos.x + NightMode.phases[this.currentPhase];
    const moonOutputWidth = moonSourceWidth;
    let starSize = NightMode.config.STAR_SIZE;
    let starSourceX = spriteDefinitionByType.original.LDPI.STAR.x;

    if (IS_HIDPI) {
      moonSourceWidth *= 2;
      moonSourceHeight *= 2;
      moonSourceX =
          this.spritePos.x + (NightMode.phases[this.currentPhase] * 2);
      starSize *= 2;
      starSourceX = spriteDefinitionByType.original.HDPI.STAR.x;
    }

    this.canvasCtx.save();
    this.canvasCtx.globalAlpha = this.opacity;

    // Stars.
    if (this.drawStars) {
      for (let i = 0; i < NightMode.config.NUM_STARS; i++) {
        this.canvasCtx.drawImage(
            Runner.origImageSprite, starSourceX, this.stars[i].sourceY,
            starSize, starSize, Math.round(this.stars[i].x), this.stars[i].y,
            NightMode.config.STAR_SIZE, NightMode.config.STAR_SIZE);
      }
    }

    // Moon.
    this.canvasCtx.drawImage(
        Runner.origImageSprite, moonSourceX, this.spritePos.y, moonSourceWidth,
        moonSourceHeight, Math.round(this.xPos), this.yPos, moonOutputWidth,
        NightMode.config.HEIGHT);

    this.canvasCtx.globalAlpha = 1;
    this.canvasCtx.restore();
  }

  // Do star placement.
  placeStars() {
    const segmentSize =
        Math.round(this.containerWidth / NightMode.config.NUM_STARS);

    for (let i = 0; i < NightMode.config.NUM_STARS; i++) {
      this.stars[i] = {};
      this.stars[i].x = getRandomNum(segmentSize * i, segmentSize * (i + 1));
      this.stars[i].y = getRandomNum(0, NightMode.config.STAR_MAX_Y);

      if (IS_HIDPI) {
        this.stars[i].sourceY = spriteDefinitionByType.original.HDPI.STAR.y +
            NightMode.config.STAR_SIZE * 2 * i;
      } else {
        this.stars[i].sourceY = spriteDefinitionByType.original.LDPI.STAR.y +
            NightMode.config.STAR_SIZE * i;
      }
    }
  }

  reset() {
    this.currentPhase = 0;
    this.opacity = 0;
    this.update(false);
  }
}

/**
 * @enum {number}
 */
NightMode.config = {
  FADE_SPEED: 0.035,
  HEIGHT: 40,
  MOON_SPEED: 0.25,
  NUM_STARS: 2,
  STAR_SIZE: 9,
  STAR_SPEED: 0.3,
  STAR_MAX_Y: 70,
  WIDTH: 20,
};

NightMode.phases = [140, 120, 100, 60, 40, 20, 0];