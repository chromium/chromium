// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import {ActionName} from 'chrome://personalization/trusted/personalization_actions.js';
import {WallpaperSelected} from 'chrome://personalization/trusted/wallpaper_selected_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperSelectedTest() {
  /** @type {?HTMLElement} */
  let wallpaperSelectedElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperSelectedElement) {
      wallpaperSelectedElement.remove();
    }
    wallpaperSelectedElement = null;
    await flushTasks();
  });

  test('shows loading spinner at startup', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected.is);

    assertEquals(
        null, wallpaperSelectedElement.shadowRoot.querySelector('img'));

    assertEquals(
        null,
        wallpaperSelectedElement.shadowRoot.getElementById('textContainer'));

    const spinner =
        wallpaperSelectedElement.shadowRoot.querySelector('paper-spinner-lite');

    assertTrue(spinner.active);
  });

  test('sets wallpaper image in store on first load', async () => {
    personalizationStore.expectAction(ActionName.SET_CURRENT_IMAGE);
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    const action =
        await personalizationStore.waitForAction(ActionName.SET_CURRENT_IMAGE);
    assertEquals(wallpaperProvider.currentWallpaper, action.image);
  });

  test('shows wallpaper image and attribution when loaded', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected.is);

    const spinner =
        wallpaperSelectedElement.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(spinner.active);

    personalizationStore.data = {
      selectedImage: wallpaperProvider.currentWallpaper
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertFalse(spinner.active);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    const attributionLines =
        wallpaperSelectedElement.shadowRoot.getElementById('textContainer')
            .querySelectorAll('p');
    assertEquals(
        wallpaperProvider.currentWallpaper.attribution.length,
        attributionLines.length);
    assertEquals(
        wallpaperProvider.currentWallpaper.attribution[0],
        attributionLines[0].innerText);
  });

  test('removes high resolution suffix from image url', async () => {
    personalizationStore.data.selectedImage = {
      url: {url: 'https://images.googleusercontent.com/abc12=w456'},
      attribution: [],
      assetId: BigInt(100),
    };
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        'chrome://image/?https://images.googleusercontent.com/abc12', img.src);
  });

  test('updates image when store is updated', async () => {
    personalizationStore.data.selectedImage =
        wallpaperProvider.currentWallpaper;

    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    personalizationStore.data.selectedImage = {
      url: {url: 'https://testing'},
      attribution: ['New attribution'],
      assetId: BigInt(100),
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals('chrome://image/?https://testing', img.src);
  });

  test('shows error text when image fails to load', async () => {
    personalizationStore.data.selectedImage = null;

    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        null, wallpaperSelectedElement.shadowRoot.querySelector('img'));

    assertEquals(
        'There was an error',
        wallpaperSelectedElement.shadowRoot.getElementById('error').innerText);
  });
}
