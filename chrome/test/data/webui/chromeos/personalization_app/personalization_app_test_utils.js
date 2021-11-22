// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {setWallpaperProviderForTesting} from 'chrome://personalization/trusted/wallpaper/wallpaper_interface_provider.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

/**
 * Constructs the given element with properties and appends it to body.
 * TODO(cowmoo) make generic and cast to specific polymer types.
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
 * Tear down an element. Make sure the iframe load callback
 * has completed to avoid weird race condition with loading.
 * @see {b/185905694, crbug/466089}
 * @param {*} element
 */
export async function teardownElement(element) {
  if (!element) {
    return;
  }
  const iframe = await element.iframePromise_;
  if (iframe) {
    iframe.remove();
    await flushTasks();
  }
  element.remove();
  await flushTasks();
}

/**
 * Sets up the test wallpaper provider, test personalization store, and clears
 * the page.
 * @param {!PersonalizationState} initialState
 * @return {{wallpaperProvider: !TestWallpaperProvider, personalizationStore:
 *     !TestPersonalizationStore}}
 */
export function baseSetup(initialState = emptyState()) {
  const wallpaperProvider = new TestWallpaperProvider();
  setWallpaperProviderForTesting(wallpaperProvider);
  const personalizationStore = new TestPersonalizationStore(initialState);
  personalizationStore.replaceSingleton();
  document.body.innerHTML = '';
  return {wallpaperProvider, personalizationStore};
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
