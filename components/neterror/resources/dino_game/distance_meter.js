// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IS_HIDPI, IS_RTL} from './constants.js';
import {Runner} from './offline.js';
import {getTimeStamp} from './utils.js';

export class DistanceMeter {
  /**
   * Handles displaying the distance meter.
   * @param {!HTMLCanvasElement} canvas
   * @param {Object} spritePos Image position in sprite.
   * @param {number} canvasWidth
   */
  constructor(canvas, spritePos, canvasWidth) {
    this.canvas = canvas;
    this.canvasCtx =
        /** @type {CanvasRenderingContext2D} */ (canvas.getContext('2d'));
    this.image = Runner.imageSprite;
    this.spritePos = spritePos;
    this.x = 0;
    this.y = 5;

    this.currentDistance = 0;
    this.maxScore = 0;
    this.highScore = '0';
    this.container = null;

    this.digits = [];
    this.achievement = false;
    this.defaultString = '';
    this.flashTimer = 0;
    this.flashIterations = 0;
    this.invertTrigger = false;
    this.flashingRafId = null;
    this.highScoreBounds = {};
    this.highScoreFlashing = false;

    this.config = DistanceMeter.config;
    this.maxScoreUnits = this.config.MAX_DISTANCE_UNITS;
    this.canvasWidth = canvasWidth;
    this.init(canvasWidth);
  }


  /**
   * Initialise the distance meter to '00000'.
   * @param {number} width Canvas width in px.
   */
  init(width) {
    let maxDistanceStr = '';

    this.calcXPos(width);
    this.maxScore = this.maxScoreUnits;
    for (let i = 0; i < this.maxScoreUnits; i++) {
      this.draw(i, 0);
      this.defaultString += '0';
      maxDistanceStr += '9';
    }

    this.maxScore = parseInt(maxDistanceStr, 10);
  }

  /**
   * Calculate the xPos in the canvas.
   * @param {number} canvasWidth
   */
  calcXPos(canvasWidth) {
    this.x = canvasWidth -
        (DistanceMeter.dimensions.DEST_WIDTH * (this.maxScoreUnits + 1));
  }

  /**
   * Draw a digit to canvas.
   * @param {number} digitPos Position of the digit.
   * @param {number} value Digit value 0-9.
   * @param {boolean=} opt_highScore Whether drawing the high score.
   */
  draw(digitPos, value, opt_highScore) {
    let sourceWidth = DistanceMeter.dimensions.WIDTH;
    let sourceHeight = DistanceMeter.dimensions.HEIGHT;
    let sourceX = DistanceMeter.dimensions.WIDTH * value;
    let sourceY = 0;

    const targetX = digitPos * DistanceMeter.dimensions.DEST_WIDTH;
    const targetY = this.y;
    const targetWidth = DistanceMeter.dimensions.WIDTH;
    const targetHeight = DistanceMeter.dimensions.HEIGHT;

    // For high DPI we 2x source values.
    if (IS_HIDPI) {
      sourceWidth *= 2;
      sourceHeight *= 2;
      sourceX *= 2;
    }

    sourceX += this.spritePos.x;
    sourceY += this.spritePos.y;

    this.canvasCtx.save();

    if (IS_RTL) {
      if (opt_highScore) {
        this.canvasCtx.translate(
            this.canvasWidth -
                (DistanceMeter.dimensions.WIDTH * (this.maxScoreUnits + 3)),
            this.y);
      } else {
        this.canvasCtx.translate(
            this.canvasWidth - DistanceMeter.dimensions.WIDTH, this.y);
      }
      this.canvasCtx.scale(-1, 1);
    } else {
      const highScoreX =
          this.x - (this.maxScoreUnits * 2) * DistanceMeter.dimensions.WIDTH;
      if (opt_highScore) {
        this.canvasCtx.translate(highScoreX, this.y);
      } else {
        this.canvasCtx.translate(this.x, this.y);
      }
    }

    this.canvasCtx.drawImage(
        this.image,
        sourceX,
        sourceY,
        sourceWidth,
        sourceHeight,
        targetX,
        targetY,
        targetWidth,
        targetHeight,
    );

    this.canvasCtx.restore();
  }

  /**
   * Covert pixel distance to a 'real' distance.
   * @param {number} distance Pixel distance ran.
   * @return {number} The 'real' distance ran.
   */
  getActualDistance(distance) {
    return distance ? Math.round(distance * this.config.COEFFICIENT) : 0;
  }

  /**
   * Update the distance meter.
   * @param {number} distance
   * @param {number} deltaTime
   * @return {boolean} Whether the achievement sound fx should be played.
   */
  update(deltaTime, distance) {
    let paint = true;
    let playSound = false;

    if (!this.achievement) {
      distance = this.getActualDistance(distance);
      // Score has gone beyond the initial digit count.
      if (distance > this.maxScore &&
          this.maxScoreUnits === this.config.MAX_DISTANCE_UNITS) {
        this.maxScoreUnits++;
        this.maxScore = parseInt(this.maxScore + '9', 10);
      } else {
        this.distance = 0;
      }

      if (distance > 0) {
        // Achievement unlocked.
        if (distance % this.config.ACHIEVEMENT_DISTANCE === 0) {
          // Flash score and play sound.
          this.achievement = true;
          this.flashTimer = 0;
          playSound = true;
        }

        // Create a string representation of the distance with leading 0.
        const distanceStr =
            (this.defaultString + distance).substr(-this.maxScoreUnits);
        this.digits = distanceStr.split('');
      } else {
        this.digits = this.defaultString.split('');
      }
    } else {
      // Control flashing of the score on reaching achievement.
      if (this.flashIterations <= this.config.FLASH_ITERATIONS) {
        this.flashTimer += deltaTime;

        if (this.flashTimer < this.config.FLASH_DURATION) {
          paint = false;
        } else if (this.flashTimer > this.config.FLASH_DURATION * 2) {
          this.flashTimer = 0;
          this.flashIterations++;
        }
      } else {
        this.achievement = false;
        this.flashIterations = 0;
        this.flashTimer = 0;
      }
    }

    // Draw the digits if not flashing.
    if (paint) {
      for (let i = this.digits.length - 1; i >= 0; i--) {
        this.draw(i, parseInt(this.digits[i], 10));
      }
    }

    this.drawHighScore();
    return playSound;
  }

  /**
   * Draw the high score.
   */
  drawHighScore() {
    if (parseInt(this.highScore, 10) > 0) {
      this.canvasCtx.save();
      this.canvasCtx.globalAlpha = .8;
      for (let i = this.highScore.length - 1; i >= 0; i--) {
        this.draw(i, parseInt(this.highScore[i], 10), true);
      }
      this.canvasCtx.restore();
    }
  }

  /**
   * Set the highscore as a array string.
   * Position of char in the sprite: H - 10, I - 11.
   * @param {number} distance Distance ran in pixels.
   */
  setHighScore(distance) {
    distance = this.getActualDistance(distance);
    const highScoreStr =
        (this.defaultString + distance).substr(-this.maxScoreUnits);

    this.highScore = ['10', '11', ''].concat(highScoreStr.split(''));
  }


  /**
   * Whether a clicked is in the high score area.
   * @param {Event} e Event object.
   * @return {boolean} Whether the click was in the high score bounds.
   */
  hasClickedOnHighScore(e) {
    let x = 0;
    let y = 0;

    if (e.touches) {
      // Bounds for touch differ from pointer.
      const canvasBounds = this.canvas.getBoundingClientRect();
      x = e.touches[0].clientX - canvasBounds.left;
      y = e.touches[0].clientY - canvasBounds.top;
    } else {
      x = e.offsetX;
      y = e.offsetY;
    }

    this.highScoreBounds = this.getHighScoreBounds();
    return x >= this.highScoreBounds.x &&
        x <= this.highScoreBounds.x + this.highScoreBounds.width &&
        y >= this.highScoreBounds.y &&
        y <= this.highScoreBounds.y + this.highScoreBounds.height;
  }

  /**
   * Get the bounding box for the high score.
   * @return {Object} Object with x, y, width and height properties.
   */
  getHighScoreBounds() {
    return {
      x: (this.x - (this.maxScoreUnits * 2) * DistanceMeter.dimensions.WIDTH) -
          DistanceMeter.config.HIGH_SCORE_HIT_AREA_PADDING,
      y: this.y,
      width: DistanceMeter.dimensions.WIDTH * (this.highScore.length + 1) +
          DistanceMeter.config.HIGH_SCORE_HIT_AREA_PADDING,
      height: DistanceMeter.dimensions.HEIGHT +
          (DistanceMeter.config.HIGH_SCORE_HIT_AREA_PADDING * 2),
    };
  }

  /**
   * Animate flashing the high score to indicate ready for resetting.
   * The flashing stops following this.config.FLASH_ITERATIONS x 2 flashes.
   */
  flashHighScore() {
    const now = getTimeStamp();
    const deltaTime = now - (this.frameTimeStamp || now);
    let paint = true;
    this.frameTimeStamp = now;

    // Reached the max number of flashes.
    if (this.flashIterations > this.config.FLASH_ITERATIONS * 2) {
      this.cancelHighScoreFlashing();
      return;
    }

    this.flashTimer += deltaTime;

    if (this.flashTimer < this.config.FLASH_DURATION) {
      paint = false;
    } else if (this.flashTimer > this.config.FLASH_DURATION * 2) {
      this.flashTimer = 0;
      this.flashIterations++;
    }

    if (paint) {
      this.drawHighScore();
    } else {
      this.clearHighScoreBounds();
    }
    // Frame update.
    this.flashingRafId = requestAnimationFrame(this.flashHighScore.bind(this));
  }

  /**
   * Draw empty rectangle over high score.
   */
  clearHighScoreBounds() {
    this.canvasCtx.save();
    this.canvasCtx.fillStyle = '#fff';
    this.canvasCtx.rect(
        this.highScoreBounds.x, this.highScoreBounds.y,
        this.highScoreBounds.width, this.highScoreBounds.height);
    this.canvasCtx.fill();
    this.canvasCtx.restore();
  }

  /**
   * Starts the flashing of the high score.
   */
  startHighScoreFlashing() {
    this.highScoreFlashing = true;
    this.flashHighScore();
  }

  /**
   * Whether high score is flashing.
   * @return {boolean}
   */
  isHighScoreFlashing() {
    return this.highScoreFlashing;
  }

  /**
   * Stop flashing the high score.
   */
  cancelHighScoreFlashing() {
    if (this.flashingRafId) {
      cancelAnimationFrame(this.flashingRafId);
    }
    this.flashIterations = 0;
    this.flashTimer = 0;
    this.highScoreFlashing = false;
    this.clearHighScoreBounds();
    this.drawHighScore();
  }

  /**
   * Clear the high score.
   */
  resetHighScore() {
    this.setHighScore(0);
    this.cancelHighScoreFlashing();
  }

  /**
   * Reset the distance meter back to '00000'.
   */
  reset() {
    this.update(0, 0);
    this.achievement = false;
  }
}

/**
 * @enum {number}
 */
DistanceMeter.dimensions = {
  WIDTH: 10,
  HEIGHT: 13,
  DEST_WIDTH: 11,
};


/**
 * Y positioning of the digits in the sprite sheet.
 * X position is always 0.
 * @type {Array<number>}
 */
DistanceMeter.yPos = [0, 13, 27, 40, 53, 67, 80, 93, 107, 120];


/**
 * Distance meter config.
 * @enum {number}
 */
DistanceMeter.config = {
  // Number of digits.
  MAX_DISTANCE_UNITS: 5,

  // Distance that causes achievement animation.
  ACHIEVEMENT_DISTANCE: 100,

  // Used for conversion from pixel distance to a scaled unit.
  COEFFICIENT: 0.025,

  // Flash duration in milliseconds.
  FLASH_DURATION: 1000 / 4,

  // Flash iterations for achievement animation.
  FLASH_ITERATIONS: 3,

  // Padding around the high score hit area.
  HIGH_SCORE_HIT_AREA_PADDING: 4,
};