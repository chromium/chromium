// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {emptyState, GooglePhotosEnablementState, kDefaultImageSymbol, PersonalizationRouter, WallpaperActionName, WallpaperCollection, WallpaperCollections, WallpaperGridItem, WallpaperImage} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
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

  // A simplified representation of WallpaperCollectionElement tile for
  // testing.
  interface Tile {
    id: string;
    type: string;
  }
  function getTiles(): Tile[] {
    // Access a private field for testing because iron-list hides elements
    // that are out of the viewport. Pick just id and type fields for
    // simpler testing.
    return (wallpaperCollectionsElement as any)
        .tiles_.map(({id, type}: Tile) => ({id, type}));
  }

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
            '_time_of_day_chromebook_collection': wallpaperProvider.images,
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
            '_time_of_day_chromebook_collection': false,
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

  for (const isTimeOfDayWallpaperEnabled of [false, true]) {
    test(`tile order time of day ${isTimeOfDayWallpaperEnabled}`, async () => {
      loadTimeData.overrideValues({isTimeOfDayWallpaperEnabled});
      const timeOfDayCollectionId =
          loadTimeData.getString('timeOfDayWallpaperCollectionId');

      personalizationStore.data = emptyState();
      // Local images are still loading.
      personalizationStore.data.wallpaper.loading.local.images = true;

      wallpaperCollectionsElement = initElement(WallpaperCollections);
      await waitAfterNextRender(wallpaperCollectionsElement);

      let tiles = getTiles();
      assertGE(tiles.length, 6, 'minimum 3 wide by 2 high');
      for (const tile of tiles) {
        assertEquals('loading', tile.type, 'all elements are loading tile');
      }

      // Local tile loads.
      personalizationStore.data.wallpaper.local.images = [kDefaultImageSymbol];
      personalizationStore.data.wallpaper.local.data = {
        [kDefaultImageSymbol]: {
          url: createSvgDataUrl(kDefaultImageSymbol.toString()),
        },
      };
      personalizationStore.data.wallpaper.loading.local.images = false;
      personalizationStore.data.wallpaper.loading.local
          .data = {[kDefaultImageSymbol]: false};
      // Google photos tile loads.
      personalizationStore.data.wallpaper.googlePhotos.enabled =
          GooglePhotosEnablementState.kEnabled;
      personalizationStore.data.wallpaper.loading.googlePhotos.enabled = false;
      personalizationStore.notifyObservers();
      await waitAfterNextRender(wallpaperCollectionsElement);

      tiles = getTiles();
      let expectedTiles: Tile[] = [];
      if (isTimeOfDayWallpaperEnabled) {
        expectedTiles.push({id: timeOfDayCollectionId, type: 'loading'});
      }
      expectedTiles.push(
          {id: 'local_', type: 'image_local'},
          {id: 'google_photos_', type: 'image_google_photos'});
      // All loading tiles with sequential backdrop temporary ids.
      expectedTiles.push(...Array.from(
          {length: tiles.length - expectedTiles.length},
          (_, i) => ({id: `backdrop_collection_${i}`, type: 'loading'})));

      assertDeepEquals(
          expectedTiles, tiles,
          'first special tiles should match and all loading after that');

      personalizationStore.data.wallpaper.backdrop.collections =
          wallpaperProvider.collections;

      if (!isTimeOfDayWallpaperEnabled) {
        personalizationStore.data.wallpaper.backdrop.collections =
            personalizationStore.data.wallpaper.backdrop.collections!.filter(
                collection => collection.id !==
                    loadTimeData.getString('timeOfDayWallpaperCollectionId'));
      }

      personalizationStore.data.wallpaper.backdrop.images =
          personalizationStore.data.wallpaper.backdrop.collections!.reduce(
              (result, next) => {
                result[next.id] = wallpaperProvider.images;
                return result;
              },
              {} as Record<string, WallpaperImage[]|null>);
      personalizationStore.data.wallpaper.loading.collections = false;
      personalizationStore.data.wallpaper.loading.images =
          personalizationStore.data.wallpaper.backdrop.collections!.reduce(
              (result, next) => {
                result[next.id] = false;
                return result;
              },
              {} as Record<string, boolean>);
      personalizationStore.notifyObservers();
      await waitAfterNextRender(wallpaperCollectionsElement);

      tiles = getTiles();
      expectedTiles = [];
      if (isTimeOfDayWallpaperEnabled) {
        expectedTiles.push({
          id: loadTimeData.getString('timeOfDayWallpaperCollectionId'),
          type: 'image_online',
        });
      }
      expectedTiles.push(
          {id: 'local_', type: 'image_local'},
          {id: 'google_photos_', type: 'image_google_photos'});
      // Loading tiles truncated to match correct number of collections, and
      // switched to correct id and type image_online.
      expectedTiles.push(
          ...personalizationStore.data.wallpaper.backdrop.collections!
              .filter(
                  collection => collection.id !==
                      loadTimeData.getString('timeOfDayWallpaperCollectionId'))
              .map(collection => ({id: collection.id, type: 'image_online'})));

      assertDeepEquals(expectedTiles, tiles, 'tiles expected to match');
    });
  }

  test('no error reopening wallpaper subpage', async () => {
    // Wallpaper collections are loaded when first navigating to the
    // wallpaper subpage. First the list of collections, then each
    // collection, is requested from server - the component somewhat relies
    // on this order to render correctly. Test what happens when user
    // navigates to, then away from, and back to the wallpaper collections
    // subpage. This begins reloading wallpaper while existing wallpaper
    // data is already populated.

    // Needs a lot of collections to reproduce the error - there must be more
    // wallpaper collections than tiles that fit on the screen.
    const generatedCollections: WallpaperCollection[] =
        Array.from({length: 20}, (i: number) => ({
                                   id: `generated_collection_${i}`,
                                   name: `Generated Collection ${i}`,
                                   descriptionContent: '',
                                   previews: [{url: createSvgDataUrl(`${i}`)}],
                                 }));
    wallpaperProvider.setCollections([
      ...wallpaperProvider.collections!,
      ...generatedCollections,
    ]);

    loadTimeData.overrideValues({isTimeOfDayWallpaperEnabled: true});

    personalizationStore.setReducersEnabled(true);
    personalizationStore.expectAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);

    wallpaperCollectionsElement = initElement(WallpaperCollections);

    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);

    const expectedTilesAfterLoading = [
      {
        id: '_time_of_day_chromebook_collection',
        type: 'image_online',
      },
      {id: 'local_', type: 'image_local'},
      {id: 'google_photos_', 'type': 'loading'},
      {id: 'id_0', type: 'image_online'},
      {id: 'id_1', type: 'image_online'},
      {id: 'id_2', type: 'image_online'},
      ...generatedCollections.map(({id}) => ({id, type: 'image_online'})),
    ];

    assertDeepEquals(
        expectedTilesAfterLoading, getTiles(), 'expected tiles should match');

    await teardownElement(wallpaperCollectionsElement);

    // Do not use initElement because it flushes startup tasks, but test
    // needs to verify an initial state.
    wallpaperCollectionsElement =
        document.createElement(WallpaperCollections.is) as
            WallpaperCollections &
        HTMLElement;

    // Sets up loading tiles again.
    assertDeepEquals(
        [
          {id: '_time_of_day_chromebook_collection', type: 'loading'},
          {id: 'local_', type: 'loading'},
          {id: 'google_photos_', type: 'loading'},
        ],
        getTiles().slice(0, 3), 'first special tiles should match');
    assertGE(getTiles().length, 6, 'at least 6 tiles at first');
    getTiles().slice(3).forEach((tile, i) => {
      assertDeepEquals({id: `backdrop_collection_${i}`, type: 'loading'}, tile);
    });

    // Put the element on the page and wait for network requests to re-fetch
    // wallpaper collections.
    personalizationStore.expectAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);
    document.body.appendChild(wallpaperCollectionsElement);
    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);

    assertDeepEquals(
        expectedTilesAfterLoading, getTiles(),
        'expected tiles match the second time');
  });
});
