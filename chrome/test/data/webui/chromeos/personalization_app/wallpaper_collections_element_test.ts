// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, kDefaultImageSymbol, WallpaperActionName, WallpaperCollections} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
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
