// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {setWallpaperProviderForTesting} from 'chrome://personalization/trusted/mojo_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from '../../chai_assert.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';

/**
 * Constructs the given element with properties and appends it to body.
 * @param {!string} tag
 * @param {!Object} properties
 * @returns {!HTMLElement}
 */
export function initElement(tag, properties = {}) {
  const element = /** @type {!HTMLElement} **/ (document.createElement(tag));
  for (const [key, value] of Object.entries(properties)) {
    element[key] = value;
  }
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Sets up the test wallpaper provider and clears the page.
 * @returns {!TestWallpaperProvider}
 */
export function baseSetup() {
  const wallpaperProvider = new TestWallpaperProvider();
  setWallpaperProviderForTesting(wallpaperProvider);
  document.body.innerHTML = '';
  return wallpaperProvider;
}

function getDebugString(w) {
  if (w === window) {
    return w.location.href;
  }
  return 'iframe';
}

/**
 * Helper function to test if two window objects are the same.
 * Plain |assertEquals| fails when it attempts to get a debug string
 * representation of cross-origin iframe window.
 * @param {!Object} x
 * @param {!Object} y
 */
export function assertWindowObjectsEqual(x, y) {
  assertTrue(
      x === y,
      `Window objects are not identical: ${getDebugString(x)}, ${
          getDebugString(y)}`);
}
