// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {beginLoadRecentSeaPenImagesAction, beginSearchSeaPenThumbnailsAction, getRecentSeaPenImages, getSeaPenStore, SeaPenState, SeaPenStoreAdapter, SeaPenStoreInterface, searchSeaPenThumbnails, setRecentSeaPenImagesAction, setSeaPenThumbnailsAction, setThumbnailResponseStatusCodeAction} from 'chrome://personalization/js/personalization_app.js';
import {MantaStatusCode} from 'chrome://resources/ash/common/sea_pen/sea_pen.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals} from 'chrome://webui-test/chai_assert.js';

import {filterAndFlattenState, typeCheck} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPen reducers', () => {
  let seaPenProvider: TestSeaPenProvider;
  let personalizationStore: TestPersonalizationStore;
  let seaPenStore: SeaPenStoreInterface;

  setup(() => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});
    seaPenProvider = new TestSeaPenProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
    personalizationStore.replaceSingleton();
    SeaPenStoreAdapter.initSeaPenStore();
    seaPenStore = getSeaPenStore();
  });

  test('sets recent sea pen images in store', async () => {
    await getRecentSeaPenImages(seaPenProvider, seaPenStore);

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
    await searchSeaPenThumbnails(query, seaPenProvider, seaPenStore);
    assertDeepEquals(
        [
          beginSearchSeaPenThumbnailsAction(query),
          setThumbnailResponseStatusCodeAction(MantaStatusCode.kOk),
          setSeaPenThumbnailsAction(query, seaPenProvider.images),
        ],
        personalizationStore.actions, 'expected actions match');

    assertDeepEquals(
        [
          {
            'wallpaper.seaPen': typeCheck<SeaPenState>({
              loading: {
                recentImageData: {},
                recentImages: false,
                thumbnails: true,
                currentSelected: false,
                setImage: 0,
              },
              recentImageData: {},
              recentImages: null,
              thumbnailResponseStatusCode: null,
              thumbnails: null,
              pendingSelected: null,
              currentSelected: null,
              shouldShowSeaPenTermsOfServiceDialog: false,
            }),
          },
          {
            'wallpaper.seaPen': typeCheck<SeaPenState>({
              loading: {
                recentImageData: {},
                recentImages: false,
                thumbnails: true,
                currentSelected: false,
                setImage: 0,
              },
              recentImageData: {},
              recentImages: null,
              thumbnailResponseStatusCode: MantaStatusCode.kOk,
              thumbnails: null,
              pendingSelected: null,
              currentSelected: null,
              shouldShowSeaPenTermsOfServiceDialog: false,
            }),
          },
          {
            'wallpaper.seaPen': typeCheck<SeaPenState>({
              loading: {
                recentImageData: {},
                recentImages: false,
                thumbnails: false,
                currentSelected: false,
                setImage: 0,
              },
              recentImageData: {},
              recentImages: null,
              thumbnailResponseStatusCode: MantaStatusCode.kOk,
              thumbnails: seaPenProvider.images,
              pendingSelected: null,
              currentSelected: null,
              shouldShowSeaPenTermsOfServiceDialog: false,
            }),
          },
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['wallpaper.seaPen'])),
        'expected states match');
  });
});
