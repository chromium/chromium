// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PersonalizationRouter} from 'chrome://personalization/trusted/personalization_router_element.js';
import {promisifySendImagesForTesting, WallpaperImages} from 'chrome://personalization/trusted/wallpaper_images_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.m.js';
import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperImagesTest() {
  /** @type {?HTMLElement} */
  let wallpaperImagesElement = null;

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
    await teardownElement(wallpaperImagesElement);
    wallpaperImagesElement = null;
    await flushTasks();
  });

  test('displays images for current collection id', async () => {
    personalizationStore.data.backdrop.images = {
      'id_0': wallpaperProvider.images,
      'id_1': [
        {assetId: BigInt(10), url: {url: 'https://id_1-0/'}},
        {assetId: BigInt(20), url: {url: 'https://id_1-1/'}},
      ],
    };
    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.loading.images = {'id_0': false, 'id_1': false};
    personalizationStore.data.loading.collections = false;

    let sendImagesPromise = promisifySendImagesForTesting();
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    const iframe =
        wallpaperImagesElement.shadowRoot.getElementById('images-iframe');

    // Wait for iframe to receive data.
    let [targetWindow, data] = await sendImagesPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    assertDeepEquals(personalizationStore.data.backdrop.images['id_0'], data);
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);
    assertFalse(iframe.hidden);

    sendImagesPromise = promisifySendImagesForTesting();
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for iframe to receive new data.
    [targetWindow, data] = await sendImagesPromise;

    await waitAfterNextRender(wallpaperImagesElement);

    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, targetWindow);
    assertDeepEquals(personalizationStore.data.backdrop.images['id_1'], data);
  });

  test('navigates back to main page on loading failure', async () => {
    const reloadPromise = new Promise((resolve) => {
      PersonalizationRouter.reloadAtRoot = resolve;
    });

    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.backdrop.images = {'id_0': null};
    personalizationStore.data.loading = {
      collections: false,
      images: {'id_0': true}
    };

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    // Simulate finish loading. Images still null. Should bail and reload
    // application.
    personalizationStore.data.loading = {images: {'id_0': false}};
    personalizationStore.notifyObservers();

    await reloadPromise;
  });

  test('navigates back to main page on unknown collection id', async () => {
    const reloadPromise = new Promise((resolve) => {
      PersonalizationRouter.reloadAtRoot = resolve;
    });

    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.backdrop.images = {
      'id_0': wallpaperProvider.images
    };
    personalizationStore.data.loading = {
      collections: false,
      images: {'id_0': false},
    };

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'unknown_id'});

    await reloadPromise;
  });
}
