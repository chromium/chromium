// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {IS_HIDPI, IS_RTL} from './constants.js';
import type {Dimensions} from './dimensions.js';
import {Runner} from './offline.js';
import {spriteDefinitionByType} from './offline_sprite_definitions.js';
import type {SpritePosition} from './sprite_position.js';
import type {Trex} from './trex.js';
import {getTimeStamp} from './utils.js';

const RESTART_ANIM_DURATION: number = 875;
const LOGO_PAUSE_DURATION: number = 875;
const FLASH_ITERATIONS: number = 5;

/**
 * Animation frames spec.
 */
const animConfig: {
  frames: number[],
  msPerFrame: number,
} = {
  frames: [0, 36, 72, 108, 144, 180, 216, 252],
  msPerFrame: RESTART_ANIM_DURATION / 8,
};

interface BaseGameOverPanelDimensions {
  textX: number;
  textY: number;
  textWidth: number;
  textHeight: number;
}

interface GameOverPanelDimensions extends BaseGameOverPanelDimensions {
  restartWidth: number;
  restartHeight: number;
}

export interface AltGameModePanelDimensions extends
    BaseGameOverPanelDimensions {
  flashing: boolean;
  flashDuration: number;
}

export interface AltGameEndConfig {
  width: number;
  height: number;
  xOffset: number;
  yOffset: number;
}

/**
 * Dimensions used in the panel.
 */
const defaultPanelDimensions: GameOverPanelDimensions = {
  textX: 0,
  textY: 13,
  textWidth: 191,
  textHeight: 11,
  restartWidth: 36,
  restartHeight: 32,
};

export class GameOverPanel {
  private canvasCtx: CanvasRenderingContext2D;
  private canvasDimensions: Dimensions;
  private textImgPos: SpritePosition;
  private restartImgPos: SpritePosition;
  private altGameEndImgPos: SpritePosition|null;
  private altGameModeActive: boolean;
  private frameTimeStamp: number = 0;
  private animTimer: number = 0;
  private currentFrame: number = 0;
  private gameOverRafId: number|null = null;
  private flashTimer: number = 0;
  private flashCounter: number = 0;
  private originalText: boolean = true;

  /**
   * Game over panel.
   */
  constructor(
      canvas: HTMLCanvasElement, textImgPos: SpritePosition,
      restartImgPos: SpritePosition, dimensions: Dimensions,
      altGameEndImgPos?: SpritePosition, altGameActive?: boolean) {
    const canvasContext = canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.canvasDimensions = dimensions;
    this.textImgPos = textImgPos;
    this.restartImgPos = restartImgPos;
    this.altGameEndImgPos = altGameEndImgPos ?? null;
    this.altGameModeActive = altGameActive ?? false;
  }


  /**
   * Update the panel dimensions.
   * @param width New canvas width.
   * @param height Optional new canvas height.
   */
  updateDimensions(width: number, height?: number) {
    this.canvasDimensions.width = width;
    if (height) {
      this.canvasDimensions.height = height;
    }
    this.currentFrame = animConfig.frames.length - 1;
  }

  private drawGameOverText(
      dimensions: BaseGameOverPanelDimensions, useAltText?: boolean) {
    const centerX = this.canvasDimensions.width / 2;
    let textSourceX = dimensions.textX;
    let textSourceY = dimensions.textY;
    let textSourceWidth = dimensions.textWidth;
    let textSourceHeight = dimensions.textHeight;

    const textTargetX = Math.round(centerX - (dimensions.textWidth / 2));
    const textTargetY = Math.round((this.canvasDimensions.height - 25) / 3);
    const textTargetWidth = dimensions.textWidth;
    const textTargetHeight = dimensions.textHeight;

    if (IS_HIDPI) {
      textSourceY *= 2;
      textSourceX *= 2;
      textSourceWidth *= 2;
      textSourceHeight *= 2;
    }

    if (!useAltText) {
      textSourceX += this.textImgPos.x;
      textSourceY += this.textImgPos.y;
    }

    const runner = Runner.getInstance();
    const spriteSource = useAltText ? runner.getAltCommonImageSprite() :
                                      runner.getOrigImageSprite();
    assert(spriteSource);

    this.canvasCtx.save();

    if (IS_RTL) {
      this.canvasCtx.translate(this.canvasDimensions.width, 0);
      this.canvasCtx.scale(-1, 1);
    }

    // Game over text from sprite.
    this.canvasCtx.drawImage(
        spriteSource, textSourceX, textSourceY, textSourceWidth,
        textSourceHeight, textTargetX, textTargetY, textTargetWidth,
        textTargetHeight);

    this.canvasCtx.restore();
  }

  /**
   * Draw additional adornments for alternative game types.
   */
  private drawAltGameElements(tRex: Trex) {
    const spriteDefinition = Runner.getInstance().getSpriteDefinition();
    // Additional adornments.
    if (this.altGameModeActive && spriteDefinition) {
      assert(this.altGameEndImgPos);
      const altGameEndConfig = spriteDefinition.altGameEndConfig;
      assert(altGameEndConfig);

      let altGameEndSourceWidth = altGameEndConfig.width;
      let altGameEndSourceHeight = altGameEndConfig.height;
      const altGameEndTargetX = tRex.xPos + altGameEndConfig.xOffset;
      const altGameEndTargetY = tRex.yPos + altGameEndConfig.yOffset;

      if (IS_HIDPI) {
        altGameEndSourceWidth *= 2;
        altGameEndSourceHeight *= 2;
      }

      const altCommonImageSprite =
          Runner.getInstance().getAltCommonImageSprite();
      assert(altCommonImageSprite);


      this.canvasCtx.drawImage(
          altCommonImageSprite, this.altGameEndImgPos.x,
          this.altGameEndImgPos.y, altGameEndSourceWidth,
          altGameEndSourceHeight, altGameEndTargetX, altGameEndTargetY,
          altGameEndConfig.width, altGameEndConfig.height);
    }
  }

  /**
   * Draw restart button.
   */
  private drawRestartButton() {
    const dimensions = defaultPanelDimensions;
    let framePosX = animConfig.frames[this.currentFrame]!;
    let restartSourceWidth = dimensions.restartWidth;
    let restartSourceHeight = dimensions.restartHeight;
    const restartTargetX =
        (this.canvasDimensions.width / 2) - (dimensions.restartHeight / 2);
    const restartTargetY = this.canvasDimensions.height / 2;

    if (IS_HIDPI) {
      restartSourceWidth *= 2;
      restartSourceHeight *= 2;
      framePosX *= 2;
    }

    this.canvasCtx.save();

    if (IS_RTL) {
      this.canvasCtx.translate(this.canvasDimensions.width, 0);
      this.canvasCtx.scale(-1, 1);
    }
    const origImageSprite = Runner.getInstance().getOrigImageSprite();

    this.canvasCtx.drawImage(
        origImageSprite, this.restartImgPos.x + framePosX, this.restartImgPos.y,
        restartSourceWidth, restartSourceHeight, restartTargetX, restartTargetY,
        dimensions.restartWidth, dimensions.restartHeight);
    this.canvasCtx.restore();
  }


  /**
   * Draw the panel.
   */
  draw(altGameModeActive?: boolean, tRex?: Trex) {
    if (altGameModeActive) {
      this.altGameModeActive = altGameModeActive;
    }

    this.drawGameOverText(defaultPanelDimensions, false);
    this.drawRestartButton();
    if (tRex) {
      this.drawAltGameElements(tRex);
    }
    this.update();
  }

  /**
   * Update animation frames.
   */
  private update() {
    const now = getTimeStamp();
    const deltaTime = now - (this.frameTimeStamp || now);

    this.frameTimeStamp = now;
    this.animTimer += deltaTime;
    this.flashTimer += deltaTime;

    // Restart Button
    if (this.currentFrame === 0 && this.animTimer > LOGO_PAUSE_DURATION) {
      this.animTimer = 0;
      this.currentFrame++;
      this.drawRestartButton();
    } else if (
        this.currentFrame > 0 && this.currentFrame < animConfig.frames.length) {
      if (this.animTimer >= animConfig.msPerFrame) {
        this.currentFrame++;
        this.drawRestartButton();
      }
    } else if (
        !this.altGameModeActive &&
        this.currentFrame === animConfig.frames.length) {
      this.reset();
      return;
    }

    // Game over text
    if (this.altGameModeActive &&
        spriteDefinitionByType.original.altGameOverTextConfig) {
      const altTextConfig =
          spriteDefinitionByType.original.altGameOverTextConfig;

      if (altTextConfig.flashing) {
        if (this.flashCounter < FLASH_ITERATIONS &&
            this.flashTimer > altTextConfig.flashDuration) {
          this.flashTimer = 0;
          this.originalText = !this.originalText;

          this.clearGameOverTextBounds();
          if (this.originalText) {
            this.drawGameOverText(defaultPanelDimensions, false);
            this.flashCounter++;
          } else {
            this.drawGameOverText(altTextConfig, true);
          }
        } else if (this.flashCounter >= FLASH_ITERATIONS) {
          this.reset();
          return;
        }
      } else {
        this.clearGameOverTextBounds(altTextConfig);
        this.drawGameOverText(altTextConfig, true);
      }
    }

    this.gameOverRafId = requestAnimationFrame(this.update.bind(this));
  }

  /**
   * Clear game over text.
   * @param dimensions Game over text config.
   */
  private clearGameOverTextBounds(
      dimensions: BaseGameOverPanelDimensions = defaultPanelDimensions) {
    this.canvasCtx.save();

    this.canvasCtx.clearRect(
        Math.round(
            this.canvasDimensions.width / 2 - (dimensions.textWidth / 2)),
        Math.round((this.canvasDimensions.height - 25) / 3),
        dimensions.textWidth, dimensions.textHeight + 4);
    this.canvasCtx.restore();
  }

  reset() {
    if (this.gameOverRafId) {
      cancelAnimationFrame(this.gameOverRafId);
      this.gameOverRafId = null;
    }
    this.animTimer = 0;
    this.frameTimeStamp = 0;
    this.currentFrame = 0;
    this.flashTimer = 0;
    this.flashCounter = 0;
    this.originalText = true;
  }
}
