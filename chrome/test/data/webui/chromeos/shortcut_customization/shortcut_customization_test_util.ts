// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorInfo, AcceleratorState, AcceleratorType} from 'chrome://shortcut-customization/js/shortcut_types.js';

export function createDefaultAccelerator(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): AcceleratorInfo {
  return {
    accelerator: {
      modifiers: modifier,
      key: keycode,
      keyDisplay: keyDisplay,
    },
    type: AcceleratorType.DEFAULT,
    state: AcceleratorState.ENABLED,
    locked: locked,
  };
}

export function createUserAccelerator(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): AcceleratorInfo {
  return {
    accelerator: {
      modifiers: modifier,
      key: keycode,
      keyDisplay: keyDisplay,
    },
    type: AcceleratorType.USER_DEFINED,
    state: AcceleratorState.ENABLED,
    locked: locked,
  };
}
