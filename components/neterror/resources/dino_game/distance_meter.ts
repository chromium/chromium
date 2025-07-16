// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {IS_HIDPI, IS_RTL} from './constants.js';
import {Runner} from './offline.js';
import type {CollisionBox} from './offline_sprite_definitions.js';
import type {SpritePosition} from './sprite_position.js';
import {getTimeStamp} from './utils.js';


/**
 * Dimensions of each individual character in pixels.
 */
enum Dimensions {
  WIDTH = 10,
  HEIGHT = 13,
  DEST_WIDTH = 11,
}

/**
 * Distance meter config.
 */
enum Config {
  // Number of digits.
  MAX_DISTANCE_UNITS = 5,

  // Distance that causes achievement animation.
  ACHIEVEMENT_DISTANCE = 100,

  // Used for conversion from pixel distance to a scaled unit.
  COEFFICIENT = 0.025,

  // Flash duration in milliseconds.
  FLASH_DURATION = 1000 / 4,

  // Flash iterations for achievement animation.
  FLASH_ITERATIONS = 3,

  // Padding around the high score hit area.
  HIGH_SCORE_HIT_AREA_PADDING = 4,
}

export class DistanceMeter {
  achievement: boolean = false;

  private canvas: HTMLCanvasElement;
  private canvasCtx: CanvasRenderingContext2D;
  private image: CanvasImageSource;
  private spritePos: SpritePosition;
  private x: number = 0;
  private y: number = 5;
  private maxScore: number = 0;
  private highScore: string = '0';
  private digits: string[] = [];
  private defaultString: string = '';
  private flashTimer: number = 0;
  private flashIterations: number = 0;
  private flashingRafId: number|null = null;
  private highScoreBounds: CollisionBox|null = null;
  private highScoreFlashing: boolean = false;
  private maxScoreUnits: number = Config.MAX_DISTANCE_UNITS;
  private canvasWidth: number;
  private frameTimeStamp?: number;

  /**
   * Handles displaying the distance meter.
   */
  constructor(
      canvas: HTMLCanvasElement, spritePos: SpritePosition,
      canvasWidth: number) {
    this.canvas = canvas;
    const canvasContext = canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    const runnerImageSprite = Runner.getInstance().getRunnerImageSprite();
    this.image = runnerImageSprite;
    this.spritePos = spritePos;

    this.canvasWidth = canvasWidth;
    this.init(canvasWidth);
  }


  /**
   * Initialise the distance meter to '00000'.
   * @param width Canvas width in px.
   */
  private init(width: number) {
    let maxDistanceStr = '';

    this.calcXpos(width);
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
   */
  calcXpos(canvasWidth: number) {
    this.x = canvasWidth - (Dimensions.DEST_WIDTH * (this.maxScoreUnits + 1));
  }

  /**
   * Draw a digit to canvas.
   * @param digitPos Position of the digit.
   * @param value Digit value 0-9.
   * @param highScore Whether drawing the high score.
   */
  private draw(digitPos: number, value: number, highScore?: boolean) {
    let sourceWidth = Dimensions.WIDTH;
    let sourceHeight = Dimensions.HEIGHT;
    let sourceX = Dimensions.WIDTH * value;
    let sourceY = 0;

    const targetX = digitPos * Dimensions.DEST_WIDTH;
    const targetY = this.y;
    const targetWidth = Dimensions.WIDTH;
    const targetHeight = Dimensions.HEIGHT;

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
      const translateX = highScore ?
          this.canvasWidth - (Dimensions.WIDTH * (this.maxScoreUnits + 3)) :
          this.canvasWidth - Dimensions.WIDTH;
      this.canvasCtx.translate(translateX, this.y);
      this.canvasCtx.scale(-1, 1);
    } else {
      const highScoreX = this.x - (this.maxScoreUnits * 2) * Dimensions.WIDTH;
      this.canvasCtx.translate(highScore ? highScoreX : this.x, this.y);
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
   * @param distance Pixel distance ran.
   * @return The 'real' distance ran.
   */
  getActualDistance(distance: number): number {
    return distance ? Math.round(distance * Config.COEFFICIENT) : 0;
  }

  /**
   * Update the distance meter.
   * @return Whether the achievement sound fx should be played.
   */
  update(deltaTime: number, distance: number): boolean {
    let paint = true;
    let playSound = false;

    if (!this.achievement) {
      distance = this.getActualDistance(distance);
      // Score has gone beyond the initial digit count.
      if (distance > this.maxScore &&
          this.maxScoreUnits === Config.MAX_DISTANCE_UNITS) {
        this.maxScoreUnits++;
        this.maxScore = parseInt(this.maxScore + '9', 10);
      }

      if (distance > 0) {
        // Achievement unlocked.
        if (distance % Config.ACHIEVEMENT_DISTANCE === 0) {
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
      if (this.flashIterations <= Config.FLASH_ITERATIONS) {
        this.flashTimer += deltaTime;

        if (this.flashTimer < Config.FLASH_DURATION) {
          paint = false;
        } else if (this.flashTimer > Config.FLASH_DURATION * 2) {
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
        this.draw(i, parseInt(this.digits[i]!, 10));
      }
    }

    this.drawHighScore();
    return playSound;
  }

  /**
   * Draw the high score.
   */
  private drawHighScore() {
    if (this.highScore.length > 0) {
      this.canvasCtx.save();
      this.canvasCtx.globalAlpha = .8;
      for (let i = this.highScore.length - 1; i >= 0; i--) {
        const characterToDraw = this.highScore[i]!;
        // Position of characterToDraw in sprite sheet, digits 0-9 are mapped
        // directly.
        let characterSpritePosition = parseInt(characterToDraw, 10);
        // If characterToDraw is not a digit then they must be part of the label
        // "HI". The position of these characters in the sheet is: H - 10, I
        // - 11.
        if (isNaN(characterSpritePosition)) {
          switch (characterToDraw) {
            case 'H':
              characterSpritePosition = 10;
              break;
            case 'I':
              characterSpritePosition = 11;
              break;
            // Any other character is ignored.
            default:
              continue;
          }
        }
        this.draw(i, characterSpritePosition, true);
      }
      this.canvasCtx.restore();
    }
  }

  /**
   * Set the highscore as a string.
   * @param distance Distance ran in pixels.
   */
  setHighScore(distance: number) {
    distance = this.getActualDistance(distance);
    const highScoreStr =
        (this.defaultString + distance).substr(-this.maxScoreUnits);

    this.highScore = 'HI ' + highScoreStr;
  }


  /**
   * Whether a clicked is in the high score area.
   * @return Whether the click was in the high score bounds.
   */
  hasClickedOnHighScore(e: Event): boolean {
    let x = 0;
    let y = 0;

    if (e instanceof TouchEvent) {
      // Bounds for touch differ from pointer.
      const canvasBounds = this.canvas.getBoundingClientRect();
      x = e.touches[0]!.clientX - canvasBounds.left;
      y = e.touches[0]!.clientY - canvasBounds.top;
    } else if (e instanceof MouseEvent) {
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
   */
  private getHighScoreBounds(): CollisionBox {
    return {
      x: (this.x - (this.maxScoreUnits * 2) * Dimensions.WIDTH) -
          Config.HIGH_SCORE_HIT_AREA_PADDING,
      y: this.y,
      width: Dimensions.WIDTH * (this.highScore.length + 1) +
          Config.HIGH_SCORE_HIT_AREA_PADDING,
      height: Dimensions.HEIGHT + (Config.HIGH_SCORE_HIT_AREA_PADDING * 2),
    };
  }

  /**
   * Animate flashing the high score to indicate ready for resetting.
   * The flashing stops following distanceMeterConfig.FLASH_ITERATIONS x 2
   * flashes.
   */
  private flashHighScore() {
    const now = getTimeStamp();
    const deltaTime = now - (this.frameTimeStamp || now);
    let paint = true;
    this.frameTimeStamp = now;

    // Reached the max number of flashes.
    if (this.flashIterations > Config.FLASH_ITERATIONS * 2) {
      this.cancelHighScoreFlashing();
      return;
    }

    this.flashTimer += deltaTime;

    if (this.flashTimer < Config.FLASH_DURATION) {
      paint = false;
    } else if (this.flashTimer > Config.FLASH_DURATION * 2) {
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
  private clearHighScoreBounds() {
    assert(this.highScoreBounds);
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
   */
  isHighScoreFlashing(): boolean {
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
