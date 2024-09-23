// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Accelerator, AcceleratorKeyState, AcceleratorState, AcceleratorType, StandardAcceleratorInfo, TextAcceleratorInfo, TextAcceleratorPart} from 'chrome://shortcut-customization/js/shortcut_types.js';


export function createStandardAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
          keyState: AcceleratorKeyState.PRESSED,
        },
      },
    },
    acceleratorLocked: false,
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kDefault,
  };
}

export function createTextAcceleratorInfo(
    parts: TextAcceleratorPart[], locked = false): TextAcceleratorInfo {
  return {
    layoutProperties: {
      textAccelerator: {
        parts,
      },
    },
    locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kDefault,
  };
}

export function createUserAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    locked = false): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
          keyState: AcceleratorKeyState.PRESSED,
        },
      },
    },
    acceleratorLocked: false,
    locked: locked,
    state: AcceleratorState.kEnabled,
    type: AcceleratorType.kUser,
  };
}

export function createCustomStandardAcceleratorInfo(
    modifier: number, keycode: number, keyDisplay: string,
    state: AcceleratorState, locked = false): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keycode,
          keyState: AcceleratorKeyState.PRESSED,
        },
      },
    },
    acceleratorLocked: false,
    locked: locked,
    state: state,
    type: AcceleratorType.kUser,
  };
}

export function createAliasedStandardAcceleratorInfo(
    modifier: number, keyCode: number, keyDisplay: string,
    state: AcceleratorState,
    originalAccelerator: Accelerator): StandardAcceleratorInfo {
  return {
    layoutProperties: {
      standardAccelerator: {
        keyDisplay: keyDisplay,
        accelerator: {
          modifiers: modifier,
          keyCode: keyCode,
          keyState: AcceleratorKeyState.PRESSED,
        },
        originalAccelerator: originalAccelerator,
      },
    },
    acceleratorLocked: false,
    locked: false,
    state: state,
    type: AcceleratorType.kUser,
  };
}
