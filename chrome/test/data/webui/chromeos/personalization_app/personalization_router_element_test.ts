// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, Paths, PersonalizationRouterElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('PersonalizationRouterElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  test('will show ambient subpage if allowed', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    const routerElement = initElement(PersonalizationRouterElement);
    PersonalizationRouterElement.instance().goToRoute(Paths.AMBIENT);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertEquals(getComputedStyle(mainElement).display, 'none');

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertTrue(!!ambientSubpage);
    assertNotEquals(getComputedStyle(ambientSubpage).display, 'none');
  });

  test('will not show ambient subpage if disallowed', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': false});
    const routerElement = initElement(PersonalizationRouterElement);
    PersonalizationRouterElement.instance().goToRoute(Paths.AMBIENT);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertNotEquals(getComputedStyle(mainElement).display, 'none');

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertFalse(!!ambientSubpage);
  });

  test('returns to root page when wrong path is keyed in', async () => {
    loadTimeData.overrideValues({'isAmbientModeAllowed': true});
    const routerElement = initElement(
        PersonalizationRouterElement, {path: '/wrongpath', queryParams: {}});
    await waitAfterNextRender(routerElement);

    // Due to the wrong path, only shows root page.
    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertNotEquals(getComputedStyle(mainElement).display, 'none');

    // No breadcrumb element, ambient subpage, user subpage and wallpaper
    // subpage are shown.
    const breadcrumbElement =
        routerElement.shadowRoot!.querySelector('personalization-breadcrumb');
    assertFalse(!!breadcrumbElement);

    const ambientSubpage =
        routerElement.shadowRoot!.querySelector('ambient-subpage');
    assertFalse(!!ambientSubpage);

    const userSubpage = routerElement.shadowRoot!.querySelector('user-subpage');
    assertFalse(!!userSubpage);

    const wallpaperSubpage =
        routerElement.shadowRoot!.querySelector('wallpaper-subpage');
    assertFalse(!!wallpaperSubpage);
  });

  test('puts googlePhotosAlbumIsShared query param in url', async () => {
    loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
    const isSharedParam = 'googlePhotosAlbumIsShared';
    const sharedAlbum: GooglePhotosAlbum = {
      id: 'aaa',
      isShared: true,
      photoCount: 5,
      preview: {url: ''},
      timestamp: {internalValue: 0n},
      title: 'fake album',
    };
    const ownedAlbum = {...sharedAlbum, id: 'bbb', isShared: false};

    const photo: GooglePhotosPhoto = {
      id: '9bd1d7a3-f995-4445-be47-53c5b58ce1cb',
      dedupKey: '2d0d1595-14af-4471-b2db-b9c8eae3a491',
      name: 'foo',
      date: {data: []},
      url: {url: 'foo.com'},
      location: 'home',
    };
    wallpaperProvider.setGooglePhotosSharedAlbums([sharedAlbum]);
    wallpaperProvider.setGooglePhotosAlbums([ownedAlbum]);
    wallpaperProvider.setGooglePhotosEnabled(
        GooglePhotosEnablementState.kEnabled);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(sharedAlbum.id, [photo]);
    wallpaperProvider.setGooglePhotosPhotosByAlbumId(ownedAlbum.id, [photo]);

    personalizationStore.setReducersEnabled(true);

    // Start at root page.
    const routerElement = initElement(PersonalizationRouterElement);
    await waitAfterNextRender(routerElement);

    // Navigate to wallpaper collections list.
    routerElement.goToRoute(Paths.COLLECTIONS);
    await wallpaperProvider.whenCalled('fetchGooglePhotosEnabled');

    // Navigate to google photos page.
    routerElement.goToRoute(Paths.GOOGLE_PHOTOS_COLLECTION);
    await wallpaperProvider.whenCalled('fetchGooglePhotosSharedAlbums');

    let params = new URLSearchParams(location.search);
    assertFalse(params.has(isSharedParam), 'param not set yet');

    // Select a shared album should put the param in query string.
    routerElement.selectGooglePhotosAlbum(sharedAlbum);
    await waitAfterNextRender(routerElement);

    params = new URLSearchParams(location.search);
    assertTrue(params.has(isSharedParam), 'param exists');
    assertEquals('true', params.get(isSharedParam), 'true value set');

    // Select owned album removes the param.
    routerElement.selectGooglePhotosAlbum(ownedAlbum);
    await waitAfterNextRender(routerElement);

    params = new URLSearchParams(location.search);
    assertFalse(params.has(isSharedParam), 'param no longer exists');
    assertEquals(null, params.get(isSharedParam), 'does not exist so null');
  });
});
