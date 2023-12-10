// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {Paths, PersonalizationRouterElement, WallpaperCollectionsElement, WallpaperGridItemElement, WallpaperSubpageElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('WallpaperSubpageElementTest', function() {
  let wallpaperSubpage: WallpaperSubpageElement|null = null;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouterElement.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouterElement);

  setup(function() {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    PersonalizationRouterElement.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(wallpaperSubpage);
    wallpaperSubpage = null;
    PersonalizationRouterElement.instance = routerOriginal;
  });

  [Paths.COLLECTIONS, Paths.GOOGLE_PHOTOS_COLLECTION].forEach(
      path => test('shows or hides Google Photos collection', async () => {
        loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});
        wallpaperSubpage = initElement(WallpaperSubpageElement, {path: path});
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSubpage);

        // Wallpaper Selected is displayed.
        const wallpaperSubpageTopElement =
            wallpaperSubpage!.shadowRoot!.querySelector(
                'wallpaper-subpage-top');
        const wallpaperSelected =
            wallpaperSubpageTopElement!.shadowRoot!.querySelector(
                'wallpaper-selected');
        assertTrue(!!wallpaperSelected);

        // Check whether Google Photos collection is displayed.
        const googlePhotosCollections =
            wallpaperSubpage!.shadowRoot!.querySelector(
                'google-photos-collection');
        assertEquals(
            !!googlePhotosCollections, path === Paths.GOOGLE_PHOTOS_COLLECTION);
      }));

  test('hides Google Photos collection for ineligible users', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: false});
    wallpaperSubpage = initElement(
        WallpaperSubpageElement, {path: Paths.GOOGLE_PHOTOS_COLLECTION});
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSubpage);

    // Ineligible users can't view Google Photos collection.
    const googlePhotosCollections =
        wallpaperSubpage!.shadowRoot!.querySelector('google-photos-collection');
    assertFalse(!!googlePhotosCollections);
  });

  test('shows SeaPen templates', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    wallpaperSubpage =
        initElement(WallpaperSubpageElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpage);

    // SeaPen templates is displayed.
    const seaPenCollection =
        wallpaperSubpage!.shadowRoot!.querySelector('sea-pen-templates');
    assertTrue(!!seaPenCollection);
  });

  test('hides SeaPen templates for ineligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: false});
    wallpaperSubpage =
        initElement(WallpaperSubpageElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpage);

    const seaPenCollection =
        wallpaperSubpage!.shadowRoot!.querySelector('sea-pen-templates');
    assertFalse(!!seaPenCollection, 'SeaPen templates are not displayed');
  });

  test('shows SeaPen results', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    wallpaperSubpage =
        initElement(WallpaperSubpageElement, {path: Paths.SEA_PEN_RESULTS});
    await waitAfterNextRender(wallpaperSubpage);

    const seaPenImages =
        wallpaperSubpage!.shadowRoot!.querySelector('sea-pen-images');
    assertTrue(!!seaPenImages, 'SeaPen images are displayed');
  });

  test('hides SeaPen results for ineligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: false});
    wallpaperSubpage =
        initElement(WallpaperSubpageElement, {path: Paths.SEA_PEN_RESULTS});
    await waitAfterNextRender(wallpaperSubpage);

    const seaPenImages =
        wallpaperSubpage!.shadowRoot!.querySelector('sea-pen-images');
    assertFalse(!!seaPenImages, 'sea pen images are not displayed');
  });

  test('show promoted tiles for sea pen', async () => {
    loadTimeData.overrideValues(
        {isSeaPenEnabled: true, isTimeOfDayWallpaperEnabled: true});
    wallpaperSubpage =
        initElement(WallpaperSubpageElement, {path: Paths.SEA_PEN_COLLECTION});
    await waitAfterNextRender(wallpaperSubpage);

    let collections: WallpaperCollectionsElement =
        wallpaperSubpage!.shadowRoot!.querySelector('wallpaper-collections')!;
    assertTrue(!!collections && collections.hidden, 'collections are hidden');

    wallpaperSubpage.path = Paths.COLLECTIONS;
    await waitAfterNextRender(wallpaperSubpage);
    collections =
        wallpaperSubpage!.shadowRoot!.querySelector('wallpaper-collections')!;
    assertTrue(!!collections, 'collections are displayed');
    assertFalse(collections.hidden, 'collections are not hidden');
    const promotedTiles =
        collections.shadowRoot!.querySelectorAll<WallpaperGridItemElement>(
            `${WallpaperGridItemElement.is}[data-is-promoted-tile]`);
    assertEquals(2, promotedTiles.length, 'two tiles are promoted');
  });
});
