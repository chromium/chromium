// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {emptyState, GooglePhotosEnablementState, kDefaultImageSymbol, Paths, PersonalizationRouterElement, WallpaperActionName, WallpaperCollection, WallpaperCollectionsElement, WallpaperGridItemElement, WallpaperImage} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertGE, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, createSvgDataUrl, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperCollectionsElementTest', function() {
  let wallpaperCollectionsElement: WallpaperCollectionsElement|null = null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouterElement.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouterElement);

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

  async function loadWallpapers(isTimeOfDayWallpaperEnabled: boolean) {
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
    await waitAfterNextRender(wallpaperCollectionsElement!);
  }

  setup(function() {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
    PersonalizationRouterElement.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(wallpaperCollectionsElement);
    wallpaperCollectionsElement = null;
    PersonalizationRouterElement.instance = routerOriginal;
  });

  test('shows error when fails to load', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);

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

    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);

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
    assertEquals(
        JSON.stringify({
          ...emptyState().wallpaper.loading,
          collections: false,
          images: {
            'id_0': false,
            'id_1': false,
            'id_2': false,
            '_time_of_day_chromebook_collection': false,
          },
          local: {
            data: {
              'LocalImage0.png': false,
              'LocalImage1.png': false,
            },
            images: false,
          },
        }),
        JSON.stringify(personalizationStore.data.wallpaper.loading),
        'expected loading state is set',
    );
  });

  test('sets aria label on main', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    assertEquals(
        loadTimeData.getString('wallpaperCollections'),
        wallpaperCollectionsElement.shadowRoot?.querySelector('main')
            ?.getAttribute('aria-label'),
        'aria label equals expected value');
  });

  test('displays no_images.svg when no local images', async () => {
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItemElement>(
                              `${WallpaperGridItemElement.is}[collage]`);

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
            .querySelector<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[collage]`)
            ?.secondaryText,
        'no images text is displayed');
  });

  test('displays 1 image when default thumbnail exists', async () => {
    personalizationStore.data.wallpaper.local.images = [kDefaultImageSymbol];
    personalizationStore.data.wallpaper.local.data = {
      [kDefaultImageSymbol]: {url: 'data:image/png;base64,qqqq'},
    };

    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItemElement>(
                              `${WallpaperGridItemElement.is}[collage]`);

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

    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const localTile = wallpaperCollectionsElement.shadowRoot!
                          .querySelector<WallpaperGridItemElement>(
                              `${WallpaperGridItemElement.is}[collage]`);

    assertTrue(!!localTile, 'local tile is present');

    assertDeepEquals(
        [
          {url: 'data:image/png;base64,qqqq'},
          {url: 'data:image/png;base64,asdf'},
          {url: 'data:image/png;base64,qwer'},
        ],
        localTile.src, 'all three images are displayed');
  });

  test('no Google Photos tile for ineligible users', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: false});
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const googlePhotosTile =
        wallpaperCollectionsElement.shadowRoot!
            .querySelector<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-google-photos]`);
    assertFalse(!!googlePhotosTile, 'google photos tile is not present');
  });

  test('customizes text for managed google photos', async () => {
    const managedIconSelector = `iron-icon[icon^='personalization:managed']`;

    personalizationStore.data.wallpaper.googlePhotos.enabled =
        GooglePhotosEnablementState.kEnabled;
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const googlePhotosTile =
        wallpaperCollectionsElement.shadowRoot!
            .querySelector<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-google-photos]`);
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
    // The mock set of collections created for this test does not contain the
    // time of day collection. If time of day wallpaper happens to be enabled,
    // the test will fail because the time of day collection is missing, which
    // is irrelevant for this test case. It must explicitly be disabled here.
    loadTimeData.overrideValues({isTimeOfDayWallpaperEnabled: false});
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
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const onlineTiles = wallpaperCollectionsElement.shadowRoot!
                            .querySelectorAll<WallpaperGridItemElement>(
                                `${WallpaperGridItemElement.is}[data-online]`);

    assertEquals(2, onlineTiles.length);
    assertDeepEquals(
        ['asdf description', ''],
        Array.from(onlineTiles).map(item => item.infoText),
        'correct info text set for both online collections');
  });

  for (const isTimeOfDayWallpaperEnabled of [false, true]) {
    test(`tile order time of day ${isTimeOfDayWallpaperEnabled}`, async () => {
      loadTimeData.overrideValues({
        isTimeOfDayWallpaperEnabled: isTimeOfDayWallpaperEnabled,
        // SeaPen tile is tested seperately in later tests
        isSeaPenEnabled: false,
      });
      const timeOfDayCollectionId =
          loadTimeData.getString('timeOfDayWallpaperCollectionId');

      personalizationStore.data = emptyState();
      // Local images are still loading.
      personalizationStore.data.wallpaper.loading.local.images = true;

      wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
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

      await loadWallpapers(isTimeOfDayWallpaperEnabled);

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

    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);

    await personalizationStore.waitForAction(
        WallpaperActionName.SET_IMAGES_FOR_COLLECTION);

    const expectedTilesAfterLoading = [
      {
        id: '_time_of_day_chromebook_collection',
        type: 'image_online',
      },
      {id: 'local_', type: 'image_local'},
      {id: 'google_photos_', 'type': 'image_google_photos'},
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
        document.createElement(WallpaperCollectionsElement.is) as
            WallpaperCollectionsElement &
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

  test('no SeaPen tile for ineligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: false});
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const seaPenTile = wallpaperCollectionsElement.shadowRoot!
                           .querySelector<WallpaperGridItemElement>(
                               `${WallpaperGridItemElement.is}[data-sea-pen]`);
    assertFalse(!!seaPenTile, 'SeaPen tile is not present');
  });

  test('shows SeaPen tile for eligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const seaPenTile = wallpaperCollectionsElement.shadowRoot!
                           .querySelector<WallpaperGridItemElement>(
                               `${WallpaperGridItemElement.is}[data-sea-pen]`);
    assertTrue(!!seaPenTile, 'SeaPen tile is present');
  });

  test('click on SeaPen tile navigates to SeaPen page', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});

    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ true);
    wallpaperCollectionsElement.shadowRoot!
        .querySelector<WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}[data-sea-pen]`)
        ?.click();
    const path = await routerMock.whenCalled('goToRoute');
    assertEquals(Paths.SEA_PEN_COLLECTION, path, 'navigates to SeaPen page');
  });

  test('shows promoted tiles section with SeaPen', async () => {
    loadTimeData.overrideValues({
      isSeaPenEnabled: true,
      isSeaPenTextInputEnabled: false,
      isTimeOfDayWallpaperEnabled: true,
    });
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const loadingTiles =
        wallpaperCollectionsElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
    assertEquals(2, loadingTiles.length, 'two tiles are loading');

    await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ true);

    const promotedTiles =
        wallpaperCollectionsElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
    assertEquals(2, promotedTiles.length, 'two tiles are promoted');
    assertTrue(
        promotedTiles[0]!.hasAttribute('data-sea-pen'),
        'sea pen is in promoted tiles');
    assertTrue(
        promotedTiles[1]!.hasAttribute('data-is-time-of-day-collection'),
        'time of day is in promoted tiles');
  });

  test('shows single promoted tile section for SeaPen', async () => {
    loadTimeData.overrideValues({
      isSeaPenEnabled: true,
      isSeaPenTextInputEnabled: false,
      isTimeOfDayWallpaperEnabled: false,
    });
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const loadingTiles =
        wallpaperCollectionsElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
    assertEquals(1, loadingTiles.length, 'expected single loading tile');

    await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ false);

    const promotedTiles =
        wallpaperCollectionsElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
    assertEquals(1, promotedTiles.length, 'expected single promoted tile');
    assertTrue(
        promotedTiles[0]!.hasAttribute('data-sea-pen'),
        'expected sea pen promoted tile');

    const templatesTileTag =
        wallpaperCollectionsElement.shadowRoot!.getElementById(
            'templatesTileTag');
    assertTrue(
        !!templatesTileTag, 'expected tile tag for Sea Pen templates tile');
  });

  test(
      'shows two promoted tiles for SeaPen prompting and templates',
      async () => {
        loadTimeData.overrideValues({
          isSeaPenEnabled: true,
          isSeaPenTextInputEnabled: true,
          isTimeOfDayWallpaperEnabled: false,
        });
        wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
        await waitAfterNextRender(wallpaperCollectionsElement);

        const loadingTiles =
            wallpaperCollectionsElement.shadowRoot!
                .querySelectorAll<WallpaperGridItemElement>(
                    `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
        assertEquals(
            2, loadingTiles.length, 'expected two Sea Pen loading tiles');

        await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ false);

        const promotedTiles =
            wallpaperCollectionsElement.shadowRoot!
                .querySelectorAll<WallpaperGridItemElement>(
                    `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
        assertEquals(2, promotedTiles.length, 'expected two promoted tiles');
        assertTrue(
            promotedTiles[0]!.hasAttribute('data-sea-pen-freeform'),
            'expected 1st tile a sea pen freeform promoted tile');
        assertTrue(
            promotedTiles[1]!.hasAttribute('data-sea-pen'),
            'expected 2nd tile a sea pen templates promoted tile');
      });

  test('shows time of day tile once', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isTimeOfDayWallpaperEnabled: true});
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ true);

    const timeOfDayTile =
        wallpaperCollectionsElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(`${
                WallpaperGridItemElement.is}[data-is-time-of-day-collection]`);

    assertEquals(1, timeOfDayTile.length, 'time of day only appears once');
  });

  test('no promoted tiles section when SeaPen is disabled', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: false, isTimeOfDayWallpaperEnabled: true});
    wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
    await waitAfterNextRender(wallpaperCollectionsElement);

    const promotedTiles =
        wallpaperCollectionsElement.shadowRoot!.querySelector<HTMLElement>(
            `#promoted`);
    assertFalse(!!promotedTiles, 'promoted tiles are hidden');

    await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ true);

    const timeOfDayTile =
        wallpaperCollectionsElement.shadowRoot!
            .querySelector<WallpaperGridItemElement>(`${
                WallpaperGridItemElement.is}[data-is-time-of-day-collection]`);
    assertTrue(!!timeOfDayTile, 'time of day tile is shown');

    const templatesTileTag =
        wallpaperCollectionsElement.shadowRoot!.getElementById(
            'templatesTileTag');
    assertFalse(!!templatesTileTag, 'no templates tile tag displayed');
  });

  test(
      'disables Sea Pen promoted tile for managed users with policy disabled',
      async () => {
        loadTimeData.overrideValues({
          isSeaPenEnabled: true,
          isSeaPenTextInputEnabled: false,
          isManagedSeaPenEnabled: false,
          isTimeOfDayWallpaperEnabled: false,
        });
        wallpaperCollectionsElement = initElement(WallpaperCollectionsElement);
        await waitAfterNextRender(wallpaperCollectionsElement);

        const loadingTiles =
            wallpaperCollectionsElement.shadowRoot!
                .querySelectorAll<WallpaperGridItemElement>(
                    `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
        assertEquals(1, loadingTiles.length, 'expected single loading tile');

        await loadWallpapers(/* isTimeOfDayWallpaperEnabled= */ false);

        const promotedTiles =
            wallpaperCollectionsElement.shadowRoot!
                .querySelectorAll<WallpaperGridItemElement>(
                    `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
        assertEquals(1, promotedTiles.length, 'expected single promoted tile');
        assertTrue(
            promotedTiles[0]!.hasAttribute('data-sea-pen'),
            'expected sea pen promoted tile');
        assertEquals(
            'true', promotedTiles[0]!.getAttribute('aria-disabled'),
            'expected sea pen promoted tile is disabled');
      });
});
