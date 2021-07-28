// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kMaximumLocalImagePreviews} from 'chrome://personalization/common/constants.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_reducers.js';
import {promisifyIframeFunctionsForTesting, WallpaperCollections} from 'chrome://personalization/trusted/wallpaper_collections_element.js';
import {assertDeepEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.m.js';
import {assertWindowObjectsEqual, baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperCollectionsTest() {
  /** @type {?HTMLElement} */
  let wallpaperCollectionsElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  setup(function() {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(wallpaperCollectionsElement);
    wallpaperCollectionsElement = null;
  });

  test('sends wallpaper collections when loaded', async () => {
    const {sendCollections: sendCollectionsPromise} =
        promisifyIframeFunctionsForTesting();
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    personalizationStore.data.loading = {
      ...personalizationStore.data.loading,
      collections: false
    };
    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    const [target, data] = await sendCollectionsPromise;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe');
    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, target);
    assertDeepEquals(wallpaperProvider.collections, data);
  });

  test('sends image counts when a collection loads', async () => {
    personalizationStore.data.backdrop = {
      collections: wallpaperProvider.collections,
      images: {},
    };
    personalizationStore.data.loading = {
      ...personalizationStore.data.loading,
      collections: false,
      images: {},
    };

    wallpaperCollectionsElement = initElement(WallpaperCollections.is);
    // Wait for initial load to complete.
    await promisifyIframeFunctionsForTesting().sendImageCounts;

    let {sendImageCounts: sendImageCountsPromise} =
        promisifyIframeFunctionsForTesting();

    personalizationStore.data.backdrop.images = {
      'id_0': [wallpaperProvider.images[0]]
    };
    personalizationStore.data.loading.images = {'id_0': false};
    personalizationStore.notifyObservers();

    let counts = (await sendImageCountsPromise)[1];
    assertDeepEquals({'id_0': 1}, counts);

    // Load two collections in at once, and simulate one failure.
    sendImageCountsPromise =
        promisifyIframeFunctionsForTesting().sendImageCounts;
    personalizationStore.data.backdrop.images = {
      'id_0': [wallpaperProvider.images[0]],
      'id_1': [wallpaperProvider.images[0], wallpaperProvider.images[1]],
      'id_2': [],
      'id_3': null,
    };
    personalizationStore.data.loading.images = {
      'id_0': false,
      'id_1': false,
      'id_2': false,
      'id_3': false,
    };
    personalizationStore.notifyObservers();

    counts = (await sendImageCountsPromise)[1];
    assertDeepEquals({'id_0': 1, 'id_1': 2, 'id_2': 0, 'id_3': null}, counts);
  });

  test('sends local images when loaded', async () => {
    const {sendLocalImages: sendLocalImagesPromise} =
        promisifyIframeFunctionsForTesting();

    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    personalizationStore.data.loading = {
      ...personalizationStore.data.loading,
      collections: false,
      local: {images: false}
    };
    personalizationStore.data.local.images = wallpaperProvider.localImages;
    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendLocalImages| to be called.
    const [target, data] = await sendLocalImagesPromise;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe');
    assertFalse(iframe.hidden);

    assertWindowObjectsEqual(iframe.contentWindow, target);
    assertDeepEquals(wallpaperProvider.localImages, data);
  });

  test('shows error when fails to load', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    // No error displayed while loading.
    let error =
        wallpaperCollectionsElement.shadowRoot.querySelector('wallpaper-error');
    assertTrue(error === null);

    personalizationStore.data.loading = {
      ...personalizationStore.data.loading,
      collections: false,
    };
    personalizationStore.data.backdrop.collections = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    error =
        wallpaperCollectionsElement.shadowRoot.querySelector('wallpaper-error');
    assertTrue(!!error);

    // Iframe should be hidden if there is an error.
    assertTrue(
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe').hidden);
  });

  test('loads backdrop and local data and saves to store', async () => {
    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);

    const {
      sendCollections: sendCollectionsPromise,
      sendLocalImages: sendLocalImagesPromise
    } = promisifyIframeFunctionsForTesting();

    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const [_, collections] = await sendCollectionsPromise;
    assertDeepEquals(wallpaperProvider.collections, collections);

    const [__, localImages] = await sendLocalImagesPromise;
    assertDeepEquals(wallpaperProvider.localImages, localImages);

    assertDeepEquals(
        {
          collections: wallpaperProvider.collections,
          images: {
            'id_0': wallpaperProvider.images,
            'id_1': wallpaperProvider.images,
          },
        },
        personalizationStore.data.backdrop,
    );
    assertDeepEquals(
        {
          images: wallpaperProvider.localImages,
          data: wallpaperProvider.localImageData,
        },
        personalizationStore.data.local,
    );
    assertDeepEquals(
        {
          ...emptyState().loading,
          collections: false,
          images: {
            'id_0': false,
            'id_1': false,
          },
          local: {
            images: false,
            data: {
              '100,10': false,
              '200,20': false,
            },
          },
        },
        personalizationStore.data.loading,
    );
  });

  test(
      'sends the first three local images that successfully load thumbnails',
      async () => {
        // Set up store data. Local image list is loaded, but thumbnails are
        // still loading in.
        personalizationStore.data.loading.local.images = false;
        personalizationStore.data.local.images = [];
        for (let i = 0; i < kMaximumLocalImagePreviews; i++) {
          personalizationStore.data.local.images.push(
              {id: {high: BigInt(i * 2), low: BigInt(i)}, name: `local-${i}`});
          personalizationStore.data.loading.local.data[`${i * 2},${i}`] = true;
        }
        // Collections are finished loading.
        personalizationStore.data.backdrop.collections =
            wallpaperProvider.collections;
        personalizationStore.data.loading.collections = false;

        const {sendLocalImages, sendLocalImageData} =
            promisifyIframeFunctionsForTesting();

        wallpaperCollectionsElement = initElement(WallpaperCollections.is);

        await sendLocalImages;

        // No thumbnails loaded so none sent.
        assertFalse(wallpaperCollectionsElement.didSendLocalImageData_);

        // First thumbnail loads in.
        personalizationStore.data.loading.local.data = {'0,0': false};
        personalizationStore.data.local.data = {'0,0': 'local_data_0'};
        personalizationStore.notifyObservers();

        await wallpaperCollectionsElement.iframePromise_;
        await waitAfterNextRender(wallpaperCollectionsElement);

        // Should not have sent any image data since more thumbnails are still
        // loading.
        assertFalse(wallpaperCollectionsElement.didSendLocalImageData_);

        // Second thumbnail fails loading. Third succeeds.
        personalizationStore.data.loading.local.data = {
          ...personalizationStore.data.loading.local.data,
          '2,1': false,
          '4,2': false,
        };
        personalizationStore.data.local.data = {
          ...personalizationStore.data.local.data,
          '2,1': null,
          '4,2': 'local_data_2',
        };
        personalizationStore.notifyObservers();

        // 2 thumbnails have now loaded. 1 failed. But there are no more
        // remaining to try loading, should send local image data anyway.
        const [_, sentData] = await sendLocalImageData;

        assertTrue(wallpaperCollectionsElement.didSendLocalImageData_);
        assertDeepEquals(
            {'0,0': 'local_data_0', '4,2': 'local_data_2'}, sentData);
      });
}
