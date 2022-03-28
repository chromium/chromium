// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {emptyState, IFrameApi, PersonalizationState, setAmbientProviderForTesting, setThemeProviderForTesting, setUserProviderForTesting, setWallpaperProviderForTesting} from 'chrome://personalization/trusted/personalization_app.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
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
    cls: {new (): T, is: string}, properties = {}): T {
  const element = document.createElement(cls.is) as T & HTMLElement;
  for (const [key, value] of Object.entries(properties)) {
    (element as any)[key] = value;
  }
  document.body.appendChild(element);
  flush();
  return element;
}

/**
 * Tear down an element. Remove from dom and call |flushTasks| to finish any
 * async cleanup in polymer and execute pending promises.
 */
export async function teardownElement(element: HTMLElement|null) {
  if (!element) {
    return;
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

/**
 * Helper function to setup a mock `IFrameApi` singleton.
 */
export function setupTestIFrameApi(): IFrameApi&TestBrowserProxy<IFrameApi> {
  const testProxy = TestBrowserProxy.fromClass(IFrameApi);
  IFrameApi.setInstance(testProxy);
  return testProxy;
}
