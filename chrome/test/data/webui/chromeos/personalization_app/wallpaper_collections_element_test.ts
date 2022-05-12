// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, GooglePhotosEnablementState, IFrameApi, kDefaultImageSymbol, kMaximumLocalImagePreviews, Paths, PersonalizationRouter, WallpaperActionName, WallpaperCollections} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, setupTestIFrameApi, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperCollectionsTest', function() {
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

    const main = wallpaperCollectionsElement.shadowRoot!.querySelector('main');
    assertFalse(main!.hidden);

    assertEquals(
        wallpaperCollectionsElement.$.collectionsGrid, target,
        'collections data is sent to collections-grid');
    assertDeepEquals(wallpaperProvider!.collections, data);
  });

  [GooglePhotosEnablementState.kDisabled, GooglePhotosEnablementState.kEnabled,
   GooglePhotosEnablementState.kError]
      .forEach(
          enabled => test(
              'shows managed UI when Google Photos access is disabled',
              async () => {
                // Use production |IFrameApi|.
                IFrameApi.setInstance(new IFrameApi());

                // Initialize |wallpaperCollectionsElement|.
                wallpaperCollectionsElement = initElement(WallpaperCollections);
                await waitAfterNextRender(wallpaperCollectionsElement);

                // Set Google Photos enabled.
                personalizationStore.data.wallpaper.googlePhotos.enabled =
                    enabled;
                personalizationStore.notifyObservers();

                // Set Google Photos photos.
                personalizationStore.data.wallpaper.googlePhotos.photos = null;
                personalizationStore.notifyObservers();

                // Cache |googlePhotosTile|.
                await waitAfterNextRender(wallpaperCollectionsElement);
                const googlePhotosTile =
                    wallpaperCollectionsElement.$.collectionsGrid.shadowRoot!
                        .querySelector('.google-photos-empty');
                assertNotEquals(googlePhotosTile, null);

                // Verify text expectations.
                const text = googlePhotosTile!.querySelectorAll('p');
                assertEquals(text?.[0]?.innerHTML, 'Google Photos');
                assertEquals(
                    window.getComputedStyle(text?.[1]!).visibility, 'hidden');

                // Verify icon expectations.
                const icon = googlePhotosTile!.querySelector('iron-icon');
                assertEquals(icon?.icon, 'personalization:managed');
                assertEquals(
                    getComputedStyle(icon!).getPropertyValue('display'),
                    enabled === GooglePhotosEnablementState.kDisabled ?
                        'block' :
                        'none');

                // Mock singleton |PersonalizationRouter|.
                const proxy = TestBrowserProxy.fromClass(PersonalizationRouter);
                PersonalizationRouter.instance = () => proxy;

                // Verify click expectations.
                (googlePhotosTile as HTMLElement).click();
                assertEquals(
                    proxy.getCallCount('goToRoute'),
                    enabled === GooglePhotosEnablementState.kDisabled ? 0 : 1);
                assertEquals(
                    proxy.getArgs('goToRoute')[0] ??
                        Paths.GOOGLE_PHOTOS_COLLECTION,
                    Paths.GOOGLE_PHOTOS_COLLECTION);
              }));

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

    const testProxy = setupTestIFrameApi();
    wallpaperCollectionsElement = initElement(WallpaperCollections);
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

  test('sends local images when local and default image loaded', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false, data: {[kDefaultImageSymbol]: false}}
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

    const main = wallpaperCollectionsElement.shadowRoot!.querySelector('main');
    assertFalse(main!.hidden);

    assertEquals(
        wallpaperCollectionsElement.$.collectionsGrid, target,
        'local image urls are sent to collections-grid');
    assertDeepEquals(wallpaperProvider.localImages, data);
  });

  test('default image is included in local image list', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false, data: {[kDefaultImageSymbol]: false}},
    };
    personalizationStore.data.wallpaper.local.images =
        [kDefaultImageSymbol, ...wallpaperProvider.localImages!];
    personalizationStore.data.wallpaper.local.data =
        {[kDefaultImageSymbol]: wallpaperProvider.defaultImageThumbnail};
    personalizationStore.notifyObservers();

    const [_, data] = await testProxy.whenCalled('sendLocalImages') as
        Parameters<IFrameApi['sendLocalImages']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertTrue(Array.isArray(data), 'sent local images array');
    assertEquals(
        1 + wallpaperProvider.localImages!.length, data.length,
        'Sent default image plus local images');
    assertEquals(kDefaultImageSymbol, data[0], 'default image is index 0');
  });

  test('sends collections and local images when no internet', async () => {
    const testProxy = setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    personalizationStore.data.wallpaper.loading = {
      ...personalizationStore.data.wallpaper.loading,
      collections: false,
      local: {images: false, data: {[kDefaultImageSymbol]: false}},
    };
    personalizationStore.data.wallpaper.local.images =
        wallpaperProvider.localImages;
    // Simulate online collections failed to load when no internet connection.
    personalizationStore.data.wallpaper.backdrop.collections = null;
    personalizationStore.notifyObservers();

    // Wait for |sendCollections| to be called.
    const [target, data] = await testProxy.whenCalled('sendCollections') as
        Parameters<IFrameApi['sendCollections']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    const main = wallpaperCollectionsElement.shadowRoot!.querySelector('main');
    assertFalse(main!.hidden);

    assertEquals(
        wallpaperCollectionsElement.$.collectionsGrid, target,
        'null sent to collections-grid when failed to fetch data');
    assertEquals(null, data);

    // Wait for |sendLocalImages| to be called.
    const [imageTarget, imageData] =
        await testProxy.whenCalled('sendLocalImages') as
        Parameters<IFrameApi['sendLocalImages']>;
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertFalse(main!.hidden);

    assertEquals(
        wallpaperCollectionsElement.$.collectionsGrid, imageTarget,
        'local images still sent to collections-grid when offline');
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
      local: {images: false, data: {[kDefaultImageSymbol]: false}},
    };
    personalizationStore.data.wallpaper.backdrop.collections = null;
    personalizationStore.data.wallpaper.local.images = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    error = wallpaperCollectionsElement.shadowRoot!.querySelector(
        'wallpaper-error');
    assertTrue(!!error);

    assertTrue(
        wallpaperCollectionsElement.shadowRoot!.querySelector('main')!.hidden,
        'main should be hidden if there is an error');
  });

  test('loads backdrop data and saves to store', async () => {
    // Make sure state starts at expected value.
    assertDeepEquals(emptyState(), personalizationStore.data);
    // Actually run the reducers.
    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);
    setupTestIFrameApi();

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);


    assertDeepEquals(
        {
          collections: wallpaperProvider.collections,
          images: {
            'id_0': wallpaperProvider.images,
            'id_1': wallpaperProvider.images,
            'id_2': wallpaperProvider.images,
          },
        },
        personalizationStore.data.wallpaper.backdrop,
        'expected backdrop data is set',
    );
    assertDeepEquals(
        {
          ...emptyState().wallpaper.loading,
          collections: false,
          images: {
            'id_0': false,
            'id_1': false,
            'id_2': false,
          },
        },
        personalizationStore.data.wallpaper.loading,
        'expected loading state is set',
    );
  });

  test(
      'sends the first four local images that successfully load thumbnails',
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

        await waitAfterNextRender(wallpaperCollectionsElement);

        // Should not have sent any image data since more thumbnails are still
        // loading.
        assertFalse(wallpaperCollectionsElement['didSendLocalImageData_']);

        // Second thumbnail fails loading. Third and fourth succeed.
        personalizationStore.data.wallpaper.loading.local.data = {
          ...personalizationStore.data.wallpaper.loading.local.data,
          'LocalImage1.png': false,
          'LocalImage2.png': false,
          'LocalImage3.png': false,
        };
        personalizationStore.data.wallpaper.local.data = {
          ...personalizationStore.data.wallpaper.local.data,
          'LocalImage1.png': '',
          'LocalImage2.png': 'local_data_2',
          'LocalImage3.png': 'local_data_3',
        };
        personalizationStore.notifyObservers();

        // 3 thumbnails have now loaded. 1 failed. But there are no more
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
              'LocalImage3.png': 'local_data_3',
            },
            sentData);
      });

  test('sets aria label on main', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertEquals(
        loadTimeData.getString('wallpaperCollections'),
        wallpaperCollectionsElement.shadowRoot?.querySelector('main')
            ?.getAttribute('aria-label'),
        'aria label equals expected value');
  });
});
