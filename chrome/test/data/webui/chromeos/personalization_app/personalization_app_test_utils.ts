// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {setAmbientProviderForTesting} from 'chrome://personalization/trusted/ambient/ambient_interface_provider.js';
import {IFrameApi} from 'chrome://personalization/trusted/iframe_api.js';
import {emptyState, PersonalizationState} from 'chrome://personalization/trusted/personalization_state.js';
import {setThemeProviderForTesting} from 'chrome://personalization/trusted/theme/theme_interface_provider.js';
import {setUserProviderForTesting} from 'chrome://personalization/trusted/user/user_interface_provider.js';
import {setWallpaperProviderForTesting} from 'chrome://personalization/trusted/wallpaper/wallpaper_interface_provider.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {flushTasks} from 'chrome://webui-test/test_util.js';

import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestThemeProvider} from './test_theme_interface_provider.js';
import {TestUserProvider} from './test_user_interface_provider.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

/**
 * Constructs the given element with properties and appends it to body.
 */
export function initElement<T extends PolymerElement>(
    cls: {new (): T; is: string}, properties = {}): T {
  const element = document.createElement(cls.is) as T & HTMLElement;
  for (const [key, value] of Object.entries(properties)) {
    (element as any)[key] = value;
  }
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Tear down an element. Make sure the iframe load callback
 * has completed to avoid weird race condition with loading.
 * @see {b/185905694, crbug/466089}
 */
export async function teardownElement(element: HTMLElement|null) {
  if (!element) {
    return;
  }
  const iframe = await (element as any).iframePromise_;
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
 */
export function baseSetup(initialState: PersonalizationState = emptyState()) {
  const wallpaperProvider = new TestWallpaperProvider();
  setWallpaperProviderForTesting(wallpaperProvider);
  const ambientProvider = new TestAmbientProvider();
  setAmbientProviderForTesting(ambientProvider);
  const themeProvider = new TestThemeProvider();
  setThemeProviderForTesting(themeProvider);
  const userProvider = new TestUserProvider();
  setUserProviderForTesting(userProvider);
  const personalizationStore = new TestPersonalizationStore(initialState);
  personalizationStore.replaceSingleton();
  document.body.innerHTML = '';
  return {
    ambientProvider,
    themeProvider,
    userProvider,
    wallpaperProvider,
    personalizationStore
  };
}

function getDebugString(w: any) {
  if (w === window) {
    return w.location.href;
  }
  return 'iframe';
}

/**
 * Helper function to test if two window objects are the same.
 * Plain |assertEquals| fails when it attempts to get a debug string
 * representation of cross-origin iframe window.
 */
export function assertWindowObjectsEqual(x: object|null, y: object|null) {
  assertTrue(
      x === y,
      `Window objects are not identical: ${getDebugString(x)}, ${
          getDebugString(y)}`);
}

/**
 * Helper function to setup a mock `IFrameApi` singleton.
 */
export function setupTestIFrameApi(): IFrameApi&TestBrowserProxy<IFrameApi> {
  const testProxy = TestBrowserProxy.fromClass(IFrameApi);
  IFrameApi.setInstance(testProxy);
  return testProxy;
}
