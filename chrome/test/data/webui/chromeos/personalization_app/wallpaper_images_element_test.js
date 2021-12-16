// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {PersonalizationRouter} from 'chrome://personalization/trusted/personalization_router_element.js';
import {getDarkLightImageTiles, getRegularImageTiles, promisifyImagesIframeFunctionsForTesting, WallpaperImages} from 'chrome://personalization/trusted/wallpaper/wallpaper_images_element.js';

import {assertDeepEquals, assertEquals, assertFalse} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

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

  test('send current wallpaper asset id', async () => {
    let {sendCurrentWallpaperAssetId: sendCurrentWallpaperAssetIdPromise} =
        promisifyImagesIframeFunctionsForTesting();

    // Set the current wallpaper as an online wallpaper.
    // The currentSelected asset id should be sent to iframe.
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    const iframe =
        wallpaperImagesElement.shadowRoot.getElementById('images-iframe');

    // Wait for iframe to receive data.
    let [targetWindow, data] = await sendCurrentWallpaperAssetIdPromise;

    assertEquals(iframe.contentWindow, targetWindow);
    assertDeepEquals(
        BigInt(personalizationStore.data.wallpaper.currentSelected.key), data);

    sendCurrentWallpaperAssetIdPromise =
        promisifyImagesIframeFunctionsForTesting().sendCurrentWallpaperAssetId;

    // Set the current wallpaper as a daily refresh wallpaper.
    // The currentSelected asset id should be sent to iframe.
    personalizationStore.data.wallpaper.currentSelected = {
      attribution: ['Image 1'],
      layout: WallpaperLayout.kCenter,
      key: '2',
      type: WallpaperType.kDaily,
      url: {url: 'https://images.googleusercontent.com/1'},
    };
    personalizationStore.notifyObservers();

    // Wait for iframe to receive data.
    [targetWindow, data] = await sendCurrentWallpaperAssetIdPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    assertDeepEquals(
        BigInt(personalizationStore.data.wallpaper.currentSelected.key), data);

    sendCurrentWallpaperAssetIdPromise =
        promisifyImagesIframeFunctionsForTesting().sendCurrentWallpaperAssetId;

    // Set the current wallpaper not as an online wallpaper.
    // No asset id is sent to iframe.
    personalizationStore.data.wallpaper.currentSelected = {
      attribution: ['Image 2'],
      layout: WallpaperLayout.kCenter,
      key: '3',
      type: WallpaperType.kDefault,
      url: {url: 'https://images.googleusercontent.com/2'},
    };
    personalizationStore.notifyObservers();

    // Wait for iframe to receive data.
    [targetWindow, data] = await sendCurrentWallpaperAssetIdPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    assertEquals(null, data);
  });

  test('displays images for current collection id', async () => {
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': wallpaperProvider.images,
      'id_1': [
        {assetId: BigInt(10), url: {url: 'https://id_1-0/'}},
        {assetId: BigInt(20), url: {url: 'https://id_1-1/'}},
      ],
    };
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.loading.images = {
      'id_0': false,
      'id_1': false
    };
    personalizationStore.data.wallpaper.loading.collections = false;

    let {sendImageTiles: sendImageTilesPromise} =
        promisifyImagesIframeFunctionsForTesting();
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    const iframe =
        wallpaperImagesElement.shadowRoot.getElementById('images-iframe');

    // Wait for iframe to receive data.
    let [targetWindow, data] = await sendImageTilesPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    assertDeepEquals(
        getRegularImageTiles(
            personalizationStore.data.wallpaper.backdrop.images['id_0']),
        data);
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);
    assertFalse(iframe.hidden);

    sendImageTilesPromise =
        promisifyImagesIframeFunctionsForTesting().sendImageTiles;
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for iframe to receive new data.
    [targetWindow, data] = await sendImageTilesPromise;

    await waitAfterNextRender(wallpaperImagesElement);

    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, targetWindow);
    assertDeepEquals(
        getRegularImageTiles(
            personalizationStore.data.wallpaper.backdrop.images['id_1']),
        data);
  });

  test('display dark/light tile for personalization hub', async () => {
    loadTimeData.overrideValues({isDarkLightModeEnabled: true});
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': wallpaperProvider.images,
      'id_1': [
        {assetId: BigInt(10), url: {url: 'https://id_1-0/'}},
        {assetId: BigInt(20), url: {url: 'https://id_1-1/'}},
      ],
    };
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.loading.images = {
      'id_0': false,
      'id_1': false
    };
    personalizationStore.data.wallpaper.loading.collections = false;

    let {sendImageTiles: sendImageTilesPromise} =
        promisifyImagesIframeFunctionsForTesting();
    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    const iframe =
        wallpaperImagesElement.shadowRoot.getElementById('images-iframe');

    // Wait for iframe to receive data.
    let [targetWindow, data] = await sendImageTilesPromise;
    assertEquals(iframe.contentWindow, targetWindow);
    const tiles = getDarkLightImageTiles(
        false, personalizationStore.data.wallpaper.backdrop.images['id_0']);
    assertDeepEquals(tiles, data);
    assertEquals(data[0].preview.length, 2);
    // Check that light variant comes before dark variant.
    assertEquals(
        data[0].preview[0].url, 'https://images.googleusercontent.com/1');
    assertEquals(
        data[0].preview[1].url, 'https://images.googleusercontent.com/0');
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);
    assertFalse(iframe.hidden);

    sendImageTilesPromise =
        promisifyImagesIframeFunctionsForTesting().sendImageTiles;
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for iframe to receive new data.
    [targetWindow, data] = await sendImageTilesPromise;

    await waitAfterNextRender(wallpaperImagesElement);

    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, targetWindow);
    assertDeepEquals(
        getDarkLightImageTiles(
            false, personalizationStore.data.wallpaper.backdrop.images['id_1']),
        data);
  });

  test('navigates back to main page on loading failure', async () => {
    const reloadPromise = new Promise((resolve) => {
      PersonalizationRouter.reloadAtWallpaper = resolve;
    });

    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.backdrop.images = {'id_0': null};
    personalizationStore.data.wallpaper.loading = {
      collections: false,
      images: {'id_0': true}
    };

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'id_0'});

    // Simulate finish loading. Images still null. Should bail and reload
    // application.
    personalizationStore.data.wallpaper.loading = {images: {'id_0': false}};
    personalizationStore.notifyObservers();

    await reloadPromise;
  });

  test('navigates back to main page on unknown collection id', async () => {
    const reloadPromise = new Promise((resolve) => {
      PersonalizationRouter.reloadAtWallpaper = resolve;
    });

    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': wallpaperProvider.images
    };
    personalizationStore.data.wallpaper.loading = {
      collections: false,
      images: {'id_0': false},
    };

    wallpaperImagesElement =
        initElement(WallpaperImages.is, {collectionId: 'unknown_id'});

    await reloadPromise;
  });
}
