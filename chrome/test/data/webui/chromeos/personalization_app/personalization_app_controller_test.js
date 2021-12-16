// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import '../../mojo_webui_test_support.js';

import {WallpaperCollection} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import * as wallpaperAction from 'chrome://personalization/trusted/wallpaper/wallpaper_actions.js';
import {fetchCollections, fetchGooglePhotosAlbum, fetchLocalData, getLocalImages, initializeBackdropData, initializeGooglePhotosData, selectWallpaper} from 'chrome://personalization/trusted/wallpaper/wallpaper_controller.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

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

suite('Personalization app controller', () => {
  let wallpaperProvider;
  let personalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  [true, false].forEach(isGooglePhotosIntegrationEnabled => {
    test('initializes Google Photos data in store', async () => {
      loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled});

      await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

      let expectedCount, expectedAlbums, expectedPhotos;
      if (isGooglePhotosIntegrationEnabled) {
        expectedCount = 0;
        expectedAlbums = [];
        expectedPhotos = [];
      } else {
        expectedCount = null;
        expectedAlbums = null;
        expectedPhotos = null;
      }

      assertDeepEquals(
          [
            {
              name: 'begin_load_google_photos_count',
            },
            {
              name: 'set_google_photos_count',
              count: expectedCount,
            },
            {
              name: 'begin_load_google_photos_albums',
            },
            {
              name: 'begin_load_google_photos_photos',
            },
            {
              name: 'set_google_photos_albums',
              albums: expectedAlbums,
            },
            {
              name: 'set_google_photos_photos',
              photos: expectedPhotos,
            },
          ],
          personalizationStore.actions);

      assertDeepEquals(
          [
            // BEGIN_LOAD_GOOGLE_PHOTOS_COUNT.
            {
              'wallpaper.loading.googlePhotos': {
                count: true,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: undefined,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
              },
            },
            // SET_GOOGLE_PHOTOS_COUNT.
            {
              'wallpaper.loading.googlePhotos': {
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
              },
            },
            // BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS.
            {
              'wallpaper.loading.googlePhotos': {
                count: false,
                albums: true,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
              },
            },
            // BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS.
            {
              'wallpaper.loading.googlePhotos': {
                count: false,
                albums: true,
                photos: true,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
              },
            },
            // SET_GOOGLE_PHOTOS_ALBUMS.
            {
              'wallpaper.loading.googlePhotos': {
                count: false,
                albums: false,
                photos: true,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: expectedCount,
                albums: expectedAlbums,
                photos: undefined,
                photosByAlbumId: {},
              },
            },
            // SET_GOOGLE_PHOTOS_PHOTOS.
            {
              'wallpaper.loading.googlePhotos': {
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                count: expectedCount,
                albums: expectedAlbums,
                photos: expectedPhotos,
                photosByAlbumId: {},
              },
            },
          ],
          personalizationStore.states.map(filterAndFlattenState(
              ['wallpaper.googlePhotos', 'wallpaper.loading.googlePhotos'])));
    });
  });

  test('sets Google Photos album in store', async () => {
    const album = new WallpaperCollection();
    album.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';

    // Attempts to `fetchGooglePhotosAlbum()` will fail unless the entire list
    // of Google Photos albums has already been fetched and saved to the store.
    personalizationStore.dispatch(
        wallpaperAction.beginLoadGooglePhotosAlbumsAction());
    personalizationStore.dispatch(
        wallpaperAction.setGooglePhotosAlbumsAction([album]));
    personalizationStore.reset(personalizationStore.data);

    await fetchGooglePhotosAlbum(
        wallpaperProvider, personalizationStore, album.id);

    assertDeepEquals(
        [
          {
            name: 'begin_load_google_photos_album',
            albumId: album.id,
          },
          {
            name: 'set_google_photos_album',
            albumId: album.id,
            photos: Array.from({length: 1000}),
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM
          {
            'wallpaper.loading.googlePhotos': {
              count: false,
              albums: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: true,
              },
            },
            'wallpaper.googlePhotos': {
              count: undefined,
              albums: [
                {
                  id: album.id,
                },
              ],
              photos: undefined,
              photosByAlbumId: {},
            },
          },
          // SET_GOOGLE_PHOTOS_ALBUM
          {
            'wallpaper.loading.googlePhotos': {
              count: false,
              albums: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: false,
              },
            },
            'wallpaper.googlePhotos': {
              count: undefined,
              albums: [
                {
                  id: album.id,
                },
              ],
              photos: undefined,
              photosByAlbumId: {
                [album.id]: Array.from({length: 1000}),
              },
            },
          },
        ],
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.googlePhotos', 'wallpaper.loading.googlePhotos'])));
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
            'wallpaper.loading.local': {images: true, data: {}},
            'wallpaper.local': {images: null, data: {}}
          },
          // Done loading local image data.
          {
            'wallpaper.loading.local': {data: {}, images: false},
            'wallpaper.local': {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {}
            }
          },
          // Mark image 0 as loading.
          {
            'wallpaper.loading.local': {
              data: {'LocalImage0.png': true},
              images: false,
            },
            'wallpaper.local': {
              images: [{path: 'LocalImage0.png'}, {path: 'LocalImage1.png'}],
              data: {},
            },
          },
          // Mark image 1 as loading.
          {
            'wallpaper.loading.local': {
              data: {'LocalImage0.png': true, 'LocalImage1.png': true},
              images: false,
            },
            'wallpaper.local': {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {},
            }
          },
          // Finish loading image 0.
          {
            'wallpaper.loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': true},
              images: false,
            },
            'wallpaper.local': {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {'LocalImage0.png': 'data://localimage0data'},
            }
          },
          // Finish loading image 1.
          {
            'wallpaper.loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': false},
              images: false,
            },
            'wallpaper.local': {
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
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.local', 'wallpaper.loading.local'])));
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
            'wallpaper.loading.local': {
              data: {'LocalImage0.png': false, 'LocalImage1.png': false},
              images: true,
            },
            'wallpaper.local': {
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
            'wallpaper.loading.local':
                {data: {'LocalImage0.png': false}, images: false},
            'wallpaper.local': {
              images: [{path: 'LocalImage0.png'}],
              data: {'LocalImage0.png': 'data://localimage0data'},
            },
          },
        ],
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.local', 'wallpaper.loading.local'])));
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
            'wallpaper.loading.local': {
              'data': {'LocalImage0.png': false, 'LocalImage1.png': false},
              'images': true,
            },
            'wallpaper.local': {
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
            'wallpaper.loading.local': {
              'data': {'LocalImage0.png': false},
              'images': false,
            },
            'wallpaper.local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {'LocalImage0.png': 'data://localimage0data'},
            },
          },
          // Begin loading NewPath.png data.
          {
            'wallpaper.loading.local': {
              'data': {'LocalImage0.png': false, 'NewPath.png': true},
              'images': false,
            },
            'wallpaper.local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {'LocalImage0.png': 'data://localimage0data'},
            },
          },
          // Done loading NewPath.png data.
          {
            'wallpaper.loading.local': {
              'data': {'LocalImage0.png': false, 'NewPath.png': false},
              'images': false,
            },
            'wallpaper.local': {
              'images': [{'path': 'LocalImage0.png'}, {'path': 'NewPath.png'}],
              'data': {
                'LocalImage0.png': 'data://localimage0data',
                'NewPath.png': 'data://newpath',
              },
            },
          }
        ],
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.local', 'wallpaper.loading.local'])));
  });

  test('clears local images when fetching new image list fails', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    wallpaperProvider.localImages = null;
    await fetchLocalData(wallpaperProvider, personalizationStore);

    assertEquals(null, personalizationStore.data.wallpaper.local.images);
    assertDeepEquals({}, personalizationStore.data.wallpaper.local.data);
    assertDeepEquals(
        {}, personalizationStore.data.wallpaper.loading.local.data);
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
    loadTimeData.resetForTesting({[fullscreenPreviewFeature]: true});
  });

  test(
      'enters full screen mode when in tablet and preview flag is set',
      async () => {
        await initializeBackdropData(wallpaperProvider, personalizationStore);

        assertFalse(personalizationStore.data.wallpaper.fullscreen);

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

          assertFalse(personalizationStore.data.wallpaper.fullscreen);
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

          assertTrue(personalizationStore.data.wallpaper.fullscreen);
        }
      });
});

suite('observes pendingState during wallpaper selection', () => {
  let wallpaperProvider;
  let personalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  test(
      'sets pendingState to selected image for successful operation',
      async () => {
        await selectWallpaper(
            wallpaperProvider.images[0], wallpaperProvider,
            personalizationStore);

        assertDeepEquals(
            [
              {
                name: 'begin_select_image',
                image: wallpaperProvider.images[0],
              },
              {
                name: 'begin_load_selected_image',
              },
              {
                name: 'end_select_image',
                image: wallpaperProvider.images[0],
                success: true,
              },
              {
                name: 'set_fullscreen_enabled',
                enabled: true,
              },
            ],
            personalizationStore.actions,
        );

        assertDeepEquals(
            [
              // Begin selecting image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.images[0],
              },
              // Begin loading image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.images[0],
              },
              // End selecting image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.images[0],
              },
              // Set fullscreen enabled
              {
                'wallpaper.pendingSelected': wallpaperProvider.images[0],
              },
            ],
            personalizationStore.states.map(
                filterAndFlattenState(['wallpaper.pendingSelected'])));
      });

  test(
      'clears pendingState when error occured and only one request',
      async () => {
        await selectWallpaper(
            wallpaperProvider.localImages[0], wallpaperProvider,
            personalizationStore);
        // Reset the history of actions and prior states, but keep the current
        // state.
        personalizationStore.reset(personalizationStore.data);

        loadTimeData.overrideValues({['setWallpaperError']: 'someError'});

        // sets selected image without file path to force fail the operation.
        wallpaperProvider.localImages = [{path: ''}];
        await selectWallpaper(
            wallpaperProvider.localImages[0], wallpaperProvider,
            personalizationStore);

        assertDeepEquals(
            [
              {
                name: 'begin_select_image',
                image: wallpaperProvider.localImages[0],
              },
              {
                name: 'begin_load_selected_image',
              },
              {
                name: 'end_select_image',
                image: wallpaperProvider.localImages[0],
                success: false,
              },
              {
                name: 'set_selected_image',
                image: personalizationStore.data.wallpaper.currentSelected,
              },
            ],
            personalizationStore.actions,
        );

        assertDeepEquals(
            [
              // Begin selecting image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.localImages[0],
              },
              // Begin loading image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.localImages[0],
              },
              // End selecting image, pendingState is cleared.
              {
                'wallpaper.pendingSelected': null,
              },
              // Set selected image
              {
                'wallpaper.pendingSelected': null,
              },
            ],
            personalizationStore.states.map(
                filterAndFlattenState(['wallpaper.pendingSelected'])));
      });
});

suite('local images available but no internet connection', () => {
  let wallpaperProvider;
  let personalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  test(
      'error displays when fetch collections failed but local images loaded',
      async () => {
        loadTimeData.overrideValues({['networkError']: 'someError'});

        // Set collections to null to simulate collections failure.
        wallpaperProvider.setCollections(null);

        // Assume that collections are loaded before local images.
        const collectionsPromise =
            fetchCollections(wallpaperProvider, personalizationStore);
        const localImagesPromise =
            getLocalImages(wallpaperProvider, personalizationStore);

        await collectionsPromise;

        assertFalse(personalizationStore.data.wallpaper.loading.collections);
        assertEquals(
            null, personalizationStore.data.wallpaper.backdrop.collections);

        await localImagesPromise;

        assertFalse(personalizationStore.data.wallpaper.loading.local.images);
        assertDeepEquals(
            wallpaperProvider.localImages,
            personalizationStore.data.wallpaper.local.images);

        assertDeepEquals(
            [
              {
                name: 'begin_load_local_images',
              },
              {
                name: 'set_collections',
                collections: null,
              },
              {name: 'set_local_images', images: wallpaperProvider.localImages},
            ],
            personalizationStore.actions,
        );


        assertDeepEquals(
            [
              // Begin load local images
              {
                'error': null,
              },
              // Set collections.
              // Collections are completed loading with null value
              // but local images are not yet done, no error displays.
              {
                'error': null,
              },
              // Set local images.
              // Error displays once local images are loaded.
              {
                'error': loadTimeData.getString('networkError'),
              },
            ],
            personalizationStore.states.map(filterAndFlattenState(['error'])));
      });
});
