// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {cancelPreviewWallpaper, fetchCollections, fetchGooglePhotosAlbum, fetchLocalData, getLocalImages, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, initializeBackdropData, initializeGooglePhotosData, selectWallpaper} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

/**
 * Get a sub-property in obj. Splits on '.'
 */
function getProperty(obj: object, key: string): unknown {
  let ref: any = obj;
  for (const part of key.split('.')) {
    ref = ref[part];
  }
  return ref;
}

/**
 * Returns a function that returns only nested subproperties in state.
 */
function filterAndFlattenState(keys: string[]): (state: any) => any {
  return (state) => {
    const result: any = {};
    for (const key of keys) {
      result[key] = getProperty(state, key);
    }
    return result;
  };
}

suite('Personalization app controller', () => {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  [true, false].forEach(isGooglePhotosIntegrationEnabled => {
    test('initializes Google Photos data in store', async () => {
      loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled});

      await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

      let expectedEnabled, expectedCount, expectedAlbums, expectedPhotos;
      if (isGooglePhotosIntegrationEnabled) {
        expectedEnabled = GooglePhotosEnablementState.kEnabled;
        expectedCount = 0;
        expectedAlbums = [];
        expectedPhotos = [];
      } else {
        expectedEnabled = GooglePhotosEnablementState.kError;
        expectedCount = null;
        expectedAlbums = null;
        expectedPhotos = null;
      }

      assertDeepEquals(
          [
            {
              name: 'begin_load_google_photos_enabled',
            },
            {
              name: 'set_google_photos_enabled',
              enabled: expectedEnabled,
            },
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
              name: 'append_google_photos_albums',
              albums: expectedAlbums,
              resumeToken: null,
            },
            {
              name: 'append_google_photos_photos',
              photos: expectedPhotos,
              resumeToken: null,
            },
          ],
          personalizationStore.actions);

      assertDeepEquals(
          [
            // BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: true,
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: undefined,
                count: undefined,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // SET_GOOGLE_PHOTOS_ENABLED.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: undefined,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // BEGIN_LOAD_GOOGLE_PHOTOS_COUNT.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: true,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: undefined,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // SET_GOOGLE_PHOTOS_COUNT.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // BEGIN_LOAD_GOOGLE_PHOTOS_ALBUMS.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: true,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // BEGIN_LOAD_GOOGLE_PHOTOS_PHOTOS.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: true,
                photos: true,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: expectedCount,
                albums: undefined,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // APPEND_GOOGLE_PHOTOS_ALBUMS.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: false,
                photos: true,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: expectedCount,
                albums: expectedAlbums,
                photos: undefined,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
            // APPEND_GOOGLE_PHOTOS_PHOTOS.
            {
              'wallpaper.loading.googlePhotos': {
                enabled: false,
                count: false,
                albums: false,
                photos: false,
                photosByAlbumId: {},
              },
              'wallpaper.googlePhotos': {
                enabled: expectedEnabled,
                count: expectedCount,
                albums: expectedAlbums,
                photos: expectedPhotos,
                photosByAlbumId: {},
                resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
              },
            },
          ],
          personalizationStore.states.map(filterAndFlattenState(
              ['wallpaper.googlePhotos', 'wallpaper.loading.googlePhotos'])));
    });
  });

  test('sets Google Photos album in store', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});

    const album = new GooglePhotosAlbum();
    album.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';
    album.preview = {url: 'bar.com'};

    const photos: GooglePhotosPhoto[] = [{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    }];

    wallpaperProvider.setGooglePhotosCount(photos.length);
    wallpaperProvider.setGooglePhotosAlbums([album]);
    wallpaperProvider.setGooglePhotosPhotos(photos);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(album.id, photos);

    // Attempts to `fetchGooglePhotosAlbum()` will fail unless the entire list
    // of Google Photos albums has already been fetched and saved to the store.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    personalizationStore.reset(personalizationStore.data);

    await fetchGooglePhotosAlbum(
        wallpaperProvider, personalizationStore, album.id);

    // The wallpaper controller is expected to impose max resolution.
    album.preview.url += '=s512';
    photos.forEach(photo => photo.url.url += '=s512');

    assertDeepEquals(
        [
          {
            name: 'begin_load_google_photos_album',
            albumId: album.id,
          },
          {
            name: 'append_google_photos_album',
            albumId: album.id,
            photos: photos,
            resumeToken: null,
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // BEGIN_LOAD_GOOGLE_PHOTOS_ALBUM
          {
            'wallpaper.loading.googlePhotos': {
              enabled: false,
              count: false,
              albums: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: true,
              },
            },
            'wallpaper.googlePhotos': {
              enabled: GooglePhotosEnablementState.kEnabled,
              count: photos.length,
              albums: [
                {
                  id: album.id,
                  preview: album.preview,
                },
              ],
              photos: photos,
              photosByAlbumId: {},
              resumeTokens: {albums: null, photos: null, photosByAlbumId: {}},
            },
          },
          // APPEND_GOOGLE_PHOTOS_ALBUM
          {
            'wallpaper.loading.googlePhotos': {
              enabled: false,
              count: false,
              albums: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: false,
              },
            },
            'wallpaper.googlePhotos': {
              enabled: GooglePhotosEnablementState.kEnabled,
              count: photos.length,
              albums: [
                {
                  id: album.id,
                  preview: album.preview,
                },
              ],
              photos: photos,
              photosByAlbumId: {
                [album.id]: photos,
              },
              resumeTokens: {
                albums: null,
                photos: null,
                photosByAlbumId: {[album.id]: null},
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
    wallpaperProvider.localImages = [wallpaperProvider.localImages![0]!];
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
      wallpaperProvider.localImages![0]!,
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

  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

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
              wallpaperProvider.images![0]!, wallpaperProvider,
              personalizationStore);
          const [assetId, previewMode] =
              await wallpaperProvider.whenCalled('selectWallpaper');
          assertFalse(previewMode);
          assertEquals(wallpaperProvider.images![0]!.assetId, assetId);

          await selectWallpaperPromise;
          assertEquals(
              0, wallpaperProvider.getCallCount('makeTransparent'),
              'makeTransparent is not called when fullscreen preview is off');
          assertEquals(
              0, wallpaperProvider.getCallCount('makeOpaque'),
              'makeOpaque is not called when fullscreen preview is off');

          assertFalse(personalizationStore.data.wallpaper.fullscreen);
        }

        wallpaperProvider.reset();

        {
          // Now with flag turned on.
          loadTimeData.overrideValues({[fullscreenPreviewFeature]: true});

          assertEquals(0, wallpaperProvider.getCallCount('makeTransparent'));
          assertEquals(0, wallpaperProvider.getCallCount('makeOpaque'));

          const selectWallpaperPromise = selectWallpaper(
              wallpaperProvider.images![0]!, wallpaperProvider,
              personalizationStore);

          const [assetId, previewMode] =
              await wallpaperProvider.whenCalled('selectWallpaper');
          assertTrue(previewMode);
          assertEquals(wallpaperProvider.images![0]!.assetId, assetId);

          await selectWallpaperPromise;
          assertEquals(
              1, wallpaperProvider.getCallCount('makeTransparent'),
              'makeTransparent is called while calling selectWallpaper');

          assertTrue(personalizationStore.data.wallpaper.fullscreen);

          await cancelPreviewWallpaper(wallpaperProvider);
          assertEquals(
              1, wallpaperProvider.getCallCount('makeOpaque'),
              'makeOpaque is called while calling cancelPreviewWallpaper');
        }
      });
});

suite('observes pendingState during wallpaper selection', () => {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
  });

  test(
      'sets pendingState to selected image for successful operation',
      async () => {
        await selectWallpaper(
            wallpaperProvider.images![0]!, wallpaperProvider,
            personalizationStore);

        assertDeepEquals(
            [
              {
                name: 'begin_select_image',
                image: wallpaperProvider.images![0],
              },
              {
                name: 'begin_load_selected_image',
              },
              {
                name: 'end_select_image',
                image: wallpaperProvider.images![0],
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
                'wallpaper.pendingSelected': wallpaperProvider.images![0],
              },
              // Begin loading image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.images![0],
              },
              // End selecting image.
              {
                'wallpaper.pendingSelected': wallpaperProvider.images![0],
              },
              // Set fullscreen enabled
              {
                'wallpaper.pendingSelected': wallpaperProvider.images![0],
              },
            ],
            personalizationStore.states.map(
                filterAndFlattenState(['wallpaper.pendingSelected'])));
      });

  test(
      'clears pendingState when error occured and only one request',
      async () => {
        await selectWallpaper(
            wallpaperProvider.localImages![0]!, wallpaperProvider,
            personalizationStore);
        // Reset the history of actions and prior states, but keep the current
        // state.
        personalizationStore.reset(personalizationStore.data);

        loadTimeData.overrideValues({['setWallpaperError']: 'someError'});

        // sets selected image without file path to force fail the operation.
        wallpaperProvider.localImages = [{path: ''}];
        await selectWallpaper(
            wallpaperProvider.localImages[0]!, wallpaperProvider,
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
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

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
        wallpaperProvider.setCollectionsToFail();

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
