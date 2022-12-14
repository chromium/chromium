// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AcceleratorState, AcceleratorType, DefaultAcceleratorInfo} from 'chrome://shortcut-customization/js/shortcut_types.js';


export function createDefaultAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): DefaultAcceleratorInfo {
  return {
    layoutProperties: {
      defaultAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
        },
      },
    },
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kDefault,
  };
}

export function createUserAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): DefaultAcceleratorInfo {
  return {
    layoutProperties: {
      defaultAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
        },
      },
    },
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kUser,
  };
}
