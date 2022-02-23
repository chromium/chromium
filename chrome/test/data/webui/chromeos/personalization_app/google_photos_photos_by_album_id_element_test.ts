// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotosAlbum, GooglePhotosPhoto, WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {GooglePhotosPhotosByAlbumId} from 'chrome://personalization/trusted/wallpaper/google_photos_photos_by_album_id_element.js';
import {initializeGooglePhotosData} from 'chrome://personalization/trusted/wallpaper/wallpaper_controller.js';
import {WallpaperGridItem} from 'chrome://personalization/trusted/wallpaper/wallpaper_grid_item_element.js';
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function GooglePhotosPhotosByAlbumIdTest() {
  let googlePhotosPhotosByAlbumIdElement: GooglePhotosPhotosByAlbumId|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns all matches for |selector| in
   * |googlePhotosPhotosByAlbumIdElement|'s shadow DOM.
   */
  function querySelectorAll(selector: string): Element[]|null {
    const matches =
        googlePhotosPhotosByAlbumIdElement!.shadowRoot!.querySelectorAll(
            selector);
    return matches ? [...matches] : null;
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosByAlbumIdElement);
    googlePhotosPhotosByAlbumIdElement = null;
  });

  test('displays photos for the selected album id', async () => {
    const photosByAlbumId: Record<string, GooglePhotosPhoto[]> = {
      '1': [
        {
          id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
          name: 'foo',
          date: {data: []},
          url: {url: 'foo.com'}
        },
        {
          id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
          name: 'bar',
          date: {data: []},
          url: {url: 'bar.com'}
        },
      ],
      '2': [
        {
          id: '0a268a37-877a-4936-81d4-38cc84b0f596',
          name: 'baz',
          date: {data: []},
          url: {url: 'baz.com'}
        },
      ],
    };

    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Initially no album id selected. Photos should be absent.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Select an album id. Photos should be absent since they are not loaded.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', '1');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Expect a request to load photos for the selected album id.
    await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos');
    assertEquals(
        wallpaperProvider.getArgs('fetchGooglePhotosPhotos')[0][1], '1');

    // Start loading photos for the selected album id. Photos should still be
    // absent since they are still not loaded.
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '1': true,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Load photos for an album id other than that which is selected. Photos
    // should still be absent since they are still not loaded for the selected
    // album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      '2': photosByAlbumId['2'],
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '2': false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);

    // Finish loading photos for the selected album id. Photos should now be
    // present since they are finished loading for the selected album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      '1': photosByAlbumId['1'],
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '1': false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    const photosEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photosEls.length, photosByAlbumId['1']?.length);
    photosEls.forEach((photoEl, i) => {
      assertEquals(photoEl.imageSrc, photosByAlbumId['1']![i]!.url.url);
    });

    // Select the other album id for which data is already loaded. Photos should
    // immediately update since they are already loaded for the selected album
    // id.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', '2');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(
        querySelectorAll(photoSelector)!.length, photosByAlbumId['2']?.length);

    // Un-select the album id. Photos should be absent since no album id is
    // selected.
    googlePhotosPhotosByAlbumIdElement.removeAttribute('album-id');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector)!.length, 0);
  });

  test('displays photo selected', async () => {
    personalizationStore.setReducersEnabled(true);

    const album: GooglePhotosAlbum =
        {id: '1', title: '', photoCount: 2, preview: {url: ''}};

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'}
    };

    const anotherPhoto: GooglePhotosPhoto = {
      id: '0ec40478-9712-42e1-b5bf-3e75870ca042',
      name: 'bar',
      date: {data: []},
      url: {url: 'bar.com'}
    };

    // Set values returned by |wallpaperProvider|.
    wallpaperProvider.setGooglePhotosAlbums([album]);
    wallpaperProvider.setGooglePhotosCount(2);
    wallpaperProvider.setGooglePhotosPhotos([photo, anotherPhoto]);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(
        album.id, [photo, anotherPhoto]);

    // Initialize Google Photos data in the |personalizationStore|.
    await initializeGooglePhotosData(wallpaperProvider, personalizationStore);

    // Initialize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify that the expected photos are rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 2);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |photo|.
    personalizationStore.data.wallpaper.pendingSelected = photo;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: photo.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: photo.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, true);
    assertEquals(photoEls[1]!.selected, false);

    // Start a pending selection for |anotherPhoto|.
    personalizationStore.data.wallpaper.pendingSelected = anotherPhoto;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: anotherPhoto.url,
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kGooglePhotos,
      key: anotherPhoto.id
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, true);

    // Start a pending selection for a |FilePath| backed wallpaper.
    personalizationStore.data.wallpaper.pendingSelected = {path: '//foo'};
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);

    // Complete the pending selection.
    personalizationStore.data.wallpaper.pendingSelected = null;
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'foo://'},
      attribution: [],
      layout: WallpaperLayout.kCenter,
      type: WallpaperType.kCustomized,
      key: '//foo'
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify selected states.
    assertEquals(photoEls[0]!.selected, false);
    assertEquals(photoEls[1]!.selected, false);
  });

  test('selects photo', async () => {
    personalizationStore.setReducersEnabled(true);

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'}
    };

    // Initialize Google Photos data in the |personalizationStore|.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      '1': [photo]
    };

    // Initiallize |googlePhotosPhotosByAlbumIdElement|.
    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId, {hidden: false});
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', '1');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Verify that the expected |photo| is rendered.
    const photoSelector = 'wallpaper-grid-item:not([hidden]).photo';
    const photoEls = querySelectorAll(photoSelector) as WallpaperGridItem[];
    assertEquals(photoEls.length, 1);
    assertEquals(photoEls[0]!.imageSrc, photo.url.url);
    assertEquals(photoEls[0]!.primaryText, undefined);
    assertEquals(photoEls[0]!.secondaryText, undefined);

    // Select |photo| and verify selection started.
    photoEls[0]!.click();
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 1);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, true);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, photo);

    // Wait for and verify hard-coded selection failure.
    const methodName = 'selectGooglePhotosPhoto';
    assertEquals(await wallpaperProvider.whenCalled(methodName), photo.id);
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(personalizationStore.data.wallpaper.loading.setImage, 0);
    assertEquals(personalizationStore.data.wallpaper.loading.selected, false);
    assertEquals(personalizationStore.data.wallpaper.pendingSelected, null);
  });
}
