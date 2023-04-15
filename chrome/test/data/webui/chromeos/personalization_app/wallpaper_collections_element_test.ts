// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, GooglePhotosEnablementState, kDefaultImageSymbol, PersonalizationRouter, WallpaperActionName, WallpaperCollections, WallpaperGridItem} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperCollectionsTest', function() {
  let wallpaperCollectionsElement: WallpaperCollections|null = null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouter.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouter);

  setup(function() {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
    PersonalizationRouter.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(wallpaperCollectionsElement);
    wallpaperCollectionsElement = null;
    PersonalizationRouter.instance = routerOriginal;
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
            'id_3': wallpaperProvider.images,
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
            'id_3': false,
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

  test('displays no_images.svg when no local images', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItem>(
                              `${WallpaperGridItem.is}[collage]`);

    assertTrue(!!localTile, 'local tile is present');

    assertDeepEquals(
        [{url: 'chrome://personalization/images/no_images.svg'}], localTile.src,
        'no local images present');

    assertEquals(
        loadTimeData.getString('myImagesLabel'), localTile.primaryText,
        'correct local tile primary text');

    assertEquals(
        loadTimeData.getString('zeroImages'),
        wallpaperCollectionsElement.shadowRoot!
            .querySelector<WallpaperGridItem>(
                `${WallpaperGridItem.is}[collage]`)
            ?.secondaryText,
        'no images text is displayed');
  });

  test('displays 1 image when default thumbnail exists', async () => {
    personalizationStore.data.wallpaper.local.images = [kDefaultImageSymbol];
    personalizationStore.data.wallpaper.local.data = {
      [kDefaultImageSymbol]: {url: 'data:image/png;base64,qqqq'},
    };

    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItem>(
                              `${WallpaperGridItem.is}[collage]`);

    assertTrue(!!localTile, 'local tile is present');

    assertDeepEquals(
        [{url: 'data:image/png;base64,qqqq'}], localTile.src,
        'default image thumbnail present');
  });

  test('mixes local images in with default image thumbnail', async () => {
    personalizationStore.data.wallpaper.local.images =
        [kDefaultImageSymbol, {path: '/asdf'}, {path: '/qwer'}];
    personalizationStore.data.wallpaper.local.data = {
      [kDefaultImageSymbol]: {url: 'data:image/png;base64,qqqq'},
      '/asdf': {url: 'data:image/png;base64,asdf'},
      '/qwer': {url: 'data:image/png;base64,qwer'},
    };
    personalizationStore.data.wallpaper.loading.local.data = {
      [kDefaultImageSymbol]: false,
      '/asdf': false,
      '/qwer': false,
    };

    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItem>(
                              `${WallpaperGridItem.is}[collage]`);

    assertTrue(!!localTile, 'local tile is present');

    assertDeepEquals(
        [
          {url: 'data:image/png;base64,qqqq'},
          {url: 'data:image/png;base64,asdf'},
          {url: 'data:image/png;base64,qwer'},
        ],
        localTile.src, 'all three images are displayed');
  });

  test('customizes text for managed google photos', async () => {
    const managedIconSelector = `iron-icon[icon='personalization:managed']`;

    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kEnabled;
    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const googlePhotosTile =
        wallpaperCollectionsElement.shadowRoot!
            .querySelector<WallpaperGridItem>(
                `${WallpaperGridItem.is}[data-google-photos]`);
    assertTrue(!!googlePhotosTile, 'google photos tile is present');
    assertEquals(
        null, googlePhotosTile.querySelector(managedIconSelector),
        'no managed icon is shown');

    // Update to managed state.
    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kDisabled;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertTrue(
        !!googlePhotosTile.querySelector(managedIconSelector),
        'managed icon now shown');
  });

  test('sets collection description text', async () => {
    wallpaperProvider.setCollections([
      {
        id: 'asdf',
        name: 'asdf name',
        descriptionContent: 'asdf description',
        previews: [{url: createSvgDataUrl('asdf')}],
      },
      {
        id: 'qwerty',
        name: 'qwerty name',
        descriptionContent: '',
        previews: [{url: createSvgDataUrl('qwerty')}],
      },
    ]);
    personalizationStore.data.wallpaper.backdrop.collections =
        wallpaperProvider.collections;
    personalizationStore.data.wallpaper.backdrop.images = {
      asdf: wallpaperProvider.images,
      qwerty: wallpaperProvider.images,
    };
    personalizationStore.data.wallpaper.loading.collections = false;
    personalizationStore.data.wallpaper.loading.images = {
      asdf: false,
      qwerty: false,
    };
    wallpaperCollectionsElement = initElement(WallpaperCollections);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const onlineTiles = wallpaperCollectionsElement.shadowRoot!
                            .querySelectorAll<WallpaperGridItem>(
                                `${WallpaperGridItem.is}[data-online]`);

    assertEquals(2, onlineTiles.length);
    assertDeepEquals(
        ['asdf description', ''],
        Array.from(onlineTiles).map(item => item.infoText),
        'correct info text set for both online collections');
  });

  test(
      'dismisses the banner after clicking on time of day collection',
      async () => {
        personalizationStore.data.ambient.shouldShowTimeOfDayBanner = true;
        personalizationStore.data.wallpaper.backdrop.collections =
            wallpaperProvider.collections;
        personalizationStore.data.wallpaper.backdrop.images = {
          [wallpaperProvider.timeOfDayCollectionId]: wallpaperProvider.images,
        };
        personalizationStore.data.wallpaper.loading.collections = false;
        personalizationStore.data.wallpaper.loading.images = {
          [wallpaperProvider.timeOfDayCollectionId]: false,
        };
        wallpaperCollectionsElement = initElement(WallpaperCollections);
        await waitAfterNextRender(wallpaperCollectionsElement);

        const onlineTiles =
            wallpaperCollectionsElement.shadowRoot!
                .querySelectorAll<WallpaperGridItem>(`${
                    WallpaperGridItem
                        .is}[data-online][data-is-time-of-day-collection]`);
        assertEquals(1, onlineTiles.length);

        personalizationStore.setReducersEnabled(true);
        onlineTiles[0]!.click();
        assertFalse(
            personalizationStore.data.ambient.shouldShowTimeOfDayBanner,
            'banner is dismissed');
      });
});
