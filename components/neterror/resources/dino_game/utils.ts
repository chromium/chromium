// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IS_IOS} from './constants.js';
import {Runner} from './offline.js';


export function getRandomNum(min: number, max: number): number {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

/**
 * Return the current timestamp.
 */
export function getTimeStamp(): number {
  return IS_IOS ? new Date().getTime() : performance.now();
}


/**
 * Gets the static 'imageSprite' property from the Runner class. To be used
 * only during typescript migration.
 * TODO(373951324): Remove once Runner is migrated to ts.
 */
export function getRunnerImageSprite(): CanvasImageSource|null {
  if ('imageSprite' in Runner) {
    return Runner.imageSprite as CanvasImageSource;
  }
  return null;
}

export function getRunnerAltGameImageSprite(): CanvasImageSource|null {
  if ('altGameImageSprite' in Runner) {
    return Runner.altGameImageSprite as CanvasImageSource;
  }
  return null;
}

export function getRunnerAltCommonImageSprite(): CanvasImageSource|null {
  if ('altCommonImageSprite' in Runner) {
    return Runner.altCommonImageSprite as CanvasImageSource;
  }
  return null;
}

export function getRunnerOrigImageSprite(): CanvasImageSource|null {
  if ('origImageSprite' in Runner) {
    return Runner.origImageSprite as CanvasImageSource;
  }
  return null;
}

export function getRunnerSlowdown(): boolean|null {
  if ('slowDown' in Runner && typeof Runner.slowDown === 'boolean') {
    return Runner.slowDown;
  }
  return null;
}

export function getRunnerAudioCues(): boolean|null {
  if ('audioCues' in Runner && typeof Runner.audioCues === 'boolean') {
    return Runner.audioCues;
  }
  return null;
}
