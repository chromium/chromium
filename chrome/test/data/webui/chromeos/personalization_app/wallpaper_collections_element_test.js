// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kMaximumLocalImagePreviews} from 'chrome://personalization/common/constants.js';
import {unguessableTokenToString} from 'chrome://personalization/common/utils.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_reducers.js';
import {promisifyIframeFunctionsForTesting, WallpaperCollections} from 'chrome://personalization/trusted/wallpaper_collections_element.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
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

  test('shows loading on startup', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    const iframe =
        wallpaperCollectionsElement.shadowRoot.querySelector('iframe');
    assertTrue(iframe.hidden);
  });

  test('shows wallpaper collections when loaded', async () => {
    const {sendCollections: sendCollectionsPromise} =
        promisifyIframeFunctionsForTesting();
    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(!!spinner);
    assertTrue(spinner.active);

    personalizationStore.data.loading = {collections: false};
    personalizationStore.data.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    const [target, data] = await sendCollectionsPromise;
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);

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
      collections: false,
      images: {},
    };

    wallpaperCollectionsElement = initElement(WallpaperCollections.is);
    // Wait for initial load to complete.
    await wallpaperCollectionsElement.iframePromise_;

    let {sendImageCounts: sendImageCountsPromise} =
        promisifyIframeFunctionsForTesting();

    personalizationStore.data.backdrop.images = {
      'id_0': [wallpaperProvider.images[0]]
    };
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
      // Ignores id_3 because it is not Array.
      'id_3': undefined,
    };
    personalizationStore.notifyObservers();

    counts = (await sendImageCountsPromise)[1];
    assertDeepEquals({'id_0': 1, 'id_1': 2, 'id_2': 0}, counts);
  });

  test('sends local images when loaded', async () => {
    const {sendLocalImages: sendLocalImagesPromise} =
        promisifyIframeFunctionsForTesting();

    wallpaperCollectionsElement = initElement(WallpaperCollections.is);

    personalizationStore.data.loading = {
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

    const spinner = wallpaperCollectionsElement.shadowRoot.querySelector(
        'paper-spinner-lite');
    assertTrue(spinner.active);

    // No error displayed while loading.
    const error =
        wallpaperCollectionsElement.shadowRoot.querySelector('#error');
    assertTrue(error.hidden);

    personalizationStore.data.loading = {collections: false};
    personalizationStore.data.backdrop.collections = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(spinner.active);
    assertFalse(error.hidden);

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
      'sends the first local images that successfully load thumbnails',
      async () => {
        // Set up store data. Local image list is loaded, but thumbnails are
        // still loading in.
        personalizationStore.data.loading.local.images = false;
        personalizationStore.data.local.images = [];
        for (let i = 0; i < kMaximumLocalImagePreviews * 2; i++) {
          personalizationStore.data.local.images.push(
              {id: {high: BigInt(i * 2), low: BigInt(i)}, name: `local-${i}`});
          personalizationStore.data.loading.local.data[`${i * 2},${i}`] = true;
        }
        // Collections are finished loading.
        personalizationStore.data.backdrop.collections =
            wallpaperProvider.collections;
        personalizationStore.data.loading.collections = false;

        let {sendLocalImages, sendLocalImageData} =
            promisifyIframeFunctionsForTesting();

        wallpaperCollectionsElement = initElement(WallpaperCollections.is);

        await sendLocalImages;

        // No thumbnails loaded so none sent.
        assertEquals(0, wallpaperCollectionsElement.sentLocalImages_.size);

        // First thumbnail loads in.
        personalizationStore.data.loading.local.data = {'0,0': false};
        personalizationStore.data.local.data = {'0,0': 'local_data_0'};
        personalizationStore.notifyObservers();

        // This thumbnail should have just loaded in.
        let sent = await sendLocalImageData;
        assertDeepEquals(
            ['0,0'], Array.from(wallpaperCollectionsElement.sentLocalImages_));
        assertEquals('0,0', unguessableTokenToString(sent[1].id));
        assertEquals('local_data_0', sent[2]);

        sendLocalImageData =
            promisifyIframeFunctionsForTesting().sendLocalImageData;

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

        sent = await sendLocalImageData;
        // '4,2' successfully loaded and '2,1' did not. '4,2' should have been
        // sent to iframe.
        assertDeepEquals(
            ['0,0', '4,2'],
            Array.from(wallpaperCollectionsElement.sentLocalImages_));

        assertEquals('4,2', unguessableTokenToString(sent[1].id));
        assertEquals('local_data_2', sent[2]);
      });
}
