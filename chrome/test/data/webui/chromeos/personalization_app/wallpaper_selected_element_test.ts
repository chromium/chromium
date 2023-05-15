// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-selected component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CurrentWallpaper, DailyRefreshType, GooglePhotosSharedAlbumDialog, Paths, WallpaperLayout, WallpaperSelected, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertStringContains, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

const descriptionOptionsId = 'descriptionOptions';
const descriptionDialogId = 'descriptionDialog';
const dailyRefreshButtonId = 'dailyRefresh';

suite('WallpaperSelectedTest', function() {
  let wallpaperSelectedElement: WallpaperSelected|null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  async function clickDailyRefreshButton() {
    const button = wallpaperSelectedElement!.shadowRoot!.getElementById(
        dailyRefreshButtonId);
    button!.click();
    await waitAfterNextRender(wallpaperSelectedElement!);
  }

  async function clickSharedAlbumsDialogButton(id: string) {
    const dialog = wallpaperSelectedElement!.shadowRoot!
                       .querySelector<GooglePhotosSharedAlbumDialog>(
                           GooglePhotosSharedAlbumDialog.is);
    assertNotEquals(null, dialog, 'dialog element must exist to click button');
    const button = dialog!.shadowRoot!.getElementById(id);
    assertNotEquals(null, button, `button with id ${id} must exist`);
    button!.click();
    await waitAfterNextRender(wallpaperSelectedElement!);
  }

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
          selected: true,
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
          selected: false,
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
          selected: false,
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
    assertStringContains(
        img!.src,
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`);

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
      ...wallpaperProvider.currentWallpaper,
      attribution: [],
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
    assertStringContains(
        img!.src,
        `chrome://personalization/wallpaper.jpg?key=${
            wallpaperProvider.currentWallpaper.key}`);


    personalizationStore.data.wallpaper.currentSelected = {
      ...personalizationStore.data.wallpaper.currentSelected,
      key: 'new_key',
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertStringContains(
        img.src, `chrome://personalization/wallpaper.jpg?key=new_key`);
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
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement =
        initElement(WallpaperSelected, {'path': Paths.COLLECTION_IMAGES});
    await waitAfterNextRender(wallpaperSelectedElement);

    const dailyRefresh = wallpaperSelectedElement.shadowRoot!.getElementById(
        dailyRefreshButtonId);
    assertTrue(!!dailyRefresh);

    const refreshWallpaper =
        wallpaperSelectedElement.shadowRoot!.getElementById('refreshWallpaper');
    assertTrue(refreshWallpaper!.hidden);
  });

  test('hides daily refresh option for time of day collection', async () => {
    personalizationStore.data.wallpaper.currentSelected =
        wallpaperProvider.currentWallpaper;
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement = initElement(WallpaperSelected, {
      'path': Paths.COLLECTION_IMAGES,
      'collectionId': wallpaperProvider.timeOfDayCollectionId,
    });
    await waitAfterNextRender(wallpaperSelectedElement);

    const dailyRefresh = wallpaperSelectedElement.shadowRoot!.getElementById(
        dailyRefreshButtonId);
    assertTrue(dailyRefresh!.hidden);

    const refreshWallpaper =
        wallpaperSelectedElement.shadowRoot!.getElementById('refreshWallpaper');
    assertTrue(refreshWallpaper!.hidden);
  });

  test(
      'shows daily refresh option on non-empty google photos album view',
      async () => {
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.data.wallpaper.loading.selected = false;

        const album_id = 'test_album_id';
        wallpaperSelectedElement = initElement(WallpaperSelected, {
          'path': Paths.GOOGLE_PHOTOS_COLLECTION,
          'googlePhotosAlbumId': album_id,
        });
        await waitAfterNextRender(wallpaperSelectedElement);

        const dailyRefresh =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                dailyRefreshButtonId);
        assertTrue(!!dailyRefresh);

        const refreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertTrue(refreshWallpaper!.hidden);
      });

  test(
      'hides daily refresh option on empty google photos album view',
      async () => {
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
        personalizationStore.data.wallpaper.loading.selected = false;

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          'path': Paths.GOOGLE_PHOTOS_COLLECTION,
          'googlePhotosAlbumId': '',
        });
        await waitAfterNextRender(wallpaperSelectedElement);

        const dailyRefresh =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                dailyRefreshButtonId);
        assertTrue(dailyRefresh!.hidden);

        const refreshWallpaper =
            wallpaperSelectedElement.shadowRoot!.getElementById(
                'refreshWallpaper');
        assertTrue(refreshWallpaper!.hidden);
      });

  test(
      'shows refresh button only on collection with daily refresh enabled',
      async () => {
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
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
        personalizationStore.data.wallpaper.currentSelected =
            wallpaperProvider.currentWallpaper;
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
      attribution: [],
      descriptionContent: '',
      descriptionTitle: '',
      key: 'key',
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kOnceGooglePhotos,
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
      descriptionContent: '',
      descriptionTitle: '',
      key: 'key',
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kDefault,
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

  test(
      'shows google photos shared album confirmation dialog for daily refresh',
      async () => {
        loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
        const currentSelected: CurrentWallpaper = {
          attribution: ['testing attribution'],
          descriptionContent: '',
          descriptionTitle: '',
          key: 'key',
          layout: WallpaperLayout.kStretch,
          type: WallpaperType.kDefault,
        };
        personalizationStore.data.wallpaper.currentSelected = currentSelected;
        personalizationStore.data.wallpaper.loading.selected = false;
        const album_id = 'test_album_id';

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
          googlePhotosAlbumId: album_id,
          isGooglePhotosAlbumShared: true,
        });
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        await clickDailyRefreshButton();

        assertNotEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.querySelector(
                GooglePhotosSharedAlbumDialog.is),
            'dialog element exists');
      });

  test(
      'clicks cancel on the Google Photos shared album confirmation dialog',
      async () => {
        loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
        personalizationStore.data.wallpaper.currentSelected = {
          attribution: ['testing attribution'],
          descriptionContent: '',
          descriptionTitle: '',
          key: 'key',
          layout: WallpaperLayout.kStretch,
          type: WallpaperType.kDefault,
        };
        personalizationStore.data.wallpaper.loading.selected = false;
        const album_id = 'test_album_id';

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
          googlePhotosAlbumId: album_id,
          isGooglePhotosAlbumShared: true,
        });
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        await clickDailyRefreshButton();

        await clickSharedAlbumsDialogButton('close');

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.querySelector(
                GooglePhotosSharedAlbumDialog.is),
            'cancel button click closes dialog');
        assertEquals(
            0, wallpaperProvider.getCallCount('selectGooglePhotosAlbum'),
            'no requests to select album');
      });

  test(
      'clicks proceed on the Google Photos shared album confirmation dialog',
      async () => {
        loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
        personalizationStore.data.wallpaper.currentSelected = {
          attribution: ['testing attribution'],
          descriptionContent: '',
          descriptionTitle: '',
          key: 'key',
          layout: WallpaperLayout.kStretch,
          type: WallpaperType.kDefault,
        };
        personalizationStore.data.wallpaper.loading.selected = false;
        const album_id = 'test_album_id';

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
          googlePhotosAlbumId: album_id,
          isGooglePhotosAlbumShared: true,
        });
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        await clickDailyRefreshButton();

        await clickSharedAlbumsDialogButton('accept');

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.querySelector(
                GooglePhotosSharedAlbumDialog.is),
            'proceed button closes dialog');
        assertEquals(
            album_id,
            await wallpaperProvider.whenCalled('selectGooglePhotosAlbum'));
      });

  test('turns off daily refresh for Google Photos shared album', async () => {
    loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
    personalizationStore.data.wallpaper.currentSelected = {
      attribution: ['testing attribution'],
      descriptionContent: '',
      descriptionTitle: '',
      key: 'key',
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kDefault,
    };
    personalizationStore.data.wallpaper.loading.selected = false;
    const album_id = 'test_album_id';

    // Daily refresh is already enabled.
    personalizationStore.data.wallpaper.dailyRefresh = {
      id: album_id,
      type: DailyRefreshType.GOOGLE_PHOTOS,
    };

    wallpaperSelectedElement = initElement(WallpaperSelected, {
      path: Paths.GOOGLE_PHOTOS_COLLECTION,
      googlePhotosAlbumId: album_id,
      isGooglePhotosAlbumShared: true,
    });
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    await clickDailyRefreshButton();

    assertEquals(
        null,
        wallpaperSelectedElement.shadowRoot!.querySelector(
            GooglePhotosSharedAlbumDialog.is),
        'no dialog for turning off daily refresh');
  });

  test(
      'does not show confirmation dialog for google photos unshared album',
      async () => {
        loadTimeData.overrideValues({isGooglePhotosSharedAlbumsEnabled: true});
        personalizationStore.data.wallpaper.currentSelected = {
          attribution: ['testing attribution'],
          descriptionContent: '',
          descriptionTitle: '',
          key: 'key',
          layout: WallpaperLayout.kStretch,
          type: WallpaperType.kDefault,
        };
        personalizationStore.data.wallpaper.loading.selected = false;
        const album_id = 'test_album_id';

        wallpaperSelectedElement = initElement(WallpaperSelected, {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
          googlePhotosAlbumId: album_id,
          isGooglePhotosAlbumShared: false,
        });
        personalizationStore.notifyObservers();
        await waitAfterNextRender(wallpaperSelectedElement);

        await clickDailyRefreshButton();

        assertEquals(
            null,
            wallpaperSelectedElement.shadowRoot!.querySelector(
                GooglePhotosSharedAlbumDialog.is),
            'no dialog because album is not shared');
      });

  test('shows description options when wallpaper has description', async () => {
    loadTimeData.overrideValues({isPersonalizationJellyEnabled: true});
    personalizationStore.data.wallpaper.currentSelected = {
      attribution: ['testing'],
      descriptionContent: '',
      descriptionTitle: '',
      key: 'key',
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kDefault,
    };
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement = initElement(
        WallpaperSelected,
        {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
        },
    );
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        null,
        wallpaperSelectedElement.shadowRoot!.getElementById(
            descriptionOptionsId),
        'no description options present');

    personalizationStore.data.wallpaper.currentSelected = {
      ...personalizationStore.data.wallpaper.currentSelected,
      descriptionContent: 'content',
      descriptionTitle: 'title',
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertTrue(
        !!wallpaperSelectedElement.shadowRoot!.getElementById(
            descriptionOptionsId),
        'description options present');
  });

  test('clicking description options opens dialog', async () => {
    loadTimeData.overrideValues({isPersonalizationJellyEnabled: true});
    personalizationStore.data.wallpaper.currentSelected = {
      attribution: ['testing'],
      descriptionContent: 'content text',
      descriptionTitle: 'title text',
      key: 'key',
      layout: WallpaperLayout.kStretch,
      type: WallpaperType.kDefault,
    };
    personalizationStore.data.wallpaper.loading.selected = false;

    wallpaperSelectedElement = initElement(
        WallpaperSelected,
        {
          path: Paths.GOOGLE_PHOTOS_COLLECTION,
        },
    );
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        null,
        wallpaperSelectedElement.shadowRoot!.getElementById(
            descriptionDialogId),
        'no description dialog until button clicked');

    wallpaperSelectedElement.shadowRoot!.getElementById(descriptionOptionsId)!
        .querySelector('cr-button')!.click();
    await waitAfterNextRender(wallpaperSelectedElement);

    const dialog = wallpaperSelectedElement.shadowRoot!.getElementById(
        descriptionDialogId);
    assertTrue(!!dialog, 'dialog exists after button was clicked');

    assertEquals(
        'title text',
        dialog.querySelector<HTMLHeadingElement>(`h3[slot='title']`)!.innerText,
        'title text matches');
    assertEquals(
        'content text',
        dialog.querySelector<HTMLParagraphElement>(`p[slot='body']`)!.innerText,
        'content text matches');

    wallpaperSelectedElement.shadowRoot!.getElementById(
                                            'dialogCloseButton')!.click();
    await waitAfterNextRender(wallpaperSelectedElement);

    assertEquals(
        null,
        wallpaperSelectedElement.shadowRoot!.getElementById(
            descriptionDialogId),
        'no description dialog after close button clicked');
  });
});
