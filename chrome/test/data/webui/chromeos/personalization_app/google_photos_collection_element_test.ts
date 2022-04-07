// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {GooglePhotosAlbum, GooglePhotosCollection, GooglePhotosEnablementState, Paths, PersonalizationRouter} from 'chrome://personalization/trusted/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosCollectionTest', function() {
  let googlePhotosCollectionElement: GooglePhotosCollection|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns the match for |selector| in |googlePhotosCollectionElement|'s
   * shadow DOM.
   */
  function querySelector(selector: string): HTMLElement|null {
    return googlePhotosCollectionElement!.shadowRoot!.querySelector(selector);
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
      'isGooglePhotosIntegrationEnabled': true,
    });

    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    personalizationStore.setReducersEnabled(true);
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(googlePhotosCollectionElement);
    googlePhotosCollectionElement = null;
  });

  test('displays only photos content', async () => {
    // Tabs and albums content are not displayed if albums are absent.
    wallpaperProvider.setGooglePhotosAlbums(undefined);
    wallpaperProvider.setGooglePhotosCount(1);
    wallpaperProvider.setGooglePhotosPhotos([{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    }]);

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection, {hidden: false});
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
    const albums: GooglePhotosAlbum[] = [{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      title: 'Album 0',
      photoCount: 1,
      preview: {url: 'foo.com'}
    }];
    wallpaperProvider.setGooglePhotosAlbums(albums);
    wallpaperProvider.setGooglePhotosCount(1);
    wallpaperProvider.setGooglePhotosPhotos([{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home'
    }]);

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection, {hidden: false});
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
    googlePhotosCollectionElement.setAttribute('album-id', albums[0]!.id);
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
    assertEquals(window.getComputedStyle(tabStrip!).display, 'block');
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
    wallpaperProvider.setGooglePhotosAlbums([]);
    wallpaperProvider.setGooglePhotosCount(0);
    wallpaperProvider.setGooglePhotosPhotos([]);

    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection, {hidden: false});
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

  test('sets google photos aria label', async () => {
    googlePhotosCollectionElement =
        initElement(GooglePhotosCollection, {hidden: false});
    await waitAfterNextRender(googlePhotosCollectionElement);

    assertEquals(
        loadTimeData.getString('googlePhotosLabel'),
        googlePhotosCollectionElement.$.main.getAttribute('aria-label'),
        'google photos main aria label is set');
  });

  [GooglePhotosEnablementState.kDisabled, GooglePhotosEnablementState.kEnabled,
   GooglePhotosEnablementState.kError]
      .forEach(
          enabled => test(
              'Redirects when Google Photos access is disabled.', async () => {
                // Set values returned by |wallpaperProvider|.
                wallpaperProvider.setGooglePhotosEnabled(enabled);

                // Initialize |googlePhotosCollectionElement|.
                googlePhotosCollectionElement =
                    initElement(GooglePhotosCollection, {hidden: false});
                await waitAfterNextRender(googlePhotosCollectionElement);

                // Mock |PersonalizationRouter.reloadAtWallpaper()|.
                let didCallReloadAtWallpaper = false;
                PersonalizationRouter.reloadAtWallpaper = () => {
                  didCallReloadAtWallpaper = true;
                };

                // Select Google Photos collection.
                googlePhotosCollectionElement.setAttribute(
                    'path', Paths.GooglePhotosCollection);
                await waitAfterNextRender(googlePhotosCollectionElement);

                // Verify redirect expecations.
                assertEquals(
                    didCallReloadAtWallpaper,
                    enabled === GooglePhotosEnablementState.kDisabled);
              }));
});
