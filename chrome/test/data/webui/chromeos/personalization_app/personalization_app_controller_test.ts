// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {cancelPreviewWallpaper, DailyRefreshType, DefaultImageSymbol, DisplayableImage, fetchCollections, fetchGooglePhotosAlbum, fetchGooglePhotosAlbums, fetchLocalData, getDefaultImageThumbnail, GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, initializeBackdropData, initializeGooglePhotosData, isDefaultImage, isFilePath, isGooglePhotosPhoto, isWallpaperImage, kDefaultImageSymbol, selectGooglePhotosAlbum, selectWallpaper, updateDailyRefreshWallpaper, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {FilePath} from 'chrome://resources/mojo/mojo/public/mojom/base/file_path.mojom-webui.js';
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

  test('initializes Google Photos data in store', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});

    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    const expectedEnabled = GooglePhotosEnablementState.kEnabled;

    assertDeepEquals(
        [
          {
            name: 'begin_load_google_photos_enabled',
          },
          {
            name: 'set_google_photos_enabled',
            enabled: expectedEnabled,
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // BEGIN_LOAD_GOOGLE_PHOTOS_ENABLED.
          {
            'wallpaper.loading.googlePhotos': {
              enabled: true,
              albums: false,
              albumsShared: false,
              photos: false,
              photosByAlbumId: {},
            },
            'wallpaper.googlePhotos': {
              enabled: undefined,
              albums: undefined,
              albumsShared: undefined,
              photos: undefined,
              photosByAlbumId: {},
              resumeTokens: {
                albums: null,
                albumsShared: null,
                photos: null,
                photosByAlbumId: {},
              },
            },
          },
          // SET_GOOGLE_PHOTOS_ENABLED.
          {
            'wallpaper.loading.googlePhotos': {
              enabled: false,
              albums: false,
              albumsShared: false,
              photos: false,
              photosByAlbumId: {},
            },
            'wallpaper.googlePhotos': {
              enabled: expectedEnabled,
              albums: undefined,
              albumsShared: undefined,
              photos: undefined,
              photosByAlbumId: {},
              resumeTokens: {
                albums: null,
                albumsShared: null,
                photos: null,
                photosByAlbumId: {},
              },
            },
          },
        ],
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.googlePhotos', 'wallpaper.loading.googlePhotos'])));
  });

  test('sets Google Photos album in store', async () => {
    loadTimeData.overrideValues({isGooglePhotosIntegrationEnabled: true});

    const album = new GooglePhotosAlbum();
    album.id = '9bd1d7a3-f995-4445-be47-53c5b58ce1cb';
    album.preview = {url: 'bar.com'};

    const photos: GooglePhotosPhoto[] = [{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home',
    }];

    wallpaperProvider.setGooglePhotosAlbums([album]);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(album.id, photos);

    // Attempts to `fetchGooglePhotosAlbum()` will fail unless the entire list
    // of Google Photos albums has already been fetched and saved to the store.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);
    await fetchGooglePhotosAlbums(wallpaperProvider, personalizationStore);
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
              albums: false,
              albumsShared: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: true,
              },
            },
            'wallpaper.googlePhotos': {
              enabled: GooglePhotosEnablementState.kEnabled,
              albums: [
                {
                  id: album.id,
                  preview: album.preview,
                },
              ],
              albumsShared: undefined,
              photos: undefined,
              photosByAlbumId: {},
              resumeTokens: {
                albums: null,
                albumsShared: null,
                photos: null,
                photosByAlbumId: {},
              },
            },
          },
          // APPEND_GOOGLE_PHOTOS_ALBUM
          {
            'wallpaper.loading.googlePhotos': {
              enabled: false,
              albums: false,
              albumsShared: false,
              photos: false,
              photosByAlbumId: {
                [album.id]: false,
              },
            },
            'wallpaper.googlePhotos': {
              enabled: GooglePhotosEnablementState.kEnabled,
              albums: [
                {
                  id: album.id,
                  preview: album.preview,
                },
              ],
              albumsShared: undefined,
              photos: undefined,
              photosByAlbumId: {
                [album.id]: photos,
              },
              resumeTokens: {
                albums: null,
                albumsShared: null,
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
            data: {url: 'data:image/png;base64,localimage0data'},
          },
          {
            name: 'set_local_image_data',
            id: 'LocalImage1.png',
            data: {url: 'data:image/png;base64,localimage1data'},
          },
        ],
        personalizationStore.actions);

    assertDeepEquals(
        [
          // Begin loading local image list.
          {
            'wallpaper.loading.local': {images: true, data: {}},
            'wallpaper.local': {images: null, data: {}},
          },
          // Done loading local image data.
          {
            'wallpaper.loading.local': {data: {}, images: false},
            'wallpaper.local': {
              images: [
                {path: 'LocalImage0.png'},
                {path: 'LocalImage1.png'},
              ],
              data: {},
            },
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
            },
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
              data: {
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
              },
            },
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
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
                'LocalImage1.png': {
                  url: 'data:image/png;base64,localimage1data',
                },
              },
            },
          },
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
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
                'LocalImage1.png': {
                  url: 'data:image/png;base64,localimage1data',
                },
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
              data: {
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
              },
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
      'NewPath.png': {url: 'data:image/png;base64,newpath'},
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
            data: {url: 'data:image/png;base64,newpath'},
          },
        ],
        personalizationStore.actions,
        JSON.stringify(personalizationStore.actions));

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
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
                'LocalImage1.png': {
                  url: 'data:image/png;base64,localimage1data',
                },
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
              'data': {
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
              },
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
              'data': {
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
              },
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
                'LocalImage0.png': {
                  url: 'data:image/png;base64,localimage0data',
                },
                'NewPath.png': {url: 'data:image/png;base64,newpath'},
              },
            },
          },
        ],
        personalizationStore.states.map(filterAndFlattenState(
            ['wallpaper.local', 'wallpaper.loading.local'])));
  });

  test('clears local images when fetching new image list fails', async () => {
    // No default image on this device.
    wallpaperProvider.defaultImageThumbnail = {url: ''};
    await getDefaultImageThumbnail(wallpaperProvider, personalizationStore);
    await fetchLocalData(wallpaperProvider, personalizationStore);

    wallpaperProvider.localImages = null;
    await fetchLocalData(wallpaperProvider, personalizationStore);

    assertEquals(
        null, personalizationStore.data.wallpaper.local.images,
        'local images set to null');
    assertDeepEquals({}, personalizationStore.data.wallpaper.local.data);
    assertDeepEquals(
        {url: ''},
        personalizationStore.data.wallpaper.local.data[kDefaultImageSymbol],
        'default image still present but set to empty url');
    assertDeepEquals(
        {}, personalizationStore.data.wallpaper.loading.local.data,
        'local images not loading');
    assertFalse(
        personalizationStore.data.wallpaper.loading.local
            .data[kDefaultImageSymbol],
        'default image is not loading');
  });

  test('fails to enable Google Photos daily refresh', async () => {
    wallpaperProvider.selectGooglePhotosAlbumResponse = {
      success: false,
      forceRefresh: true,
    };

    wallpaperProvider.albumId = 'albumId';
    wallpaperProvider.collectionId = '';

    const selectGooglePhotosAlbumPromise = selectGooglePhotosAlbum(
        'albumId', wallpaperProvider, personalizationStore);

    await Promise.all([
      wallpaperProvider.getDailyRefreshCollectionId(),
      wallpaperProvider.getGooglePhotosDailyRefreshAlbumId(),
    ]);

    await selectGooglePhotosAlbumPromise;

    assertDeepEquals(
        [
          // Set error action when daily refresh failed.
          {
            name: 'set_error',
            error: {message: loadTimeData.getString('googlePhotosError')},
          },
          // Set daily refresh enabled for the selected Google Photos album.
          {
            name: 'set_google_photos_daily_refresh_album_id',
            albumId: 'albumId',
          },
        ],
        personalizationStore.actions);
  });

  test(
      'fails to refresh a new wallpaper in a Google Photos album', async () => {
        personalizationStore.data.wallpaper.dailyRefresh = {
          id: 'abumId',
          type: DailyRefreshType.GOOGLE_PHOTOS,
        };
        wallpaperProvider.updateDailyRefreshWallpaperResponse = false;
        await updateDailyRefreshWallpaper(
            wallpaperProvider, personalizationStore);
        assertDeepEquals(
            [
              {
                name: 'begin_update_daily_refresh_image',
              },
              {
                name: 'begin_load_selected_image',
              },
              {
                name: 'set_updated_daily_refreshed_image',
              },
              {
                name: 'set_selected_image',
                image: personalizationStore.data.wallpaper.currentSelected,
              },
              {
                name: 'set_error',
                error: {message: loadTimeData.getString('googlePhotosError')},
              },
            ],
            personalizationStore.actions);
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

  test('error displays when fetch collections failed', async () => {
    // Set collections to null to simulate collections failure.
    wallpaperProvider.setCollectionsToFail();

    // Assume that collections are loaded before local images.
    const collectionsPromise =
        fetchCollections(wallpaperProvider, personalizationStore);

    await collectionsPromise;

    assertFalse(personalizationStore.data.wallpaper.loading.collections);
    assertEquals(
        null, personalizationStore.data.wallpaper.backdrop.collections);

    assertDeepEquals(
        [
          {
            name: 'set_collections',
            collections: null,
          },
        ],
        personalizationStore.actions,
    );

    assertDeepEquals(
        [
          // Set collections.
          // Collections are completed loading with null value. Error displays.
          {
            'error': {message: loadTimeData.getString('wallpaperNetworkError')},
          },
        ],
        personalizationStore.states.map(filterAndFlattenState(['error'])));
  });
});

suite('does not respond to re-selecting the current wallpaper', () => {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider.isInTabletModeResponse = false;
  });

  function getImageKey(image: DisplayableImage): string|undefined {
    if (isDefaultImage(image)) {
      return undefined;
    }
    if (isGooglePhotosPhoto(image)) {
      return image.dedupKey ? image.dedupKey : image.id;
    }
    if (isWallpaperImage(image)) {
      return image.assetId.toString();
    }
    if (isFilePath(image)) {
      return image.path;
    }
    assertNotReached('unknown wallpaper type');
  }

  function getImageType(image: DisplayableImage): WallpaperType {
    if (isDefaultImage(image)) {
      return WallpaperType.kDefault;
    }
    if (isGooglePhotosPhoto(image)) {
      return WallpaperType.kOnceGooglePhotos;
    }
    if (isWallpaperImage(image)) {
      return WallpaperType.kOnline;
    }
    if (isFilePath(image)) {
      return WallpaperType.kCustomized;
    }
    assertNotReached('unknown wallpaper type');
  }

  // Selects `image` as the wallpaper twice and verifies that the second attempt
  // quits early because there is no work to do.
  async function testReselectWallpaper(image: DisplayableImage) {
    const selectWallpaperActions = [
      {
        name: 'begin_select_image',
        image: image,
      },
      {
        name: 'begin_load_selected_image',
      },
      {
        name: 'end_select_image',
        image: image,
        success: true,
      },
    ];

    // Select a wallpaper and verify that the correct actions are taken.
    await selectWallpaper(image, wallpaperProvider, personalizationStore);
    assertDeepEquals(personalizationStore.actions, selectWallpaperActions);

    // Complete the pending selection as would happen in production code.
    const pendingSelected = personalizationStore.data.wallpaper.pendingSelected;
    assertEquals(pendingSelected, image);
    personalizationStore.data.wallpaper.currentSelected = {
      key: getImageKey(image),
      type: getImageType(image),
    };
    personalizationStore.data.wallpaper.pendingSelected = null;

    // Select the same wallpaper and verify that no further actions are taken.
    await selectWallpaper(image, wallpaperProvider, personalizationStore);
    assertDeepEquals(personalizationStore.actions, selectWallpaperActions);
  }

  test('re-selects online wallpaper', async () => {
    await initializeBackdropData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    const onlineImages = wallpaperProvider.images;
    assertTrue(!!onlineImages && onlineImages.length > 0);
    const image = onlineImages[0]!;

    await testReselectWallpaper(image);
  });

  test('re-selects local wallpaper', async () => {
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    const localImages = personalizationStore.data.wallpaper.local.images;
    assertTrue(!!localImages && localImages.length > 0);
    const image = localImages[0]!;

    await testReselectWallpaper(image);
  });

  // Check with both |dedupKey| absent and present for backwards compatibility
  // with older clients that do not support the latter.
  [undefined, '2d0d1595-14af-4471-b2db-b9c8eae3a491'].forEach(
      dedupKey => test('re-selects Google Photos wallpaper', async () => {
        const image: GooglePhotosPhoto = {
          id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
          dedupKey: dedupKey,
          name: 'foo',
          date: {data: []},
          url: {url: 'foo.com'},
          location: 'home',
        };
        // Reset the history of actions and prior states, but keep the current
        // state.
        personalizationStore.reset(personalizationStore.data);
        await testReselectWallpaper(image);
      }));

  test('re-selects default image', async () => {
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);
    await testReselectWallpaper(kDefaultImageSymbol);
  });
});

suite('updates default image', () => {
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    wallpaperProvider = new TestWallpaperProvider();
    personalizationStore = new TestPersonalizationStore({});
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider.isInTabletModeResponse = false;
  });

  test('get default image thumbnail', async () => {
    // Initialize some local image data.
    await fetchLocalData(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    assertTrue(
        personalizationStore.data.wallpaper.local.images.every(
            (image: FilePath|DefaultImageSymbol) => isFilePath(image) &&
                !!personalizationStore.data.wallpaper.local.data[image.path]),
        'every image is file path with data');

    await getDefaultImageThumbnail(wallpaperProvider, personalizationStore);

    assertDeepEquals(
        [
          {name: 'begin_load_default_image'},
          {
            thumbnail: {url: 'data:image/png;base64,default_image_thumbnail'},
            name: 'set_default_image',
          },
        ],
        personalizationStore.actions,
        'load default image thumbnail actions',
    );


    assertDeepEquals(
        [true, false],
        personalizationStore.states.map(
            state => state.wallpaper.loading.local.data[kDefaultImageSymbol]),
        'expected loading state while fetching default thumbnail',
    );
    assertDeepEquals(
        wallpaperProvider.defaultImageThumbnail,
        personalizationStore.data.wallpaper.local.data[kDefaultImageSymbol],
        'default image thumbnail is set');
  });

  test('refresh local image list keeps default thumbnail', async () => {
    // Initialize some local image data.
    await fetchLocalData(wallpaperProvider, personalizationStore);
    await getDefaultImageThumbnail(wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    assertDeepEquals(
        wallpaperProvider.defaultImageThumbnail,
        personalizationStore.data.wallpaper.local.data[kDefaultImageSymbol],
        'default image thumbnail is set');

    assertDeepEquals(
        [kDefaultImageSymbol, ...wallpaperProvider.localImages!],
        personalizationStore.data.wallpaper.local.images,
        'local images include default thumbnail',
    );

    // Simulate user deleting a local image from Downloads directory. Keep the
    // first image only.
    wallpaperProvider.localImages = wallpaperProvider.localImages!.slice(0, 1);
    await fetchLocalData(wallpaperProvider, personalizationStore);

    // Default image symbol does not show up in Object.keys.
    assertDeepEquals(
        [wallpaperProvider.localImages[0]!.path],
        Object.keys(personalizationStore.data.wallpaper.local.data),
        'local image data deleted for missing image');

    assertEquals(
        wallpaperProvider.defaultImageThumbnail,
        personalizationStore.data.wallpaper.local.data[kDefaultImageSymbol],
        'default image thumbnail is still set');
  });

  test('enable Google Photos daily refresh triggers loading UI', async () => {
    await selectGooglePhotosAlbum(
        'albumId', wallpaperProvider, personalizationStore);
    // Reset the history of actions and prior states, but keep the current
    // state.
    personalizationStore.reset(personalizationStore.data);

    assertTrue(
        personalizationStore.data.wallpaper.loading.refreshWallpaper,
        'loading refresh wallpaper.');
  });
});
