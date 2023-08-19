// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {Paths, PersonalizationRouter, WallpaperSubpage} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';
import {TestMock} from 'chrome://webui-test/test_mock.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

suite('WallpaperSubpageTest', function() {
  let wallpaperSubpage: WallpaperSubpage|null = null;
  let personalizationStore: TestPersonalizationStore;
  const routerOriginal = PersonalizationRouter.instance;
  const routerMock = TestMock.fromClass(PersonalizationRouter);

  setup(function() {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    PersonalizationRouter.instance = () => routerMock;
  });

  teardown(async () => {
    await teardownElement(wallpaperSubpage);
    wallpaperSubpage = null;
    PersonalizationRouter.instance = routerOriginal;
  });

  [Paths.COLLECTIONS, Paths.GOOGLE_PHOTOS_COLLECTION].forEach(
      path => test('shows or hides Google Photos collection', async () => {
        loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});
        wallpaperSubpage = initElement(WallpaperSubpage, {path: path});
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSubpage);

        // Check whether Google Photos collection is displayed.
        const googlePhotosCollections =
            wallpaperSubpage!.shadowRoot!.querySelector(
                'google-photos-collection');
        assertEquals(
            !!googlePhotosCollections, path === Paths.GOOGLE_PHOTOS_COLLECTION);
      }));

  test('hides Google Photos collection for ineligible users', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: false});
    wallpaperSubpage =
        initElement(WallpaperSubpage, {path: Paths.GOOGLE_PHOTOS_COLLECTION});
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSubpage);

    // Ineligible users can't view Google Photos collection.
    const googlePhotosCollections =
        wallpaperSubpage!.shadowRoot!.querySelector('google-photos-collection');
    assertFalse(!!googlePhotosCollections);
  });
});
