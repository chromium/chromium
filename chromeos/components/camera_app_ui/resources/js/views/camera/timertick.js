// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as dom from '../../dom.js';
import {play} from '../../sound.js';
import * as state from '../../state.js';
import * as util from '../../util.js';

/**
 * Handler to cancel the active running timer-ticks.
 * @type {?function()}
 */
let doCancel = null;

/**
 * Starts timer ticking if applicable.
 * @return {!Promise} Promise for the operation.
 */
export function start() {
  doCancel = null;
  if (!state.get(state.State.TIMER)) {
    return Promise.resolve();
  }
  return new Promise((resolve, reject) => {
    let tickTimeout = null;
    const tickMsg = dom.get('#timer-tick-msg', HTMLElement);
    doCancel = () => {
      if (tickTimeout) {
        clearTimeout(tickTimeout);
        tickTimeout = null;
      }
      util.animateCancel(tickMsg);
      reject(new Error('cancel'));
    };

    let tickCounter = state.get(state.State.TIMER_10SEC) ? 10 : 3;
    const sounds = {
      1: '#sound-tick-final',
      2: '#sound-tick-inc',
      3: '#sound-tick-inc',
      [tickCounter]: '#sound-tick-start',
    };
    const onTimerTick = () => {
      if (tickCounter === 0) {
        resolve();
      } else {
        if (sounds[tickCounter] !== undefined) {
          play(sounds[tickCounter]);
        }
        tickMsg.textContent = tickCounter + '';
        util.animateOnce(tickMsg);
        tickTimeout = setTimeout(onTimerTick, 1000);
        tickCounter--;
      }
    };
    // First tick immediately in the next message loop cycle.
    tickTimeout = setTimeout(onTimerTick, 0);
  });
}

/**
 * Cancels active timer ticking if applicable.
 */
export function cancel() {
  if (doCancel) {
    doCancel();
    doCancel = null;
  }
}
