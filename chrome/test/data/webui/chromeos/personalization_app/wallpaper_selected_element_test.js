// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import {ActionName} from 'chrome://personalization/trusted/personalization_actions.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_reducers.js';
import {WallpaperSelected} from 'chrome://personalization/trusted/wallpaper_selected_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
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
    personalizationStore.expectAction(ActionName.SET_SELECTED_IMAGE);
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    const action =
        await personalizationStore.waitForAction(ActionName.SET_SELECTED_IMAGE);
    assertEquals(wallpaperProvider.currentWallpaper, action.image);
  });

  test('shows wallpaper image and attribution when loaded', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected.is);

    const spinner =
        wallpaperSelectedElement.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(spinner.active);

    personalizationStore.data.loading.selected = false;
    personalizationStore.data.selected = wallpaperProvider.currentWallpaper;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertFalse(spinner.active);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    const textContainerElements =
        wallpaperSelectedElement.shadowRoot.querySelectorAll(
            '#textContainer p');

    // First p tag is 'Currently Set' text.
    assertEquals('currentlySet', textContainerElements[0].id);
    assertEquals(
        wallpaperSelectedElement.i18n('currentlySet'),
        textContainerElements[0].textContent);

    // Following text elements are for the photo attribution text.
    const attributionLines = Array.from(textContainerElements).slice(1);

    assertEquals(
        wallpaperProvider.currentWallpaper.attribution.length,
        attributionLines.length);
    wallpaperProvider.currentWallpaper.attribution.forEach((line, i) => {
      assertEquals(line, attributionLines[i].innerText);
    });
  });

  test('removes high resolution suffix from image url', async () => {
    personalizationStore.data.selected = {
      url: {url: 'https://images.googleusercontent.com/abc12=w456'},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        'chrome://image/?https://images.googleusercontent.com/abc12', img.src);
  });

  test('updates image when store is updated', async () => {
    personalizationStore.data.selected = wallpaperProvider.currentWallpaper;
    personalizationStore.data.loading.selected = false;

    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals(
        `chrome://image/?${wallpaperProvider.currentWallpaper.url.url}`,
        img.src);

    personalizationStore.data.selected = {
      url: {url: 'https://testing'},
      attribution: ['New attribution'],
      assetId: BigInt(100),
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals('chrome://image/?https://testing', img.src);
  });

  test('shows error text when image fails to load', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    // Still loading.
    personalizationStore.data.loading.selected = true;
    personalizationStore.data.selected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    const spinner =
        wallpaperSelectedElement.shadowRoot.querySelector('paper-spinner-lite');
    assertTrue(spinner.active);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.loading.selected = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertFalse(spinner.active);

    assertEquals(
        null, wallpaperSelectedElement.shadowRoot.querySelector('img'));

    assertEquals(
        'There was an error',
        wallpaperSelectedElement.shadowRoot.getElementById('error').innerText);
  });

  test('sets selected wallpaper data in store', async () => {
    // Make sure state starts as expected.
    assertDeepEquals(emptyState(), personalizationStore.data);
    assertTrue(personalizationStore.data.loading.selected);
    // Run the actual reducers.
    personalizationStore.setReducersEnabled(true);

    wallpaperSelectedElement = initElement(WallpaperSelected.is);

    // Wait for api call to complete.
    await wallpaperProvider.whenCalled('getCurrentWallpaper');

    // Should be done loading now.
    assertFalse(personalizationStore.data.loading.selected);
    // Shallow equals - they should be the same object.
    assertEquals(
        wallpaperProvider.currentWallpaper, personalizationStore.data.selected);
  });

  test('shows image url with data scheme', async () => {
    personalizationStore.data.selected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected.is);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot.querySelector('img');
    assertEquals('data:image/png;base64,abc=', img.src);
  });
}
