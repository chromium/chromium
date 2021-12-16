// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GooglePhotosCollection} from 'chrome://personalization/trusted/wallpaper/google_photos_collection_element.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function GooglePhotosCollectionTest() {
  /** @type {?HTMLElement} */
  let googlePhotosCollectionElement = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /**
   * Returns the match for |selector| in |googlePhotosCollectionElement|'s
   * shadow DOM.
   * @return {?Element|undefined}
   */
  function querySelector(selector) {
    return googlePhotosCollectionElement?.shadowRoot?.querySelector(selector);
  }

  setup(() => {
    // Google Photos strings are only supplied when the Google Photos
    // integration feature flag is enabled.
    loadTimeData.overrideValues({
      'googlePhotosLabel': 'Google Photos',
      'googlePhotosAlbumsTabLabel': 'Albums',
      'googlePhotosPhotosTabLabel': 'Photos',
      'googlePhotosZeroStateMessage':
          'No image available. To add photos, go to $1',
    });

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    await teardownElement(googlePhotosCollectionElement);
    googlePhotosCollectionElement = null;
  });

  test('displays only photos content', async () => {
    // Tabs and albums content are not displayed if albums are absent.
    personalizationStore.data.wallpaper.googlePhotos.albums = null;
    personalizationStore.data.wallpaper.googlePhotos.photos =
        Array.from({length: 1});
    personalizationStore.data.wallpaper.loading.googlePhotos.albums = false;
    personalizationStore.data.wallpaper.loading.googlePhotos.photos = false;

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection.is, {hidden: false});
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Zero state should be absent.
    assertEquals(querySelector('#zeroState'), null);

    // Tabs should be absent.
    assertEquals(querySelector('#tabStrip'), null);

    // Photos content should be present and visible.
    const photosContent = querySelector('#photosContent');
    assertTrue(!!photosContent);
    assertFalse(photosContent.hidden);

    // Albums content should be absent.
    assertEquals(querySelector('#albumsContent'), null);

    // Photos by album id content should be absent.
    assertEquals(querySelector('#photosByAlbumId'), null);
  });

  test('displays tabs and content for photos and albums', async () => {
    // Tabs and albums content are only displayed if albums are present.
    personalizationStore.data.wallpaper.googlePhotos.albums =
        Array.from({length: 1});
    personalizationStore.data.wallpaper.googlePhotos.photos =
        Array.from({length: 1});
    personalizationStore.data.wallpaper.loading.googlePhotos.albums = false;
    personalizationStore.data.wallpaper.loading.googlePhotos.photos = false;

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection.is, {hidden: false});
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Zero state should be absent.
    assertEquals(querySelector('#zeroState'), null);

    // Tab strip should be present and visible.
    const tabStrip = querySelector('#tabStrip');
    assertTrue(!!tabStrip);
    assertFalse(tabStrip.hidden);

    // Photos tab should be present, visible, and pressed.
    const photosTab = querySelector('#photosTab');
    assertTrue(!!photosTab);
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'true');

    // Photos content should be present and visible.
    const photosContent = querySelector('#photosContent');
    assertTrue(!!photosContent);
    assertFalse(photosContent.hidden);

    // Albums tab should be present, visible, and *not* pressed.
    const albumsTab = querySelector('#albumsTab');
    assertTrue(!!albumsTab);
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'false');

    // Albums content should be present and hidden.
    const albumsContent = querySelector('#albumsContent');
    assertTrue(!!albumsContent);
    assertTrue(albumsContent.hidden);

    // Photos by album id content should be present and hidden.
    const photosByAlbumIdContent = querySelector('#photosByAlbumIdContent');
    assertTrue(!!photosByAlbumIdContent);
    assertTrue(photosByAlbumIdContent.hidden);

    // Clicking the albums tab should cause:
    // * albums tab to be visible and pressed.
    // * albums content to be visible.
    // * photos tab to be visible and *not* pressed.
    // * photos content to be hidden.
    // * photos by album id content to be hidden.
    albumsTab.click();
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'true');
    assertFalse(albumsContent.hidden);
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'false');
    assertTrue(photosContent.hidden);
    assertTrue(photosByAlbumIdContent.hidden);

    // Selecting an album should cause:
    // * tab strip to be hidden.
    // * photos by album id content to be visible.
    // * albums content to be hidden.
    // * photos content to be hidden.
    googlePhotosCollectionElement.setAttribute('album-id', '1');
    await waitAfterNextRender(googlePhotosCollectionElement);
    assertEquals(window.getComputedStyle(tabStrip).display, 'none');
    assertFalse(photosByAlbumIdContent.hidden);
    assertTrue(albumsContent.hidden);
    assertTrue(photosContent.hidden);

    // Un-selecting an album should cause:
    // * tab strip to be visible.
    // * photos by album id content to be hidden.
    // * albums content to be visible.
    // * photos content to be hidden.
    googlePhotosCollectionElement.removeAttribute('album-id');
    await waitAfterNextRender(googlePhotosCollectionElement);
    assertEquals(window.getComputedStyle(tabStrip).display, 'block');
    assertTrue(photosByAlbumIdContent.hidden);
    assertFalse(albumsContent.hidden);
    assertTrue(photosContent.hidden);

    // Clicking the photos tab should cause:
    // * photos tab to be visible and pressed.
    // * photos content to be visible.
    // * albums tab to be visible and *not* pressed.
    // * albums content to be hidden.
    // * photos by album id content to be hidden.
    photosTab.click();
    assertFalse(photosTab.hidden);
    assertEquals(photosTab.getAttribute('aria-pressed'), 'true');
    assertFalse(photosContent.hidden);
    assertFalse(albumsTab.hidden);
    assertEquals(albumsTab.getAttribute('aria-pressed'), 'false');
    assertTrue(albumsContent.hidden);
    assertTrue(photosByAlbumIdContent.hidden);
  });

  test('displays zero state when there is no content', async () => {
    personalizationStore.data.wallpaper.googlePhotos.albums = [];
    personalizationStore.data.wallpaper.googlePhotos.photos = [];
    personalizationStore.data.wallpaper.loading.googlePhotos.albums = false;
    personalizationStore.data.wallpaper.loading.googlePhotos.photos = false;

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection.is, {hidden: false});
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Photos tab should be absent.
    assertEquals(querySelector('#photosTab'), null);

    // Photos content should be absent.
    assertEquals(querySelector('#photosContent'), null);

    // Albums tab should be absent.
    assertEquals(querySelector('#albumsTab'), null);

    // Albums content should be absent.
    assertEquals(querySelector('#albumsContent'), null);

    // Photos by album id content should be absent.
    assertEquals(querySelector('#photosByAlbumIdContent'), null);

    // Zero state should be present and visible.
    const zeroState = querySelector('#zeroState');
    assertTrue(!!zeroState);
    assertFalse(zeroState.hidden);
  });
}
