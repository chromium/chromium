// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A set of utilities to help test the chrome://personalization
 * SWA.
 */

import {emptyState, PersonalizationState, setAmbientProviderForTesting, setKeyboardBacklightProviderForTesting, setThemeProviderForTesting, setUserProviderForTesting, setWallpaperProviderForTesting} from 'chrome://personalization/js/personalization_app.js';
import {String16} from 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import {flush, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {TestAmbientProvider} from './test_ambient_interface_provider.js';
import {TestKeyboardBacklightProvider} from './test_keyboard_backlight_interface_provider.js';
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
  const keyboardBacklightProvider = new TestKeyboardBacklightProvider();
  setKeyboardBacklightProviderForTesting(keyboardBacklightProvider);
  const themeProvider = new TestThemeProvider();
  setThemeProviderForTesting(themeProvider);
  const userProvider = new TestUserProvider();
  setUserProviderForTesting(userProvider);
  const personalizationStore = new TestPersonalizationStore(initialState);
  personalizationStore.replaceSingleton();
  document.body.innerHTML = window.trustedTypes!.emptyHTML;
  return {
    ambientProvider,
    keyboardBacklightProvider,
    themeProvider,
    userProvider,
    wallpaperProvider,
    personalizationStore,
  };
}

/** Returns a |String16| from the specified |value|. */
export function toString16(value: string): String16 {
  const data = [];
  for (let i = 0; i < value.length; ++i) {
    data[i] = value.charCodeAt(i);
  }
  return {data};
}

/**
 * Returns a svg data url. This is useful in tests to force img on-load events
 * to fire so that wallpaper-grid-item resolves its loading state.
 */
export function createSvgDataUrl(id: string): string {
  return 'data:image/svg+xml;utf8,' +
      '<svg xmlns="http://www.w3.org/2000/svg" ' +
      `height="100px" width="100px" id="${id}">` +
      '<rect fill="red" height="100px" width="100px"></rect>' +
      '</svg>';
}

/**
 * Waits for the specified |element| to be the active element in
 * the containing element's shadow DOM.
 */
export async function waitForActiveElement(
    targetElement: Element, elementContainer: HTMLElement) {
  while (elementContainer.shadowRoot!.activeElement !== targetElement) {
    await waitAfterNextRender(elementContainer!);
  }
}

/** Dispatches a keydown event to |element| for the specified |key|. */
export function dispatchKeydown(element: HTMLElement, key: string) {
  const init: KeyboardEventInit = {bubbles: true, key};
  switch (key) {
    case 'ArrowDown':
      init.keyCode = 40;
      break;
    case 'ArrowRight':
      init.keyCode = 39;
      break;
    case 'ArrowLeft':
      init.keyCode = 37;
      break;
    case 'ArrowUp':
      init.keyCode = 38;
      break;
  }
  element.dispatchEvent(new KeyboardEvent('keydown', init));
}

/** Returns the active element in the given element's shadow DOM. */
export function getActiveElement(element: Element): HTMLElement {
  return (element.shadowRoot!.activeElement as HTMLElement);
}
