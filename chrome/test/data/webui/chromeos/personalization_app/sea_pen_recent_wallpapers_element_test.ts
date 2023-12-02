// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {emptyState, SeaPenActionName, SeaPenRecentWallpapersElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('SeaPenRecentWallpapersElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let wallpaperProvider: TestWallpaperProvider;
  let seaPenRecentWallpapersElement: SeaPenRecentWallpapersElement|null;

  function getLoadingPlaceholders(): WallpaperGridItemElement[] {
    if (!seaPenRecentWallpapersElement) {
      return [];
    }

    return Array.from(seaPenRecentWallpapersElement.shadowRoot!
                          .querySelectorAll<WallpaperGridItemElement>(
                              'div:not([hidden]) .recent-image[placeholder]'));
  }

  function getDisplayedRecentImages(): WallpaperGridItemElement[] {
    if (!seaPenRecentWallpapersElement) {
      return [];
    }

    return Array.from(
        seaPenRecentWallpapersElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                'div:not([hidden]) .recent-image:not([placeholder])'));
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenRecentWallpapersElement);
    seaPenRecentWallpapersElement = null;
    await flushTasks();
  });

  test('displays recently used Sea Pen wallpapers', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImages;
    personalizationStore.data.wallpaper.seaPen.recentImageData =
        seaPenProvider.recentImageData;
    personalizationStore.data.wallpaper.loading.seaPen = {
      recentImages: false,
      recentImageData: {
        '/sea_pen/111.jpg': false,
        '/sea_pen/222.jpg': false,
        '/sea_pen/333.jpg': false,
      },
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentImages = getDisplayedRecentImages();
    assertEquals(3, recentImages.length);

    // The menu button for each Sea Pen wallpaper should display.
    const menuIconButtons =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .menu-icon-container .menu-icon-button');
    assertEquals(
        3, menuIconButtons!.length, 'should be 3 menu icon buttons available.');

    // All menu options for all images should be closed.
    const actionMenus =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .action-menu-container');
    assertEquals(3, actionMenus!.length, 'should be 3 action menus available.');
    actionMenus.forEach(function(actionMenu: Element, i: number) {
      const menuDialog =
          (actionMenu as HTMLElement).shadowRoot!.querySelector('dialog') as
          HTMLDialogElement;
      assertFalse(!!menuDialog!.open, `menu dialog ${i} should be closed.`);
    });
  });


  test(
      'loads recently used Sea Pen wallpapers and saves to store', async () => {
        assertDeepEquals(emptyState(), personalizationStore.data);

        personalizationStore.setReducersEnabled(true);
        personalizationStore.expectAction(
            SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES);
        personalizationStore.expectAction(
            SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA);

        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);

        await personalizationStore.waitForAction(
            SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES);
        await personalizationStore.waitForAction(
            SeaPenActionName.SET_RECENT_SEA_PEN_IMAGE_DATA);

        assertEquals(
            seaPenProvider.recentImages,
            personalizationStore.data.wallpaper.seaPen.recentImages,
            'expected recent images are set');
        for (const [key, value] of Object.entries(
                 personalizationStore.data.wallpaper.seaPen.recentImageData)) {
          assertTrue(
              seaPenProvider.recentImageData.hasOwnProperty(key),
              `expected image data for file path ${key} is set`);
          assertDeepEquals(
              seaPenProvider.recentImageData[key]!.url, value!.url,
              `expected url for file path ${key} is set`);
        }
      });

  test(
      'displays placeholder tiles with no src for unloaded local images',
      async () => {
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImages;
        personalizationStore.data.wallpaper.seaPen.recentImageData =
            seaPenProvider.recentImageData;

        // No image data loaded.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {},
        };

        // Initialize |seaPenRecentWallpapersElement|.
        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Iron-list creates some extra dom elements as a scroll buffer and
        // hides them.  Only select visible elements here to get the real ones.
        let loadingPlaceholders = getLoadingPlaceholders();

        // Counts as loading if store.loading.local.data does not contain an
        // entry for the image. Therefore should be 2 loading tiles.
        assertEquals(
            3, loadingPlaceholders.length, 'first time 3 placeholders');

        // All images are loading data.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {
            '/sea_pen/111.jpg': true,
            '/sea_pen/222.jpg': true,
            '/sea_pen/333.jpg': true,
          },
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        loadingPlaceholders = getLoadingPlaceholders();
        assertEquals(3, loadingPlaceholders.length, 'still 3 placeholders');

        // Only 3rd image is still loading data.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {
            '/sea_pen/111.jpg': false,
            '/sea_pen/222.jpg': false,
            '/sea_pen/333.jpg': true,
          },
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        loadingPlaceholders = getLoadingPlaceholders();
        assertEquals(
            1, loadingPlaceholders.length, 'Only one loading placeholder');
      });

  test(
      'displays images for recent sea pen images that have successfully loaded',
      async () => {
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImages;
        personalizationStore.data.wallpaper.seaPen.recentImageData =
            seaPenProvider.recentImageData;

        // No image data loaded.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {},
        };

        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        let recentImages = getDisplayedRecentImages();
        // No recent images are displayed.
        assertEquals(0, recentImages.length);

        // 1st image has finished loading data.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {
            '/sea_pen/111.jpg': false,
            '/sea_pen/222.jpg': true,
            '/sea_pen/333.jpg': true,
          },
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        recentImages = getDisplayedRecentImages();
        assertEquals(1, recentImages.length);
        assertDeepEquals(
            {url: 'data:image/jpeg;base64,image111data'},
            recentImages![0]!.src);

        // Set loading failed for second thumbnail.
        personalizationStore.data.wallpaper.loading.seaPen = {
          recentImages: false,
          recentImageData: {
            '/sea_pen/111.jpg': false,
            '/sea_pen/222.jpg': false,
            '/sea_pen/333.jpg': true,
          },
        };
        personalizationStore.data.wallpaper.seaPen.recentImageData = {
          '/sea_pen/111.jpg': {
            url: {url: 'data:image/jpeg;base64,image111data'},
            queryInfo: 'query 1',
          },
          '/sea_pen/222.jpg': {url: {url: ''}, queryInfo: 'query 2'},
          '/sea_pen/333.jpg': {url: {url: ''}, queryInfo: 'query 3'},
        };
        personalizationStore.notifyObservers();
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Still only first thumbnail displayed.
        recentImages = getDisplayedRecentImages();
        assertEquals(1, recentImages.length);
        assertDeepEquals(
            {url: 'data:image/jpeg;base64,image111data'},
            recentImages![0]!.src);
      });

  test('sets selected if image name matches currently selected', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImages;
    personalizationStore.data.wallpaper.seaPen.recentImageData =
        seaPenProvider.recentImageData;
    personalizationStore.data.wallpaper.loading.seaPen = {
      recentImages: false,
      recentImageData: {
        '/sea_pen/111.jpg': false,
        '/sea_pen/222.jpg': false,
        '/sea_pen/333.jpg': false,
      },
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentImages = getDisplayedRecentImages();
    assertEquals(3, recentImages.length);

    // Every image is not selected.
    assertTrue(recentImages.every(image => !image.selected));

    personalizationStore.data.wallpaper.currentSelected = {
      ...wallpaperProvider.currentWallpaper,
      key: '/sea_pen/333.jpg',
    };
    personalizationStore.notifyObservers();

    assertEquals(3, recentImages.length);
    assertFalse(recentImages[0]!.selected!);
    assertFalse(recentImages[1]!.selected!);
    assertTrue(recentImages[2]!.selected!);
  });

  test('opens menu options for a Sea Pen wallpaper', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImages;

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // The menu button for each Sea Pen wallpaper should display.
    const menuIconButtons =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .menu-icon-container .menu-icon-button');
    assertEquals(
        3, menuIconButtons!.length, 'should be 3 menu icon buttons available.');

    // Click on the menu icon button on the second image to open its menu
    // options.
    (menuIconButtons[1] as HTMLElement)!.click();

    // There is an action menu tied to the menu icon button of each image.
    // Only the action menu corresponding to the clicked-on menu icon button
    // should open.
    const actionMenus =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .action-menu-container');
    assertEquals(3, actionMenus!.length, 'should be 3 action menus available.');
    actionMenus.forEach(function(actionMenu: Element, i: number) {
      const menuDialog =
          (actionMenu as HTMLElement).shadowRoot!.querySelector('dialog') as
          HTMLDialogElement;
      if (i === 1) {
        assertTrue(!!menuDialog!.open, `menu dialog ${i} should be opened.`);
      } else {
        assertFalse(!!menuDialog!.open, `menu dialog ${i} should be closed.`);
      }
    });
  });

  test(
      'selects Wallpaper Info menu option for a Sea Pen wallpaper',
      async () => {
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImages;

        // Initialize |seaPenRecentWallpapersElement|.
        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // The menu button for each Sea Pen wallpaper should display.
        const menuIconButtons =
            seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
                'div:not([hidden]) .menu-icon-container .menu-icon-button');
        assertEquals(
            3, menuIconButtons!.length,
            'should be 3 menu icon buttons available.');

        // Click on the menu icon button on the third image to open its menu
        // options.
        (menuIconButtons[2] as HTMLElement)!.click();

        // There is an action menu tied to the menu icon button of each image.
        // Only the action menu corresponding to the clicked-on menu icon button
        // should open.
        const actionMenus =
            seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
                'div:not([hidden]) .action-menu-container');
        assertEquals(
            3, actionMenus!.length, 'should be 3 action menus available.');

        // Menu dialog 2 should be opened.
        const actionMenu2 = actionMenus[2] as HTMLElement;
        const menuDialog = actionMenu2.shadowRoot!.querySelector('dialog') as
            HTMLDialogElement;
        assertTrue(!!menuDialog!.open, `menu dialog 2 should be opened.`);

        // Wallpaper Info menu option is available. Click on this option.
        const wallpaperInfoOption =
            actionMenu2.querySelector('.wallpaper-info-option') as HTMLElement;
        assertTrue(
            !!wallpaperInfoOption, 'Wallpaper Info option should display.');
        wallpaperInfoOption!.click();

        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Wallpaper Info dialog is shown.
        const wallpaperInfoDialog =
            seaPenRecentWallpapersElement.shadowRoot!.getElementById(
                'wallpaperInfoDialog') as HTMLElement;
        assertTrue(
            !!wallpaperInfoDialog, 'Wallpaper Info dialog should display.');
        assertEquals('2', wallpaperInfoDialog.dataset['id']);

        // Click on 'Close' button to close the dialog.
        const closeButton = wallpaperInfoDialog.querySelector(
                                '#wallpaperInfoCloseButton') as HTMLElement;
        assertTrue(
            !!closeButton,
            'close button for Wallpaper Info dialog should display.');
        closeButton!.click();

        await waitAfterNextRender(seaPenRecentWallpapersElement);

        assertNull(
            seaPenRecentWallpapersElement.shadowRoot!.getElementById(
                'wallpaperInfoDialog'),
            'no Wallpaper Info dialog after close button clicked');
      });

  test('clicks on a recent wallpaper to set wallpaper', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImages;

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentImages =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .recent-image');
    assertEquals(3, recentImages!.length, 'should be 3 images available.');

    // Click on the second image to set it as wallpaper.
    (recentImages[1] as HTMLElement)!.click();

    const filePath = await seaPenProvider.whenCalled('selectRecentSeaPenImage');
    assertEquals(
        seaPenProvider.recentImages[1], filePath,
        'file_path sent for the second Sea Pen image');
  });
});
