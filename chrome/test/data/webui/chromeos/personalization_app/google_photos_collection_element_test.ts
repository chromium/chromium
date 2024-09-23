// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {GooglePhotosAlbum, GooglePhotosCollectionElement, GooglePhotosEnablementState, Paths, PersonalizationRouterElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('GooglePhotosCollectionElementTest', function() {
  let googlePhotosCollectionElement: GooglePhotosCollectionElement|null;
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  /**
   * Returns an initialized `googlePhotosCollectionElement`.
   */
  async function displayElement(enabled = GooglePhotosEnablementState.kEnabled):
      Promise<GooglePhotosCollectionElement> {
    personalizationStore.data.wallpaper.googlePhotos.enabled = enabled;
    const googlePhotosCollectionElement =
        initElement(GooglePhotosCollectionElement);
    personalizationStore.notifyObservers();
    await waitAfterNextRender(googlePhotosCollectionElement);
    return Promise.resolve(googlePhotosCollectionElement);
  }

  /**
   * Returns the match for `selector` in `googlePhotosCollectionElement`'s
   * shadow DOM.
   */
  function querySelector(selector: string): HTMLElement|null {
    return googlePhotosCollectionElement!.shadowRoot!.querySelector(selector);
  }

  setup(() => {
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
    wallpaperProvider.setGooglePhotosAlbums(null);
    wallpaperProvider.setGooglePhotosPhotos([{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home',
    }]);
    googlePhotosCollectionElement = await displayElement();

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

  test('displays tabs and content for only albums', async () => {
    // NOTE: Intentionally set photos count to a non-zero value while setting an
    // empty array of photos to simulate an unlikely but possible scenario in
    // which server-side APIs temporarily disagree with one another.
    wallpaperProvider.setGooglePhotosPhotos([]);
    wallpaperProvider.setGooglePhotosAlbums([{
      id: '1',
      title: '',
      photoCount: 0,
      preview: {url: ''},
      timestamp: {internalValue: BigInt('1')},
      isShared: false,
    }]);
    googlePhotosCollectionElement = await displayElement();
    await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums');
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Photos tab should be present and selected.
    assertEquals(
        querySelector('#photosTab')?.getAttribute('aria-pressed'), 'true');

    // Photos content should be hidden.
    assertEquals(querySelector('#photosContent')?.hidden, true);

    // Albums tab should be present and *not* selected.
    assertEquals(
        querySelector('#albumsTab')?.getAttribute('aria-pressed'), 'false');

    // Albums content should be hidden.
    assertEquals(querySelector('#albumsContent')?.hidden, true);

    // Photos by album id content should be hidden.
    assertEquals(querySelector('#photosByAlbumIdContent')?.hidden, true);

    // Zero state should be present and visible.
    assertEquals(querySelector('#zeroState')?.hidden, false);

    // Click the albums tab.
    querySelector('#albumsTab')?.click();
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Photos tab should be present and *not* selected.
    assertEquals(
        querySelector('#photosTab')?.getAttribute('aria-pressed'), 'false');

    // Photos content should be hidden.
    assertEquals(querySelector('#photosContent')?.hidden, true);

    // Albums tab should be present and selected.
    assertEquals(
        querySelector('#albumsTab')?.getAttribute('aria-pressed'), 'true');

    // Albums content should be present and visible.
    assertEquals(querySelector('#albumsContent')?.hidden, false);

    // Photos by album id content should be hidden.
    assertEquals(querySelector('#photosByAlbumIdContent')?.hidden, true);

    // Zero state should be hidden.
    const zeroState = querySelector('#zeroState');
    assertTrue(!!zeroState);
    assertEquals(window.getComputedStyle(zeroState)!.display, 'none');
  });

  test('displays tabs and content for photos and albums', async () => {
    // Tabs and albums content are only displayed if albums are present.
    const albums: GooglePhotosAlbum[] = [{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      title: 'Album 0',
      photoCount: 1,
      preview: {url: 'foo.com'},
      timestamp: {internalValue: BigInt(`13318040939308000`)},
      isShared: false,
    }];
    wallpaperProvider.setGooglePhotosAlbums(albums);
    wallpaperProvider.setGooglePhotosPhotos([{
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home',
    }]);
    googlePhotosCollectionElement = await displayElement();

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
    wallpaperProvider.setGooglePhotosPhotos([]);
    googlePhotosCollectionElement = await displayElement();

    // Photos tab should be absent.
    assertEquals(querySelector('#photosTab'), null);

    // Photos content should be hidden.
    assertEquals(querySelector('#photosContent')?.hidden, true);

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

  test('displays zero state when photos by album id is empty', async () => {
    const album: GooglePhotosAlbum = {
      id: '1',
      title: '',
      photoCount: 0,
      isShared: false,
      preview: {url: ''},
      timestamp: {internalValue: BigInt(0)},
    };

    // Initialize Google Photos data in the `personalizationStore`.
    personalizationStore.data.wallpaper.googlePhotos.photosByAlbumId[album.id] =
        [];
    googlePhotosCollectionElement = await displayElement();
    googlePhotosCollectionElement.setAttribute('album-id', album.id);
    await waitAfterNextRender(googlePhotosCollectionElement);

    // Photos tab should be absent.
    assertEquals(querySelector('#photosTab'), null);

    // Photos content should be hidden.
    assertEquals(querySelector('#photosContent')?.hidden, true);

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

  test('fetches albums on first show', async () => {
    googlePhotosCollectionElement = await displayElement();

    // Albums *should* be fetched when shown.
    await wallpaperProvider.whenCalled('fetchGooglePhotosAlbums');
    wallpaperProvider.reset();

    // Tear down and re-init `googlePhotosCollectionElement`.
    await teardownElement(googlePhotosCollectionElement);
    googlePhotosCollectionElement = null;
    googlePhotosCollectionElement = await displayElement();

    // Albums should *not* be fetched when re-shown.
    await new Promise<void>(resolve => setTimeout(resolve, 100));
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosAlbums'), 0);
  });

  test('fetches photos on first show', async () => {
    googlePhotosCollectionElement = await displayElement();

    // Photos *should* be fetched when shown.
    await wallpaperProvider.whenCalled('fetchGooglePhotosPhotos');
    wallpaperProvider.reset();

    // Tear down and re-init `googlePhotosCollectionElement`.
    await teardownElement(googlePhotosCollectionElement);
    googlePhotosCollectionElement = null;
    googlePhotosCollectionElement = await displayElement();

    // Photos should *not* be fetched when re-shown.
    await new Promise<void>(resolve => setTimeout(resolve, 100));
    assertEquals(wallpaperProvider.getCallCount('fetchGooglePhotosPhotos'), 0);
  });

  test('sets aria label', async () => {
    googlePhotosCollectionElement = await displayElement();

    assertEquals(
        loadTimeData.getString('googlePhotosLabel'),
        googlePhotosCollectionElement.$.main.getAttribute('aria-label'));
  });

  [GooglePhotosEnablementState.kDisabled, GooglePhotosEnablementState.kEnabled,
   GooglePhotosEnablementState.kError]
      .forEach(
          enabled => test(
              'Redirects when Google Photos access is disabled.', async () => {
                googlePhotosCollectionElement = await displayElement(enabled);

                // Mock `PersonalizationRouter.reloadAtWallpaper()`.
                let didCallReloadAtWallpaper = false;
                PersonalizationRouterElement.reloadAtWallpaper = () => {
                  didCallReloadAtWallpaper = true;
                };

                // Select Google Photos collection.
                googlePhotosCollectionElement.setAttribute(
                    'path', Paths.GOOGLE_PHOTOS_COLLECTION);
                await waitAfterNextRender(googlePhotosCollectionElement);

                // Verify redirect expecations.
                assertEquals(
                    didCallReloadAtWallpaper,
                    enabled === GooglePhotosEnablementState.kDisabled);
              }));
});
