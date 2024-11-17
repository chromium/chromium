// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {IS_IOS} from './constants.js';

/**
 * Get random number.
 * @param {number} min
 * @param {number} max
 */
export function getRandomNum(min, max) {
  return Math.floor(Math.random() * (max - min + 1)) + min;
}

/**
 * Return the current timestamp.
 * @return {number}
 */
export function getTimeStamp() {
  return IS_IOS ? new Date().getTime() : performance.now();
}
