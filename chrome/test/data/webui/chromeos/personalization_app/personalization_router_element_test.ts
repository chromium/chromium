// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';

import {GooglePhotosAlbum, GooglePhotosEnablementState, GooglePhotosPhoto, Paths, PersonalizationRouterElement, SeaPenTermsOfServiceDialogElement} from 'chrome://personalization/js/personalization_app.js';
import {SeaPenTemplateId} from 'chrome://resources/ash/common/sea_pen/sea_pen_generated.mojom-webui.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('PersonalizationRouterElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;
  let seaPenProvider: TestSeaPenProvider;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
    seaPenProvider = mocks.seaPenProvider;
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
    const routerElement = initElement(PersonalizationRouterElement);
    routerElement.goToRoute('/wrongpath' as Paths, {});
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

  test('hides SeaPen from ineligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: false});

    const routerElement = initElement(PersonalizationRouterElement, {});

    for (const path of [Paths.SEA_PEN_COLLECTION, Paths.SEA_PEN_RESULTS]) {
      PersonalizationRouterElement.instance().goToRoute(path);
      await waitAfterNextRender(routerElement);

      // Due to the forbidden path, only shows root page.
      const mainElement =
          routerElement.shadowRoot!.querySelector('personalization-main');
      assertTrue(!!mainElement, 'main element exists');
      assertNotEquals(
          getComputedStyle(mainElement).display, 'none',
          'main element is shown');

      const seaPenRouterElement =
          routerElement.shadowRoot!.querySelector('sea-pen-router');
      assertFalse(!!seaPenRouterElement, 'sea-pen-router does not exist');
    }
  });

  test('shows SeaPen for eligible users', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});

    const routerElement = initElement(PersonalizationRouterElement);
    await waitAfterNextRender(routerElement);

    let seaPenRouterElement =
        routerElement.shadowRoot!.querySelector('sea-pen-router');
    assertFalse(!!seaPenRouterElement, 'sea-pen-router does not exist');

    routerElement.goToRoute(Paths.SEA_PEN_COLLECTION);
    await waitAfterNextRender(routerElement);

    const mainElement =
        routerElement.shadowRoot!.querySelector('personalization-main');
    assertTrue(!!mainElement);
    assertEquals(
        getComputedStyle(mainElement).display, 'none',
        'main element is hidden');

    seaPenRouterElement =
        routerElement.shadowRoot!.querySelector('sea-pen-router');
    assertTrue(!!seaPenRouterElement, 'sea-pen-router now exists');
    assertNotEquals(
        getComputedStyle(seaPenRouterElement).display, 'none',
        'sea-pen-router is shown');
  });

  test('shows wallpaper selected in SeaPen', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});

    const routerElement = initElement(PersonalizationRouterElement);
    await waitAfterNextRender(routerElement);

    routerElement.goToRoute(Paths.SEA_PEN_COLLECTION);
    await waitAfterNextRender(routerElement);

    const seaPenRouterElement =
        routerElement.shadowRoot!.querySelector('sea-pen-router');
    assertTrue(!!seaPenRouterElement, 'sea-pen-router now exists');
    assertNotEquals(
        getComputedStyle(seaPenRouterElement).display, 'none',
        'sea-pen-router is shown');

    const wallpaperSelected =
        routerElement.shadowRoot!.getElementById('wallpaperSelected');
    assertTrue(!!wallpaperSelected);
    assertNotEquals(
        getComputedStyle(wallpaperSelected).display, 'none',
        'sea-pen-router shows wallpaper-selected');
  });

  test('hides wallpaper selected on non root path sea pen', async () => {
    loadTimeData.overrideValues({isSeaPenEnabled: true});

    const routerElement = initElement(PersonalizationRouterElement);
    await waitAfterNextRender(routerElement);

    routerElement.goToRoute(Paths.SEA_PEN_COLLECTION, {
      seaPenTemplateId: SeaPenTemplateId.kFlower.toString(),
    });
    await waitAfterNextRender(routerElement);

    const seaPenRouterElement =
        routerElement.shadowRoot!.querySelector('sea-pen-router');
    assertTrue(!!seaPenRouterElement, 'sea-pen-router now exists');
    assertNotEquals(
        getComputedStyle(seaPenRouterElement).display, 'none',
        'sea-pen-router is shown');

    const wallpaperSelected =
        routerElement.shadowRoot!.getElementById('wallpaperSelected');
    assertFalse(
        !!wallpaperSelected, 'wallpaper-selected should not be displayed');
  });

  test(
      'refuses SeaPen wallpaper terms and routes back to Wallpaper subpage',
      async () => {
        loadTimeData.overrideValues({isSeaPenEnabled: true});
        personalizationStore.setReducersEnabled(true);

        const routerElement = initElement(PersonalizationRouterElement);
        await waitAfterNextRender(routerElement);

        routerElement.goToRoute(Paths.SEA_PEN_COLLECTION);
        await seaPenProvider.whenCalled('shouldShowSeaPenTermsOfServiceDialog');
        await waitAfterNextRender(routerElement);

        let seaPenRouterElement =
            routerElement.shadowRoot!.querySelector('sea-pen-router');
        assertTrue(!!seaPenRouterElement, 'sea-pen-router exists');
        assertNotEquals(
            getComputedStyle(seaPenRouterElement).display, 'none',
            'sea-pen-router is shown');

        const seaPenTermsDialog = seaPenRouterElement.shadowRoot!.querySelector(
            SeaPenTermsOfServiceDialogElement.is);
        assertTrue(
            !!seaPenTermsDialog, 'SeaPen terms of service dialog is displayed');

        const button = seaPenTermsDialog!.shadowRoot!.getElementById('refuse');
        assertTrue(!!button, `refuse must exist`);
        button!.click();
        await waitAfterNextRender(routerElement!);

        seaPenRouterElement =
            routerElement.shadowRoot!.querySelector('sea-pen-router');
        assertFalse(!!seaPenRouterElement, 'sea-pen-router no longer exists');

        assertEquals(
            Paths.COLLECTIONS,
            routerElement.shadowRoot?.querySelector('iron-location')?.path,
            'redirect to Wallpaper subpage');
      });

});
