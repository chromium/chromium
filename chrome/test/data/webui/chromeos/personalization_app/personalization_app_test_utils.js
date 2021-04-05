// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {setWallpaperProviderForTesting} from 'chrome://personalization/mojo_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';

/**
 * Constructs the given element with attributes and appends it to body.
 * @param {!string} tag
 * @param {!Object} attributes
 * @returns {!HTMLElement}
 */
export function initElement(tag, attributes = {}) {
  const element = /** @type {!HTMLElement} **/ (document.createElement(tag));
  for (const [key, value] of Object.entries(attributes)) {
    element.setAttribute(key, value);
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
