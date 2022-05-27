// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {getDarkLightImageTiles, getRegularImageTiles, IFrameApi, PersonalizationRouter, WallpaperImages, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, setupTestIFrameApi, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperImagesTest', function() {
  let wallpaperImagesElement: WallpaperImages|null = null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

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
    const testProxy = setupTestIFrameApi();

    // Set the current wallpaper as an online wallpaper.
    // The currentSelected asset id should be sent to iframe.
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperImagesElement =
        initElement(WallpaperImages, {collectionId: 'id_0'});

    const imagesGrid = wallpaperImagesElement.$.imagesGrid;

    // Wait for iframe to receive data.
    let [target, data] =
        await testProxy.whenCalled('sendCurrentWallpaperAssetId') as
        Parameters<IFrameApi['sendCurrentWallpaperAssetId']>;

    assertEquals(imagesGrid, target);
    assertDeepEquals(
        BigInt(personalizationStore.data.wallpaper.currentSelected.key), data);

    testProxy.resetResolver('sendCurrentWallpaperAssetId');

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
    [target, data] =
        await testProxy.whenCalled('sendCurrentWallpaperAssetId') as
        Parameters<IFrameApi['sendCurrentWallpaperAssetId']>;
    assertEquals(imagesGrid, target);
    assertDeepEquals(
        BigInt(personalizationStore.data.wallpaper.currentSelected.key), data);

    testProxy.resetResolver('sendCurrentWallpaperAssetId');

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
    [target, data] =
        await testProxy.whenCalled('sendCurrentWallpaperAssetId') as
        Parameters<IFrameApi['sendCurrentWallpaperAssetId']>;
    assertEquals(imagesGrid, target);
    assertEquals(undefined, data);
  });

  test('displays images for current collection id', async () => {
    loadTimeData.overrideValues({isDarkLightModeEnabled: false});
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

    const testProxy = setupTestIFrameApi();
    wallpaperImagesElement =
        initElement(WallpaperImages, {collectionId: 'id_0'});

    const imagesGrid = wallpaperImagesElement.$.imagesGrid;

    // Wait for iframe to receive data.
    let [target, data] = await testProxy.whenCalled('sendImageTiles') as
        Parameters<IFrameApi['sendImageTiles']>;
    assertEquals(imagesGrid, target);
    assertDeepEquals(
        getRegularImageTiles(
            personalizationStore.data.wallpaper.backdrop.images['id_0']),
        data);
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);

    testProxy.resetResolver('sendImageTiles');
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for iframe to receive new data.
    [target, data] = await testProxy.whenCalled('sendImageTiles') as
        Parameters<IFrameApi['sendImageTiles']>;

    await waitAfterNextRender(wallpaperImagesElement);

    assertEquals(imagesGrid, target);
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

    const testProxy = setupTestIFrameApi();
    wallpaperImagesElement =
        initElement(WallpaperImages, {collectionId: 'id_0'});

    const imagesGrid = wallpaperImagesElement.$.imagesGrid;

    // Wait for images-grid to receive data.
    let [target, data] = await testProxy.whenCalled('sendImageTiles') as
        Parameters<IFrameApi['sendImageTiles']>;
    assertEquals(imagesGrid, target);
    const tiles = getDarkLightImageTiles(
        wallpaperImagesElement.isDarkModeActive,
        personalizationStore.data.wallpaper.backdrop.images['id_0']);
    assertDeepEquals(tiles, data);
    assertEquals(data[0]!.preview.length, 2);
    // Check that light variant comes before dark variant.
    assertEquals(
        data[0]!.preview[0]!.url, 'https://images.googleusercontent.com/1');
    assertEquals(
        data[0]!.preview[1]!.url, 'https://images.googleusercontent.com/0');
    // Wait for a render to happen.
    await waitAfterNextRender(wallpaperImagesElement);

    testProxy.resetResolver('sendImageTiles');
    wallpaperImagesElement.collectionId = 'id_1';

    // Wait for iframe to receive new data.
    [target, data] = await testProxy.whenCalled('sendImageTiles') as
        Parameters<IFrameApi['sendImageTiles']>;

    await waitAfterNextRender(wallpaperImagesElement);

    assertEquals(imagesGrid, target);
    assertDeepEquals(
        getDarkLightImageTiles(
            wallpaperImagesElement.isDarkModeActive,
            personalizationStore.data.wallpaper.backdrop.images['id_1']),
        data);
  });

  test('navigates back to main page on loading failure', async () => {
    const reloadPromise = new Promise<void>((resolve) => {
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
        initElement(WallpaperImages, {collectionId: 'id_0'});

    // Simulate finish loading. Images still null. Should bail and reload
    // application.
    personalizationStore.data.wallpaper.loading = {images: {'id_0': false}};
    personalizationStore.notifyObservers();

    await reloadPromise;
  });

  test('navigates back to main page on unknown collection id', async () => {
    const reloadPromise = new Promise<void>((resolve) => {
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
        initElement(WallpaperImages, {collectionId: 'unknown_id'});

    await reloadPromise;
  });
});
