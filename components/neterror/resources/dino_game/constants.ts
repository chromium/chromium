// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Dimensions} from './dimensions.js';

// TODO(salg): Use preprocessor to filter IOS code at build time.
export const IS_IOS: boolean = /CriOS/.test(window.navigator.userAgent);

export const IS_HIDPI: boolean = window.devicePixelRatio > 1;

export const IS_MOBILE: boolean =
    /Android/.test(window.navigator.userAgent) || IS_IOS;

export const IS_RTL: boolean = document.documentElement.dir === 'rtl';


// Frames per second.
export const FPS: number = 60;

export const DEFAULT_DIMENSIONS: Dimensions = {
  width: 600,
  height: 150,
};
