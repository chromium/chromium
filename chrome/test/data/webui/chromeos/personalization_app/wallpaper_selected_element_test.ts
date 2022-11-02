// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CurrentWallpaper, DailyRefreshType, Paths, WallpaperLayout, WallpaperSelected, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperSelectedTest', function() {
  let wallpaperSelectedElement: WallpaperSelected|null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperSelectedElement) {
      wallpaperSelectedElement.remove();
    }
    wallpaperSelectedElement = null;
    await flushTasks();
  });

  test(
      'shows loading placeholder when there are in-flight requests',
      async () => {
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 1,
          setImage: 0,
        };
        wallpaperSelectedElement = initElement(WallpaperSelected);

        assertEquals(
            null, wallpaperSelectedElement.shadowRoot!.querySelector('img'));

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'textContainer'));

        const placeholder = wallpaperSelectedElement.shadowRoot!.getElementById(
            'imagePlaceholder');

        assertTrue(!!placeholder);

        // Loading placeholder should be hidden.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 0,
        };
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('none', placeholder.style.display);

        // Sent a request to update user wallpaper. Loading placeholder should
        // come back.
        personalizationStore.data.wallpaper.loading = {
          ...personalizationStore.data.wallpaper.loading,
          selected: 0,
          setImage: 1,
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        assertEquals('', placeholder.style.display);
      });

  test('shows wallpaper image and attribution when loaded', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;

    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img');
    assertEquals(
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`,
        img!.src, 'sets current wallpaper key appended to url');

    const textContainerElements =
        wallpaperSelectedElement.shadowRoot!.querySelectorAll(
            '#textContainer span');

    // First span tag is 'Currently Set' text.
    assertEquals('currentlySet', textContainerElements[0]!.id);
    assertEquals(
        wallpaperSelectedElement.i18n('currentlySet'),
        textContainerElements[0]!.textContent);

    // Following text elements are for the photo attribution text.
    const attributionLines =
        Array.from(textContainerElements).slice(1) as HTMLElement[];

    assertEquals(
        wallpaperProvider.currentWallpaper.attribution.length,
        attributionLines.length);
    wallpaperProvider.currentWallpaper.attribution.forEach((line, i) => {
      assertEquals(line, attributionLines[i]!.innerText);
    });
  });

  test('shows unknown for empty attribution', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const title =
        wallpaperSelectedElement.shadowRoot!.getElementById('imageTitle');
    assertEquals(
        wallpaperSelectedElement.i18n('unknownImageAttribution'),
        title!.textContent!.trim());
  });

  test('updates image when store is updated', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    const img = wallpaperSelectedElement.shadowRoot!.querySelector('img') as
        HTMLImageElement;
    assertEquals(
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`,
        img!.src, 'sets current wallpaper key appended to url');


    personalizationStore.data.wallpaper.currentSelected = {
      ...personalizationStore.data.wallpaper.currentSelected,
      key: 'new_key',
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        `chrome://personalization/wallpaper.jpg?key=new_key`, img.src,
        'updates wallpaper key query parameter');
  });

  test('shows placeholders when image fails to load', async () => {
    wallpaperSelectedElement = initElement(WallpaperSelected);
    await waitAfterNextRender(wallpaperSelectedElement);

    // Still loading.
    personalizationStore.data.wallpaper.loading.selected = true;
    personalizationStore.data.wallpaper.currentSelected = null;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    const placeholder =
        wallpaperSelectedElement.shadowRoot!.getElementById('imagePlaceholder');
    assertTrue(!!placeholder);

    // Loading finished and still no current wallpaper.
    personalizationStore.data.wallpaper.loading.selected = false;
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    // Dom-if will set display: none if the element is hidden. Make sure it is
    // not hidden.
    assertNotEquals('none', placeholder.style.display);
    assertEquals(
        null, wallpaperSelectedElement.shadowRoot!.querySelector('img'));
  });

  test('shows daily refresh option on the collection view', async () => {
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'data:image/png;base64,abc='},
      attribution: [],
      assetId: BigInt(100),
    };
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement =
        initElement(WallpaperSelected, {'path': Paths.COLLECTION_IMAGES});
    await waitAfterNextRender(wallpaperSelectedElement);

    const dailyRefresh =
        wallpaperSelectedElement.shadowRoot!.getElementById('dailyRefresh');
    assertTrue(!!dailyRefresh);

    const refreshWallpaper =
        wallpaperSelectedElement.shadowRoot!.getElementById('refreshWallpaper');
    assertTrue(refreshWallpaper!.hidden);
  });

  test(
      'shows daily refresh option on the google photos album view',
      async () => {
        personalizationStore.data.wallpaper.currentSelected = {
          url: {url: 'data:image/png;base64,abc='},
          attribution: [],
          assetId: BigInt(100),
        };
        personalizationStore.data.wallpaper.loading.selected = false;

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          'path': Paths.GOOGLE_PHOTOS_COLLECTION,
          'googlePhotosAlbumId': '',
        });
        await waitAfterNextRender(wallpaperSelectedElement);

        const dailyRefresh =
            wallpaperSelectedElement.shadowRoot!.getElementById('dailyRefresh');
        assertTrue(!!dailyRefresh);

        const refreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertTrue(refreshWallpaper!.hidden);
      });

  test(
      'shows refresh button only on collection with daily refresh enabled',
      async () => {
        personalizationStore.data.wallpaper.currentSelected = {
          url: {url: 'data:image/png;base64,abc='},
          attribution: [],
          assetId: BigInt(100),
        };
        personalizationStore.data.wallpaper.loading.selected = false;
        const collection_id = wallpaperProvider.collections![0]!.id;
        personalizationStore.data.wallpaper.dailyRefresh = {
          id: collection_id,
          type: DailyRefreshType.BACKDROP,
        };

        wallpaperSelectedElement = initElement(
            WallpaperSelected,
            {'path': Paths.COLLECTION_IMAGES, 'collectionId': collection_id});
        personalizationStore.notifyObservers();

        await waitAfterNextRender(wallpaperSelectedElement);

        const newRefreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertFalse(newRefreshWallpaper!.hidden);
      });

  test(
      'shows refresh button only on google photos album with daily refresh enabled',
      async () => {
        personalizationStore.data.wallpaper.currentSelected = {
          url: {url: 'data:image/png;base64,abc='},
          attribution: [],
          assetId: BigInt(100),
        };
        personalizationStore.data.wallpaper.loading.selected = false;

        const album_id = 'test_album_id';
        personalizationStore.data.wallpaper.dailyRefresh = {
          id: album_id,
          type: DailyRefreshType.GOOGLE_PHOTOS,
        };

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          'path': Paths.GOOGLE_PHOTOS_COLLECTION,
          'googlePhotosAlbumId': album_id,
        });
        personalizationStore.notifyObservers();

        await waitAfterNextRender(wallpaperSelectedElement);

        const newRefreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertFalse(newRefreshWallpaper!.hidden);
      });

  test('shows layout options for Google Photos', async () => {
    // Set a Google Photos photo as current wallpaper.
    personalizationStore.data.wallpaper.currentSelected = {
      url: {url: 'url'},
      attribution: [],
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kOnceGooglePhotos,
      key: 'key',
    };

    // Initialize |wallpaperSelectedElement|.
    wallpaperSelectedElement =
        initElement(WallpaperSelected, {'path': Paths.COLLECTION_IMAGES});
    await waitAfterNextRender(wallpaperSelectedElement);

    // Verify layout options are *not* shown when not on Google Photos path.
    const selector = '#wallpaperOptions';
    const shadowRoot = wallpaperSelectedElement.shadowRoot;
    assertEquals(shadowRoot?.querySelector(selector), null);

    // Set Google Photos path and verify layout options *are* shown.
    wallpaperSelectedElement.path = Paths.GOOGLE_PHOTOS_COLLECTION;
    await waitAfterNextRender(wallpaperSelectedElement);
    assertNotEquals(shadowRoot?.querySelector(selector), null);

    // Verify that clicking layout |button| results in mojo API call.
    const button = shadowRoot?.querySelector('#center') as HTMLElement | null;
    button?.click();
    assertDeepEquals(
        await wallpaperProvider.whenCalled('setCurrentWallpaperLayout'),
        WallpaperLayout.kCenter);
  });

  test('shows attribution for device default wallpaper', async () => {
    const currentSelected: CurrentWallpaper = {
      attribution: ['testing attribution'],
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kDefault,
      key: 'key',
    };
    personalizationStore.data.wallpaper.currentSelected = currentSelected;

    wallpaperSelectedElement =
        initElement(WallpaperSelected, {path: Paths.COLLECTION_IMAGES});
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        wallpaperSelectedElement.i18n('defaultWallpaper'),
        wallpaperSelectedElement.shadowRoot!.getElementById(
                                                'imageTitle')!.innerText,
        'default wallpaper attribution is shown');
  });
});
