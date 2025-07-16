// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {IS_HIDPI} from './constants.js';
import {Runner} from './offline.js';
import type {SpritePosition} from './sprite_position.js';
import {getRandomNum} from './utils.js';

export interface BackgroundElSpriteConfig {
  height: number;
  width: number;
  offset: number;
  xPos: number;
  fixed: boolean;
  // Only present when fixed is true.
  fixedXPos?: number;
  // Only present when fixed is true.
  fixedYPos1?: number;
  // Only present when fixed is true.
  fixedYPos2?: number;
}

export interface BackgroundElConfig {
  maxBgEls: number;
  maxGap: number;
  minGap: number;
  pos: number;
  speed: number;
  yPos: number;
  msPerFrame?: number;  // only needed when spriteConfig.fixed is true
}

/**
 * Background element object config.
 * Real values assigned when game type changes.
 */
let globalConfig: BackgroundElConfig = {
  maxBgEls: 0,
  maxGap: 0,
  minGap: 0,
  msPerFrame: 0,
  pos: 0,
  speed: 0,
  yPos: 0,
};

export function getGlobalConfig(): BackgroundElConfig {
  return globalConfig;
}

export function setGlobalConfig(config: BackgroundElConfig) {
  globalConfig = config;
}

export class BackgroundEl {
  gap: number;
  xPos: number;
  remove: boolean = false;

  private canvas: HTMLCanvasElement;
  private canvasCtx: CanvasRenderingContext2D;
  private spritePos: SpritePosition;
  private yPos: number = 0;
  private type: string;
  private animTimer: number = 0;
  private spriteConfig: BackgroundElSpriteConfig;
  private switchFrames: boolean = false;


  /**
   * Background item.
   * Similar to cloud, without random y position.
   */
  constructor(
      canvas: HTMLCanvasElement, spritePos: SpritePosition,
      containerWidth: number, type: string) {
    this.canvas = canvas;
    const canvasContext = this.canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.spritePos = spritePos;
    this.xPos = containerWidth;
    this.type = type;
    this.gap = getRandomNum(getGlobalConfig().minGap, getGlobalConfig().maxGap);

    const spriteConfig =
        Runner.getInstance().getSpriteDefinition().backgroundEl[this.type];
    assert(spriteConfig);
    this.spriteConfig = spriteConfig;
    this.init();
  }


  /**
   * Initialise the element setting the y position.
   */
  init() {
    if (this.spriteConfig.fixed) {
      assert(this.spriteConfig.fixedXPos);
      this.xPos = this.spriteConfig.fixedXPos;
    }
    this.yPos = getGlobalConfig().yPos - this.spriteConfig.height +
        this.spriteConfig.offset;
    this.draw();
  }

  /**
   * Draw the element.
   */
  draw() {
    this.canvasCtx.save();
    let sourceWidth = this.spriteConfig.width;
    let sourceHeight = this.spriteConfig.height;
    let sourceX = this.spriteConfig.xPos;
    const outputWidth = sourceWidth;
    const outputHeight = sourceHeight;
    const imageSprite = Runner.getInstance().getRunnerImageSprite();
    assert(imageSprite);

    if (IS_HIDPI) {
      sourceWidth *= 2;
      sourceHeight *= 2;
      sourceX *= 2;
    }

    this.canvasCtx.drawImage(
        imageSprite, sourceX, this.spritePos.y, sourceWidth, sourceHeight,
        this.xPos, this.yPos, outputWidth, outputHeight);

    this.canvasCtx.restore();
  }

  /**
   * Update the background element position.
   */
  update(speed: number) {
    if (!this.remove) {
      if (this.spriteConfig.fixed) {
        const globalConfig = getGlobalConfig();
        assert(globalConfig.msPerFrame);
        this.animTimer += speed;
        if (this.animTimer > globalConfig.msPerFrame) {
          this.animTimer = 0;
          this.switchFrames = !this.switchFrames;
        }

        if (this.spriteConfig.fixedYPos1 && this.spriteConfig.fixedYPos2) {
          this.yPos = this.switchFrames ? this.spriteConfig.fixedYPos1 :
                                          this.spriteConfig.fixedYPos2;
        }
      } else {
        // Fixed speed, regardless of actual game speed.
        this.xPos -= getGlobalConfig().speed;
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
   */
  isVisible(): boolean {
    return this.xPos + this.spriteConfig.width > 0;
  }
}
