// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://personalization/trusted/file_path.mojom-lite.js';
import 'chrome://personalization/trusted/personalization_app.mojom-lite.js';
import {fetchLocalData, initializeBackdropData, initializeGooglePhotosData, selectWallpaper} from 'chrome://personalization/trusted/personalization_controller.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

/**
 * Get a sub-property in obj. Splits on '.'
 * @param {!Object} obj
 * @param {string} key
 */
function getProperty(obj, key) {
  let ref = obj;
  for (const part of key.split('.')) {
    ref = ref[part];
  }
  return ref;
}

/**
 * Returns a function that returns only nested subproperties in state.
 * @param {!Array<string>} keys
 * @return {!Function}
 */
function filterAndFlattenState(keys) {
  return (state) => {
    const result = {};
    for (const key of keys) {
      result[key] = getProperty(state, key);
    }
    return result;
  };
}

suite('Updating local images', () => {
  let wallpaperProvider;
  let personalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  test('Initializes Google Photos data in store', async () => {
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    assertDeepEquals(
        [
          {
            name: 'begin_load_google_photos_count',
          },
          {
            name: 'set_google_photos_count',
            count: 1000,
          },
          {
            name: 'begin_load_google_photos_albums',
          },
          {
            name: 'begin_load_google_photos_photos',
          },
          {
            name: 'set_google_photos_albums',
            albums: [],
          },
          {
            name: 'set_google_photos_photos',
            photos: Array.from({length: 1000}),
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // BEGIN_LOAD_GOOGLE_PHOTOS_COUNT.
          {
            'loading.googlePhotos': {
              count: true,
              albums: false,
              photos: false,
            },
            googlePhotos: {
              count: undefined,
              albums: undefined,
              photos: undefined,
            },
          },
          // SET_GOOGLE_PHOTOS_COUNT.
          {
            'loading.googlePhotos': {
              count: false,
              albums: false,
              photos: false,
            },
            googlePhotos: {
              count: 1000,
              albums: undefined,
              photos: undefined,
            },
          },
          // BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS.
          {
            'loading.googlePhotos': {
              count: false,
              albums: true,
              photos: false,
            },
            googlePhotos: {
              count: 1000,
              albums: undefined,
              photos: undefined,
            },
          },
          // BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS.
          {
            'loading.googlePhotos': {
              count: false,
              albums: true,
              photos: true,
            },
            googlePhotos: {
              count: 1000,
              albums: undefined,
              photos: undefined,
            },
          },
          // SET_GOOGLE_PHOTOS_ALBUMS.
          {
            'loading.googlePhotos': {
              count: false,
              albums: false,
              photos: true,
            },
            googlePhotos: {
              count: 1000,
              albums: [],
              photos: undefined,
            },
          },
          // SET_GOOGLE_PHOTOS_PHOTOS.
          {
            'loading.googlePhotos': {
              count: false,
              albums: false,
              photos: false,
            },
            googlePhotos: {
              count: 1000,
              albums: [],
              photos: Array.from({length: 1000}),
            },
          },
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['googlePhotos', 'loading.googlePhotos'])));
  });

  test('sets local images in store', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    assertDeepEquals(
        [
          {name: 'begin_load_local_images'},
          {
            name: 'set_local_images',
            images: [
              {path: 'LocalImage0.png'},
              {path: 'LocalImage1.png'},
            ],
          },
          {name: 'begin_load_local_image_data', id: 'LocalImage0.png'},
          {name: 'begin_load_local_image_data', id: 'LocalImage1.png'},
          {
            name: 'set_local_image_data',
            id: 'LocalImage0.png',
            data: 'data://localimage0data',
          },
          {
            name: 'set_local_image_data',
            id: 'LocalImage1.png',
            data: 'data://localimage1data',
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // Begin loading local image list.
          {
            'loading.local': {images: true, data: {}},
            local: {images: null, data: {}}
          },
          // Done loading local image data.
          {
            'loading.local': {data: {}, images: false},
            local: {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {}
            }
          },
          // Mark image 0 as loading.
          {
            'loading.local': {
              data: {'LocalImage0.png': true},
              images: false,
            },
            local: {
              images: [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}],
              data: {},
            },
          },
          // Mark image 1 as loading.
          {
            'loading.local': {
              data: {'LocalImage0.png': true, 'LocalImage1.png': true},
              images: false,
            },
            local: {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {},
            }
          },
          // Finish loading image 0.
          {
            'loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': true},
              images: false,
            },
            local: {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {'LocalImage0.png': 'data://localimage0data'},
            }
          },
          // Finish loading image 1.
          {
            'loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': false},
              images: false,
            },
            local: {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {
                'LocalImage0.png': 'data://localimage0data',
                'LocalImage1.png': 'data://localimage1data',
              },
            }
          }
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['local', 'loading.local'])));
  });

  test('subtracts an image from state when it disappears', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Keep the current state but reset the history of actions and states.
    personalizationStore.reset(personalizationStore.data);

    // Only keep the first image.
    wallpaperProvider.localImages = [wallpaperProvider.localImages[0]];
    await fetchLocalData(wallpaperProvider, personalizationStore);

    assertDeepEquals(
        [
          {name: 'begin_load_local_images'},
          {
            name: 'set_local_images',
            images: [{path: 'LocalImage0.png'}],
          },
        ],
        personalizationStore.actions,
    );

    assertDeepEquals(
        [
          // Begin loading new image list.
          {
            'loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': false},
              images: true,
            },
            local: {
              images: [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}],
              data: {
                'LocalImage0.png': 'data://localimage0data',
                'LocalImage1.png': 'data://localimage1data',
              },
            },
          },
          // Load new image list with only image 0. Deletes image 1 information
          // from loading state and thumbnail state.
          {
            'loading.local': {data: {'LocalImage0.png': false}, images: false},
            local: {
              images: [{path: 'LocalImage0.png'}],
              data: {'LocalImage0.png': 'data://localimage0data'},
            },
          },
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['local', 'loading.local'])));
  });

  test('fetches new images that are added', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    // Subtract image 1 and add NewPath.png.
    wallpaperProvider.localImages = [
      wallpaperProvider.localImages[0],
      {path: 'NewPath.png'},
    ];

    wallpaperProvider.localImageData = {
      ...wallpaperProvider.localImageData,
      'NewPath.png': 'data://newpath',
    };

    await fetchLocalData(wallpaperProvider, personalizationStore);

    assertDeepEquals(
        [
          {
            name: 'begin_load_local_images',
          },
          {
            name: 'set_local_images',
            images: [{path: 'LocalImage0.png'}, {path: 'NewPath.png'}],
          },
          // Only loads data for new image.
          {
            name: 'begin_load_local_image_data',
            id: 'NewPath.png',
          },
          // Sets data for new image.
          {
            name: 'set_local_image_data',
            id: 'NewPath.png',
            data: 'data://newpath',
          }
        ],
        personalizationStore.actions,
    );

    assertDeepEquals(
        [
          // Begin loading image list.
          {
            'loading.local': {
              'data': {'LocalImage0.png': false, 'LocalImage1.png': false},
              'images': true,
            },
            'local': {
              'images': [
                {'path': 'LocalImage0.png'},
                {'path': 'LocalImage1.png'},
              ],
              'data': {
                'LocalImage0.png': 'data://localimage0data',
                'LocalImage1.png': 'data://localimage1data',
              },
            },
          },
          // Done loading image list.
          {
            'loading.local': {
              'data': {'LocalImage0.png': false},
              'images': false,
            },
            'local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {'LocalImage0.png': 'data://localimage0data'},
            },
          },
          // Begin loading NewPath.png data.
          {
            'loading.local': {
              'data': {'LocalImage0.png': false, 'NewPath.png': true},
              'images': false,
            },
            'local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {'LocalImage0.png': 'data://localimage0data'},
            },
          },
          // Done loading NewPath.png data.
          {
            'loading.local': {
              'data': {'LocalImage0.png': false, 'NewPath.png': false},
              'images': false,
            },
            'local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {
                'LocalImage0.png': 'data://localimage0data',
                'NewPath.png': 'data://newpath',
              },
            },
          }
        ],
        personalizationStore.states.map(
            filterAndFlattenState(['local', 'loading.local'])));
  });

  test('clears local images when fetching new image list fails', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    wallpaperProvider.localImages = null;
    await fetchLocalData(wallpaperProvider, personalizationStore);

    assertEquals(null, personalizationStore.data.local.images);
    assertDeepEquals({}, personalizationStore.data.local.data);
    assertDeepEquals({}, personalizationStore.data.loading.local.data);
  });
});

suite('full screen mode', () => {
  const fullscreenPreviewFeature = 'fullScreenPreviewEnabled';

  let wallpaperProvider;
  let personalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
    loadTimeData.data = {[fullscreenPreviewFeature]: true};
  });

  test(
      'enters full screen mode when in tablet and preview flag is set',
      async () => {
        await initializeBackdropData(wallpaperProvider, personalizationStore);

        assertFalse(personalizationStore.data.fullscreen);

        loadTimeData.overrideValues({[fullscreenPreviewFeature]: false});
        wallpaperProvider.isInTabletModeResponse = true;

        {
          const selectWallpaperPromise = selectWallpaper(
              wallpaperProvider.images[0], wallpaperProvider,
              personalizationStore);
          const [assetId, previewMode] =
              await wallpaperProvider.whenCalled('selectWallpaper');
          assertFalse(previewMode);
          assertEquals(wallpaperProvider.images[0].assetId, assetId);

          await selectWallpaperPromise;

          assertFalse(personalizationStore.data.fullscreen);
        }

        wallpaperProvider.reset();

        {
          // Now with flag turned on.
          loadTimeData.overrideValues({[fullscreenPreviewFeature]: true});

          const selectWallpaperPromise = selectWallpaper(
              wallpaperProvider.images[0], wallpaperProvider,
              personalizationStore);

          const [assetId, previewMode] =
              await wallpaperProvider.whenCalled('selectWallpaper');
          assertTrue(previewMode);
          assertEquals(wallpaperProvider.images[0].assetId, assetId);

          await selectWallpaperPromise;

          assertTrue(personalizationStore.data.fullscreen);
        }
      });
});
