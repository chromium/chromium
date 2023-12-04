// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {beginLoadRecentSeaPenImagesAction, beginSearchSeaPenThumbnailsAction, getRecentSeaPenImages, SeaPenState, searchSeaPenThumbnails, setRecentSeaPenImagesAction, setSeaPenThumbnailsAction} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

import {filterAndFlattenState, typeCheck} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPen reducers', () => {
  let seaPenProvider: TestSeaPenProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    seaPenProvider = new TestSeaPenProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  test('sets recent sea pen images in store', async () => {
    await getRecentSeaPenImages(seaPenProvider, personalizationStore);

    assertDeepEquals(
        [
          beginLoadRecentSeaPenImagesAction(),
          setRecentSeaPenImagesAction(seaPenProvider.recentImages),
        ],
        personalizationStore.actions, 'recent images action');

    assertDeepEquals(
        seaPenProvider.recentImages,
        personalizationStore.data.wallpaper.seaPen.recentImages,
        'recent images set in store');
  });

  test('sets sea pen thumbnails in store', async () => {
    const query = {textQuery: 'test_query'};
    await searchSeaPenThumbnails(query, seaPenProvider, personalizationStore);
    assertDeepEquals(
        [
          beginSearchSeaPenThumbnailsAction(query),
          setSeaPenThumbnailsAction(query, seaPenProvider.images),
        ],
        personalizationStore.actions, 'expected actions match');

    assertDeepEquals(
        [
          {
            'wallpaper.seaPen': typeCheck<SeaPenState>({
              recentImages: null,
              recentImageData: {},
              thumbnails: null,
              thumbnailsLoading: true,
            }),
          },
          {
            'wallpaper.seaPen': typeCheck<SeaPenState>({
              recentImages: null,
              recentImageData: {},
              thumbnails: seaPenProvider.images,
              thumbnailsLoading: false,
            }),
          },
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['wallpaper.seaPen'])),
        'expected states match');
  });
});
