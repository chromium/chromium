// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorInfo, AcceleratorKeys, AcceleratorState, AcceleratorType} from 'chrome://shortcut-customization/shortcut_types.js';

/**
 * @param {number} modifier
 * @param {number} keycode
 * @param {string} key_display
 * @param {?boolean} locked
 * @return {!AcceleratorInfo}
 */
export function CreateDefaultAccelerator(
    modifier, keycode, key_display, locked = false) {
  return /** @type {!AcceleratorInfo} */ ({
    accelerator: /** @type {!AcceleratorKeys} */ ({
      modifiers: modifier,
      key: keycode,
      key_display: key_display,
    }),
    type: AcceleratorType.kDefault,
    state: AcceleratorState.kEnabled,
    locked: locked,
  });
}

/**
 * @param {number} modifier
 * @param {number} keycode
 * @param {string} key_display
 * @param {?boolean} locked
 * @return {!AcceleratorInfo}
 */
export function CreateUserAccelerator(
    modifier, keycode, key_display, locked = false) {
  return /** @type {!AcceleratorInfo} */ ({
    accelerator: /** @type {!AcceleratorKeys} */ ({
      modifiers: modifier,
      key: keycode,
      key_display: key_display,
    }),
    type: AcceleratorType.kUserDefined,
    state: AcceleratorState.kEnabled,
    locked: locked,
  });
}
