// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {kMaximumGooglePhotosPreviews, kMaximumLocalImagePreviews} from 'chrome://personalization/common/constants.js';
import {IFrameApi} from 'chrome://personalization/trusted/iframe_api.js';
import {GooglePhotosPhoto} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {emptyState} from 'chrome://personalization/trusted/personalization_state.js';
import {WallpaperCollections} from 'chrome://personalization/trusted/wallpaper/wallpaper_collections_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {assertWindowObjectsEqual, baseSetup, initElement, setupTestIFrameApi, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function WallpaperCollectionsTest() {
  let wallpaperCollectionsElement: WallpaperCollections|null = null;

  let wallpaperProvider: TestWallpaperProvider;

  let personalizationStore: TestPersonalizationStore;

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
    const testProxy = setupTestIFrameApi();
    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false
    };
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    const [target, data] = await testProxy.whenCalled('sendCollections') as
        Parameters<IFrameApi['sendCollections']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot!.querySelector('iframe');
    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, target);
    assertDeepEquals(wallpaperProvider!.collections, data);
  });

  test('sends Google Photos count when loaded', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.googlePhotos.count = 1234;
    personalizationStore.data.wallpaper.loading.googlePhotos.count = false;
    personalizationStore.notifyObservers();

    // Wait for |sendGooglePhotosCount| to be called.
    const [target, data] =
        await testProxy.whenCalled('sendGooglePhotosCount') as
        Parameters<IFrameApi['sendGooglePhotosCount']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot!.querySelector('iframe');
    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, target);
    assertDeepEquals(
        personalizationStore.data.wallpaper.googlePhotos.count, data);
  });

  test('sends Google Photos photos when loaded', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.googlePhotos.photos =
        Array.from({length: kMaximumGooglePhotosPreviews + 1})
            .map((_, i) => ({url: {url: `foo://${i}`}}));
    personalizationStore.data.wallpaper.loading.googlePhotos.photos = false;
    personalizationStore.notifyObservers();

    // Wait for |sendGooglePhotosPhotos| to be called.
    const [target, data] =
        await testProxy.whenCalled('sendGooglePhotosPhotos') as
        Parameters<IFrameApi['sendGooglePhotosPhotos']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot!.querySelector('iframe');
    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, target);
    assertDeepEquals(
        personalizationStore.data.wallpaper.googlePhotos.photos
            .slice(0, kMaximumGooglePhotosPreviews)
            .map((googlePhoto: GooglePhotosPhoto) => googlePhoto.url),
        data);
  });

  test('sends image counts when a collection loads', async () => {
    personalizationStore.data.wallpaper.backdrop = {
      collections: wallpaperProvider.collections,
      images: {},
    };
    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      images: {},
    };

    wallpaperCollectionsElement = initElement(WallpaperCollections);
    // Wait for initial load to complete.
    const testProxy = setupTestIFrameApi();
    await testProxy.whenCalled('sendImageCounts');

    testProxy.resetResolver('sendImageCounts');
    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': [wallpaperProvider.images![0]]
    };
    personalizationStore.data.wallpaper.loading.images = {'id_0': false};
    personalizationStore.notifyObservers();

    let counts =
        (await testProxy.whenCalled('sendImageCounts') as
         Parameters<IFrameApi['sendImageCounts']>)[1];
    assertDeepEquals({'id_0': 1}, counts);

    // Load two collections in at once, and simulate one failure.
    testProxy.resetResolver('sendImageCounts');

    personalizationStore.data.wallpaper.backdrop.images = {
      'id_0': [wallpaperProvider.images![0]],
      'id_1': [wallpaperProvider.images![0], wallpaperProvider.images![1]],
      'id_2': [],
      'id_3': null,
      'id_4': [wallpaperProvider.images![0], wallpaperProvider.images![2]],
    };
    personalizationStore.data.wallpaper.loading.images = {
      'id_0': false,
      'id_1': false,
      'id_2': false,
      'id_3': false,
      'id_4': false,
    };
    personalizationStore.notifyObservers();

    counts =
        (await testProxy.whenCalled('sendImageCounts') as
         Parameters<IFrameApi['sendImageCounts']>)[1];
    assertDeepEquals(
        {'id_0': 1, 'id_1': 2, 'id_2': 0, 'id_3': null, 'id_4': 1}, counts);
  });

  test('sends local images when loaded', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false}
    };
    personalizationStore.data.wallpaper.local.images =
        wallpaperProvider.localImages;
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.notifyObservers();

    // Wait for |sendLocalImages| to be called.
    const [target, data] = await testProxy.whenCalled('sendLocalImages') as
        Parameters<IFrameApi['sendLocalImages']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot!.querySelector('iframe');
    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, target);
    assertDeepEquals(wallpaperProvider.localImages, data);
  });

  test('sends collections and local images when no internet', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false}
    };
    personalizationStore.data.wallpaper.local.images =
        wallpaperProvider.localImages;
    // Simulate online collections failed to load when no internet connection.
    personalizationStore.data.wallpaper.backdrop.collections = null;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    let [target, data] = await testProxy.whenCalled('sendCollections') as
        Parameters<IFrameApi['sendCollections']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const iframe =
        wallpaperCollectionsElement.shadowRoot!.querySelector('iframe');
    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, target);
    assertEquals(null, data);

    // Wait for |sendLocalImages| to be called.
    let [imageTarget, imageData] =
        await testProxy.whenCalled('sendLocalImages') as
        Parameters<IFrameApi['sendLocalImages']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(iframe!.hidden);

    assertWindowObjectsEqual(iframe!.contentWindow, imageTarget);
    assertDeepEquals(wallpaperProvider.localImages, imageData);
  });

  test('shows error when fails to load', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections);

    // No error displayed while loading.
    let error = wallpaperCollectionsElement.shadowRoot!.querySelector(
        'wallpaper-error');
    assertTrue(error === null);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false},
    };
    personalizationStore.data.wallpaper.backdrop.collections = null;
    personalizationStore.data.wallpaper.local.images = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    error = wallpaperCollectionsElement.shadowRoot!.querySelector(
        'wallpaper-error');
    assertTrue(!!error);

    // Iframe should be hidden if there is an error.
    assertTrue(wallpaperCollectionsElement.shadowRoot!.querySelector(
                                                          'iframe')!.hidden);
  });

  test('loads backdrop data and saves to store', async () => {
    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);

    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    const [_, collections] = await testProxy.whenCalled('sendCollections') as
        Parameters<IFrameApi['sendCollections']>;
    assertDeepEquals(wallpaperProvider.collections, collections);

    assertDeepEquals(
        {
          collections: wallpaperProvider.collections,
          images: {
            'id_0': wallpaperProvider.images,
            'id_1': wallpaperProvider.images,
          },
        },
        personalizationStore.data.wallpaper.backdrop,
    );
    assertDeepEquals(
        {
          ...emptyState().wallpaper.loading,
          collections: false,
          images: {
            'id_0': false,
            'id_1': false,
          },
        },
        personalizationStore.data.wallpaper.loading,
    );
  });

  test(
      'sends the first three local images that successfully load thumbnails',
      async () => {
        // Set up store data. Local image list is loaded, but thumbnails are
        // still loading in.
        personalizationStore.data.wallpaper.loading.local.images = false;
        personalizationStore.data.wallpaper.local.images = [];
        for (let i = 0; i < kMaximumLocalImagePreviews; i++) {
          const path = `LocalImage${i}.png`;
          personalizationStore.data.wallpaper.local.images.push({path});
          personalizationStore.data.wallpaper.loading.local.data[path] = true;
        }
        // Collections are finished loading.
        personalizationStore.data.wallpaper.backdrop.collections =
            wallpaperProvider.collections;
        personalizationStore.data.wallpaper.loading.collections = false;

        const testProxy = setupTestIFrameApi();

        wallpaperCollectionsElement = initElement(WallpaperCollections);

        await testProxy.whenCalled('sendLocalImages');

        // No thumbnails loaded so none sent.
        assertFalse(wallpaperCollectionsElement['didSendLocalImageData_']);

        // First thumbnail loads in.
        personalizationStore.data.wallpaper.loading.local.data = {
          'LocalImage0.png': false
        };
        personalizationStore.data.wallpaper.local.data = {
          'LocalImage0.png': 'local_data_0'
        };
        personalizationStore.notifyObservers();

        await wallpaperCollectionsElement['iframePromise_'];
        await waitAfterNextRender(wallpaperCollectionsElement);

        // Should not have sent any image data since more thumbnails are still
        // loading.
        assertFalse(wallpaperCollectionsElement['didSendLocalImageData_']);

        // Second thumbnail fails loading. Third succeeds.
        personalizationStore.data.wallpaper.loading.local.data = {
          ...personalizationStore.data.wallpaper.loading.local.data,
          'LocalImage1.png': false,
          'LocalImage2.png': false,
        };
        personalizationStore.data.wallpaper.local.data = {
          ...personalizationStore.data.wallpaper.local.data,
          'LocalImage1.png': '',
          'LocalImage2.png': 'local_data_2',
        };
        personalizationStore.notifyObservers();

        // 2 thumbnails have now loaded. 1 failed. But there are no more
        // remaining to try loading, should send local image data anyway.
        const [_, sentData] =
            await testProxy.whenCalled('sendLocalImageData') as
            Parameters<IFrameApi['sendLocalImageData']>;

        assertTrue(wallpaperCollectionsElement['didSendLocalImageData_']);
        assertDeepEquals(
            {
              'LocalImage0.png': 'local_data_0',
              'LocalImage1.png': '',
              'LocalImage2.png': 'local_data_2',
            },
            sentData);
      });

  test('sets aria label on collections-iframe', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertEquals(
        loadTimeData.getString('wallpaperCollections'),
        wallpaperCollectionsElement.shadowRoot
            ?.getElementById('collections-iframe')
            ?.getAttribute('aria-label'),
        'aria label equals expected value');
  });
}
