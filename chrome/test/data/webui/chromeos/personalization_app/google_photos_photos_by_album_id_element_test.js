// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotosPhotosByAlbumId, promisifyWallpaperControllerFunctionsForTesting} from 'chrome://personalization/trusted/wallpaper/google_photos_photos_by_album_id_element.js';

import {assertEquals} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function GooglePhotosPhotosByAlbumIdTest() {
  /** @type {?HTMLElement} */
  let googlePhotosPhotosByAlbumIdElement = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /**
   * Returns the match for |selector| in |googlePhotosPhotosByAlbumIdElement|'s
   * shadow DOM.
   * @return {?Element|undefined}
   */
  function querySelector(selector) {
    return googlePhotosPhotosByAlbumIdElement?.shadowRoot?.querySelector(
        selector);
  }

  /**
   * Returns all matches for |selector| in
   * |googlePhotosPhotosByAlbumIdElement|'s shadow DOM.
   * @return {?Array<Element>}
   */
  function querySelectorAll(selector) {
    const matches =
        googlePhotosPhotosByAlbumIdElement?.shadowRoot?.querySelectorAll(
            selector);
    return matches ? [...matches] : null;
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(googlePhotosPhotosByAlbumIdElement);
    googlePhotosPhotosByAlbumIdElement = null;
  });

  test('displays photos for the selected album id', async () => {
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {};
    personalizationStore.data.wallpaper.loading.googlePhotos
        .photosByAlbumId = {};

    googlePhotosPhotosByAlbumIdElement =
        initElement(GooglePhotosPhotosByAlbumId.is, {hidden: false});
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);

    // Initially no album id selected. Photos should be absent.
    const photoSelector = 'wallpaper-grid-item:not([hidden]) .photo';
    assertEquals(querySelectorAll(photoSelector).length, 0);

    const {fetchGooglePhotosAlbum: fetchGooglePhotosAlbumPromise} =
        promisifyWallpaperControllerFunctionsForTesting();

    // Select an album id. Photos should be absent since they are not loaded.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', '1');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 0);

    // Expect a request to load photos for the selected album id.
    const [, , albumId] = await fetchGooglePhotosAlbumPromise;
    assertEquals(albumId, '1');

    // Start loading photos for the selected album id. Photos should still be
    // absent since they are still not loaded.
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '1': true,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 0);

    // Load photos for an album id other than that which is selected. Photos
    // should still be absent since they are still not loaded for the selected
    // album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      '2': Array.from({length: 50}),
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '2': false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 0);

    // Finish loading photos for the selected album id. Photos should now be
    // present since they are finished loading for the selected album id.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId,
      '1': Array.from({length: 100}),
    };
    personalizationStore.data.wallpaper.loading.googlePhotos.photosByAlbumId = {
      ...personalizationStore.data.wallpaper.loading.googlePhotos
          .photosByAlbumId,
      '1': false,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 100);

    // Select the other album id for which data is already loaded. Photos should
    // immediately update since they are already loaded for the selected album
    // id.
    googlePhotosPhotosByAlbumIdElement.setAttribute('album-id', '2');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 50);

    // Un-select the album id. Photos should be absent since no album id is
    // selected.
    googlePhotosPhotosByAlbumIdElement.removeAttribute('album-id');
    await waitAfterNextRender(googlePhotosPhotosByAlbumIdElement);
    assertEquals(querySelectorAll(photoSelector).length, 0);
  });
}
