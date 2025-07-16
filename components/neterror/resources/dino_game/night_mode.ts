// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {IS_HIDPI} from './constants.js';
import {Runner} from './offline.js';
import {spriteDefinitionByType} from './offline_sprite_definitions.js';
import type {SpritePosition} from './sprite_position.js';
import {getRandomNum} from './utils.js';


const PHASES: number[] = [140, 120, 100, 60, 40, 20, 0];

interface StarPosition {
  x: number;
  y: number;
  sourceY: number;
}

enum Config {
  FADE_SPEED = 0.035,
  HEIGHT = 40,
  MOON_SPEED = 0.25,
  NUM_STARS = 2,
  STAR_SIZE = 9,
  STAR_SPEED = 0.3,
  STAR_MAX_Y = 70,
  WIDTH = 20,
}

export class NightMode {
  private spritePos: SpritePosition;
  private canvasCtx: CanvasRenderingContext2D;
  private xPos: number = 0;
  private yPos: number = 30;
  private currentPhase: number = 0;
  private opacity: number = 0;
  private containerWidth: number;
  private stars: StarPosition[] = new Array(Config.NUM_STARS);
  private drawStars: boolean = false;

  /**
   * Nightmode shows a moon and stars on the horizon.
   */
  constructor(
      canvas: HTMLCanvasElement, spritePos: SpritePosition,
      containerWidth: number) {
    this.spritePos = spritePos;
    const canvasContext = canvas.getContext('2d');
    assert(canvasContext);
    this.canvasCtx = canvasContext;
    this.containerWidth = containerWidth;
    this.placeStars();
  }


  /**
   * Update moving moon, changing phases.
   */
  update(activated: boolean) {
    // Moon phase.
    if (activated && this.opacity === 0) {
      this.currentPhase++;

      if (this.currentPhase >= PHASES.length) {
        this.currentPhase = 0;
      }
    }

    // Fade in / out.
    if (activated && (this.opacity < 1 || this.opacity === 0)) {
      this.opacity += Config.FADE_SPEED;
    } else if (this.opacity > 0) {
      this.opacity -= Config.FADE_SPEED;
    }

    // Set moon positioning.
    if (this.opacity > 0) {
      this.xPos = this.updateXpos(this.xPos, Config.MOON_SPEED);

      // Update stars.
      if (this.drawStars) {
        for (let i = 0; i < Config.NUM_STARS; i++) {
          const star = this.stars[i];
          assert(star);
          star.x = this.updateXpos(star.x, Config.STAR_SPEED);
        }
      }
      this.draw();
    } else {
      this.opacity = 0;
      this.placeStars();
    }
    this.drawStars = true;
  }

  private updateXpos(currentPos: number, speed: number) {
    if (currentPos < -Config.WIDTH) {
      currentPos = this.containerWidth;
    } else {
      currentPos -= speed;
    }
    return currentPos;
  }

  private draw() {
    let moonSourceWidth =
        this.currentPhase === 3 ? Config.WIDTH * 2 : Config.WIDTH;
    let moonSourceHeight = Config.HEIGHT;
    const currentPhaseSpritePosition = PHASES[this.currentPhase];
    assert(currentPhaseSpritePosition !== undefined);
    let moonSourceX = this.spritePos.x + currentPhaseSpritePosition;
    const moonOutputWidth = moonSourceWidth;
    let starSize = Config.STAR_SIZE;
    let starSourceX = spriteDefinitionByType.original.ldpi.star.x;
    const runnerOrigImageSprite = Runner.getInstance().getOrigImageSprite();
    assert(runnerOrigImageSprite);

    if (IS_HIDPI) {
      moonSourceWidth *= 2;
      moonSourceHeight *= 2;
      moonSourceX = this.spritePos.x + (currentPhaseSpritePosition * 2);
      starSize *= 2;
      starSourceX = spriteDefinitionByType.original.hdpi.star.x;
    }

    this.canvasCtx.save();
    this.canvasCtx.globalAlpha = this.opacity;

    // Stars.
    if (this.drawStars) {
      for (const star of this.stars) {
        this.canvasCtx.drawImage(
            runnerOrigImageSprite, starSourceX, star.sourceY, starSize,
            starSize, Math.round(star.x), star.y, Config.STAR_SIZE,
            Config.STAR_SIZE);
      }
    }

    // Moon.
    this.canvasCtx.drawImage(
        runnerOrigImageSprite, moonSourceX, this.spritePos.y, moonSourceWidth,
        moonSourceHeight, Math.round(this.xPos), this.yPos, moonOutputWidth,
        Config.HEIGHT);

    this.canvasCtx.globalAlpha = 1;
    this.canvasCtx.restore();
  }

  // Do star placement.
  private placeStars() {
    const segmentSize = Math.round(this.containerWidth / Config.NUM_STARS);

    for (let i = 0; i < Config.NUM_STARS; i++) {
      const starPosition: StarPosition = {
        x: getRandomNum(segmentSize * i, segmentSize * (i + 1)),
        y: getRandomNum(0, Config.STAR_MAX_Y),
        sourceY: 0,
      };

      if (IS_HIDPI) {
        starPosition.sourceY = spriteDefinitionByType.original.hdpi.star.y +
            Config.STAR_SIZE * 2 * i;
      } else {
        starPosition.sourceY =
            spriteDefinitionByType.original.ldpi.star.y + Config.STAR_SIZE * i;
      }

      this.stars[i] = starPosition;
    }
  }

  reset() {
    this.currentPhase = 0;
    this.opacity = 0;
    this.update(false);
  }
}
