// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/chromeos/mojo_webui_test_support.js';

import {emptyState, SeaPenActionName, SeaPenRecentWallpapersElement, WallpaperGridItemElement} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertDeepEquals, assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestSeaPenProvider} from './test_sea_pen_interface_provider.js';

suite('SeaPenRecentWallpapersElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let seaPenProvider: TestSeaPenProvider;
  let seaPenRecentWallpapersElement: SeaPenRecentWallpapersElement|null;

  function getLoadingPlaceholders(): WallpaperGridItemElement[] {
    if (!seaPenRecentWallpapersElement) {
      return [];
    }

    return Array.from(seaPenRecentWallpapersElement.shadowRoot!
                          .querySelectorAll<WallpaperGridItemElement>(
                              'div:not([hidden]) .sea-pen-image[placeholder]'));
  }

  function getDisplayedRecentImages(): WallpaperGridItemElement[] {
    if (!seaPenRecentWallpapersElement) {
      return [];
    }

    return Array.from(
        seaPenRecentWallpapersElement.shadowRoot!
            .querySelectorAll<WallpaperGridItemElement>(
                'div:not([hidden]) .sea-pen-image:not([placeholder])'));
  }

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    seaPenProvider = mocks.seaPenProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenRecentWallpapersElement);
    seaPenRecentWallpapersElement = null;
    loadTimeData.overrideValues({
      isSeaPenTextInputEnabled: false,
    });
  });

  test('displays recently used Sea Pen wallpapers', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImageIds;
    personalizationStore.data.wallpaper.seaPen.recentImageData =
        seaPenProvider.recentImageData;
    personalizationStore.data.wallpaper.seaPen.loading = {
      recentImageData: {
        111: false,
        222: false,
        333: false,
      },
      recentImages: false,
      thumbnails: false,
      currentSelected: false,
      setImage: 0,
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
      'saves recently used Sea Pen images', async () => {
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
            seaPenProvider.recentImageIds,
            personalizationStore.data.wallpaper.seaPen.recentImages,
            'expected recent images are set');
        for (const [key, value] of Object.entries(
                 personalizationStore.data.wallpaper.seaPen.recentImageData)) {
          assertTrue(
              seaPenProvider.recentImageData.hasOwnProperty(key),
              `expected image data for file path ${key} is set`);
          assertDeepEquals(
              seaPenProvider.recentImageData[parseInt(key, 10)]!.url,
              value!.url, `expected url for file path ${key} is set`);
        }
      });

  test('unloaded local images display placeholder', async () => {
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      // No image data loaded.
      loading: {
        recentImageData: {},
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
      recentImages: seaPenProvider.recentImageIds,
      recentImageData: seaPenProvider.recentImageData,
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Iron-list creates some extra dom elements as a scroll buffer and
    // hides them. Only select visible elements here to get the real ones.
    let loadingPlaceholders = getLoadingPlaceholders();

    // Counts as loading if store.loading.local.data does not contain an
    // entry for the image. Therefore should be 2 loading tiles.
    assertEquals(3, loadingPlaceholders.length, 'first time 3 placeholders');

    // All images are loading data.
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      loading: {
        recentImageData: {
          111: true,
          222: true,
          333: true,
        },
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    loadingPlaceholders = getLoadingPlaceholders();
    assertEquals(3, loadingPlaceholders.length, 'still 3 placeholders');

    // Only 3rd image is still loading data.
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      loading: {
        recentImageData: {
          111: false,
          222: false,
          333: true,
        },
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    loadingPlaceholders = getLoadingPlaceholders();
    assertEquals(1, loadingPlaceholders.length, 'Only one loading placeholder');
  });

  test('displays successfully loaded recent sea pen images', async () => {
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      // No image data loaded.
      loading: {
        recentImageData: {},
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
      recentImageData: seaPenProvider.recentImageData,
      recentImages: seaPenProvider.recentImageIds,
    };

    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    let recentImages = getDisplayedRecentImages();
    // No recent images are displayed.
    assertEquals(0, recentImages.length, 'no images loaded yet');

    // 1st image has finished loading data.
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      loading: {
        recentImageData: {
          111: false,
          222: true,
          333: true,
        },
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    recentImages = getDisplayedRecentImages();
    assertEquals(1, recentImages.length, '1 image done loading');
    assertDeepEquals(
        {url: 'data:image/jpeg;base64,image111data'}, recentImages![0]!.src);

    // Set loading failed for second thumbnail.
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      recentImageData: {
        111: {
          url: {url: 'data:image/jpeg;base64,image111data'},
          imageInfo: null,
        },
        222: {
          url: {url: ''},
          imageInfo: seaPenProvider.recentImageInfo2,
        },
      },
      loading: {
        recentImageData: {
          111: false,
          222: false,
          333: true,
        },
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Still only first thumbnail displayed.
    recentImages = getDisplayedRecentImages();
    assertEquals(1, recentImages.length, '1 image loaded successfully');
    assertDeepEquals(
        {url: 'data:image/jpeg;base64,image111data'}, recentImages![0]!.src,
        'src equals successfully loaded image url');
  });

  test('opens menu options for a Sea Pen wallpaper', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImageIds;

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
      'disables SeaPenTextInput to hide Create more option for recent image',
      async () => {
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImageIds;
        personalizationStore.data.wallpaper.seaPen.recentImageData =
            seaPenProvider.recentImageData;
        personalizationStore.data.wallpaper.seaPen.loading = {
          recentImageData: {
            111: false,
            222: false,
            333: false,
          },
          recentImages: false,
          thumbnails: false,
          currentSelected: false,
          setImage: 0,
        };

        // Initialize |seaPenRecentWallpapersElement|.
        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Get menu icon button for the second image.
        const menuIconButton =
            seaPenRecentWallpapersElement.shadowRoot?.querySelectorAll<
                HTMLElement>(
                'div:not([hidden]) .menu-icon-container .menu-icon-button')[1];
        assertTrue(
            !!menuIconButton,
            'menu icon button for the second image should display.');

        // Click on the menu icon button of the second image to open its menu
        // options.
        menuIconButton!.click();

        // Get the action menu for the second image.
        const actionMenu =
            seaPenRecentWallpapersElement.shadowRoot
                ?.querySelectorAll<HTMLElement>(
                    'div:not([hidden]) .action-menu-container')[1];
        assertTrue(!!actionMenu, 'action menu for 2nd image should display');

        const menuOptions =
            actionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
        assertEquals(2, menuOptions.length, '2 options available to select');

        const createMoreOption = actionMenu.querySelector<HTMLElement>(
            '.dropdown-item.create-more-option');
        assertFalse(
            !!createMoreOption, 'Create more option should not display.');
      });

  test(
      'enables SeaPenTextInput to show Create more option for recent image',
      async () => {
        loadTimeData.overrideValues({
          isSeaPenTextInputEnabled: true,
        });
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImageIds;
        personalizationStore.data.wallpaper.seaPen.recentImageData =
            seaPenProvider.recentImageData;
        personalizationStore.data.wallpaper.seaPen.loading = {
          recentImageData: {
            111: false,
            222: false,
            333: false,
          },
          recentImages: false,
          thumbnails: false,
          currentSelected: false,
          setImage: 0,
        };

        // Initialize |seaPenRecentWallpapersElement|.
        seaPenRecentWallpapersElement =
            initElement(SeaPenRecentWallpapersElement);
        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Get menu icon button for the second image.
        const menuIconButton =
            seaPenRecentWallpapersElement.shadowRoot?.querySelectorAll<
                HTMLElement>(
                'div:not([hidden]) .menu-icon-container .menu-icon-button')[1];
        assertTrue(
            !!menuIconButton,
            'menu icon button for the second image should display.');

        // Click on the menu icon button of the second image to open its menu
        // options.
        menuIconButton!.click();

        // Get the action menu for the second image.
        const actionMenu =
            seaPenRecentWallpapersElement.shadowRoot
                ?.querySelectorAll<HTMLElement>(
                    'div:not([hidden]) .action-menu-container')[1];
        assertTrue(!!actionMenu, 'action menu for 2nd image should display');

        const menuOptions =
            actionMenu.querySelectorAll<HTMLElement>('.dropdown-item');
        assertEquals(3, menuOptions.length, '3 options available to select');

        const createMoreOption = actionMenu.querySelector<HTMLElement>(
            '.dropdown-item.create-more-option');
        assertTrue(!!createMoreOption, 'Create more option should display.');
      });

  test(
      'select Wallpaper Info option for recent image', async () => {
        personalizationStore.data.wallpaper.seaPen.recentImages =
            seaPenProvider.recentImageIds;
        personalizationStore.data.wallpaper.seaPen.recentImageData =
            seaPenProvider.recentImageData;
        personalizationStore.data.wallpaper.seaPen.loading = {
          recentImageData: {
            111: false,
            222: false,
            333: false,
          },
          recentImages: false,
          thumbnails: false,
          currentSelected: false,
          setImage: 0,
        };

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

        // Menu dialog for the 3rd image should be opened.
        const actionMenu2 = actionMenus[2] as HTMLElement;
        const menuDialog2 = actionMenu2.shadowRoot!.querySelector('dialog') as
            HTMLDialogElement;
        assertTrue(
            !!menuDialog2!.open,
            `menu dialog for the 3rd image should be opened.`);

        // Wallpaper Info menu option is available. Click on this option.
        const wallpaperInfoOption2 =
            actionMenu2.querySelector<HTMLElement>('.wallpaper-info-option');
        assertTrue(
            !!wallpaperInfoOption2,
            'Wallpaper Info option for the 3rd image should display.');
        wallpaperInfoOption2!.click();

        await waitAfterNextRender(seaPenRecentWallpapersElement);

        // Wallpaper Info dialog is shown.
        const wallpaperInfoDialog =
            seaPenRecentWallpapersElement.shadowRoot!.getElementById(
                'wallpaperInfoDialog') as HTMLElement;
        assertTrue(
            !!wallpaperInfoDialog, 'Wallpaper Info dialog should display.');
        assertEquals('2', wallpaperInfoDialog.dataset['id']);

        const aboutQueryDesc = wallpaperInfoDialog.querySelector<HTMLElement>(
            '.about-prompt-info');
        assertTrue(
            !!aboutQueryDesc,
            'Wallpaper Info dialog should include visible user query info');
        assertTrue(
            aboutQueryDesc.innerText.includes('test freeform query'),
            'user visible query for 3rd image should display');

        const creationTimeDesc =
            wallpaperInfoDialog.querySelector<HTMLElement>('.about-date-info');
        assertTrue(
            !!creationTimeDesc,
            'Wallpaper Info dialog should include creation time info');
        assertTrue(
            creationTimeDesc.innerText.includes('Dec 31, 2023'),
            'creation time for 3rd image should display');

        // Click on 'Close' button to close the dialog.
        const closeButton = wallpaperInfoDialog.querySelector<HTMLElement>(
            '#wallpaperInfoCloseButton');
        assertTrue(
            !!closeButton,
            'close button for Wallpaper Info dialog should display.');
        closeButton!.click();

        await waitAfterNextRender(seaPenRecentWallpapersElement);

        assertNull(
            seaPenRecentWallpapersElement.shadowRoot!.getElementById(
                'wallpaperInfoDialog'),
            'no Wallpaper Info dialog after close button clicked');

        // Click on the menu icon button on the first image to open its menu
        // options.
        (menuIconButtons[0] as HTMLElement)!.click();

        // Menu dialog for the 1st image should be opened.
        const actionMenu0 = actionMenus[0] as HTMLElement;
        const menuDialog0 =
            actionMenu0.shadowRoot!.querySelector<HTMLDialogElement>('dialog');
        assertTrue(
            !!menuDialog0!.open,
            `menu dialog for the 1st image should be opened.`);

        // Wallpaper Info menu option is not available as SeaPenRecentData has
        // no imageInfo.
        const wallpaperInfoOption0 =
            actionMenu0.querySelector('.wallpaper-info-option');
        assertFalse(
            !!wallpaperInfoOption0,
            'Wallpaper Info option for the 1st image should not display.');
      });

  test('deletes a recent Sea Pen image', async () => {
    personalizationStore.setReducersEnabled(true);
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImageIds;
    personalizationStore.data.wallpaper.seaPen.recentImageData =
        seaPenProvider.recentImageData;
    personalizationStore.data.wallpaper.seaPen.loading = {
      recentImages: false,
      recentImageData: {
        111: false,
        222: false,
        333: false,
      },
      thumbnails: false,
      currentSelected: false,
      setImage: 0,
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // 3 Sea Pen wallpaper thumbnails should display.
    let recentImages = getDisplayedRecentImages();
    assertEquals(3, recentImages.length);

    // The menu button for each Sea Pen wallpaper should display.
    const menuIconButtons =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .menu-icon-container .menu-icon-button');
    assertEquals(
        3, menuIconButtons!.length, 'should be 3 menu icon buttons available.');

    // Click on the menu icon button on the 1st image to open its menu
    // options.
    (menuIconButtons[0] as HTMLElement)!.click();

    // There is an action menu tied to the menu icon button of each image.
    // Only the action menu corresponding to the clicked-on menu icon button
    // should open.
    const actionMenus =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .action-menu-container');
    assertEquals(3, actionMenus!.length, 'should be 3 action menus available.');

    // Menu dialog for the first image should be opened.
    const actionMenu = actionMenus[0] as HTMLElement;
    const menuDialog =
        actionMenu.shadowRoot!.querySelector('dialog') as HTMLDialogElement;
    assertTrue(!!menuDialog!.open, `menu dialog 0 should be opened.`);

    // Wallpaper Info menu option is available. Click on this option.
    const deleteWallpaperOption =
        actionMenu.querySelector('.delete-wallpaper-option') as HTMLElement;
    assertTrue(
        !!deleteWallpaperOption, 'delete wallpaper option should display.');

    personalizationStore.expectAction(
        SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES);
    deleteWallpaperOption!.click();

    await personalizationStore.waitForAction(
        SeaPenActionName.SET_RECENT_SEA_PEN_IMAGES);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Only two Sea Pen wallpaper thumbnails should display after one is
    // deleted.
    recentImages = getDisplayedRecentImages();
    assertEquals(2, recentImages.length, 'only 2 images should display.');
    assertDeepEquals(
        {url: 'data:image/jpeg;base64,image222data'}, recentImages![0]!.src);
    assertDeepEquals(
        {url: 'data:image/jpeg;base64,image333data'}, recentImages![1]!.src);
  });

  test('clicks on a recent wallpaper to set wallpaper', async () => {
    personalizationStore.data.wallpaper.seaPen.recentImages =
        seaPenProvider.recentImageIds;
    personalizationStore.data.wallpaper.seaPen.recentImageData =
        seaPenProvider.recentImageData;
    personalizationStore.data.wallpaper.seaPen.loading = {
      recentImageData: {
        111: false,
        222: false,
        333: false,
      },
      recentImages: false,
      thumbnails: false,
      currentSelected: false,
      setImage: 0,
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentImages = getDisplayedRecentImages();
    assertEquals(3, recentImages!.length, 'should be 3 images available.');

    // Click on the second image to set it as wallpaper.
    (recentImages[1] as HTMLElement)!.click();

    const filePath = await seaPenProvider.whenCalled('selectRecentSeaPenImage');
    assertEquals(
        seaPenProvider.recentImageIds[1], filePath,
        'file_path sent for the second Sea Pen image');
  });

  test('sets selected if image name matches currently selected', async () => {
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      // No image data loaded.
      loading: {
        recentImageData: {
          111: false,
          222: false,
          333: false,
        },
        recentImages: false,
        thumbnails: false,
        currentSelected: false,
        setImage: 0,
      },
      recentImages: seaPenProvider.recentImageIds,
      recentImageData: seaPenProvider.recentImageData,
    };

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentImages = getDisplayedRecentImages();
    assertEquals(3, recentImages.length);

    // Every image is not selected.
    assertTrue(recentImages.every(image => !image.selected));

    // Update currentSelected state to a sea pen image.
    personalizationStore.data.wallpaper.seaPen = {
      ...personalizationStore.data.wallpaper.seaPen,
      currentSelected: 333,
    };
    personalizationStore.notifyObservers();

    assertEquals(
        3, recentImages.length, 'there should be 3 recent sea pen images');
    assertFalse(recentImages[0]!.selected!, 'element 0 should not be selected');
    assertFalse(recentImages[1]!.selected!, 'element 1 should not be selected');
    assertTrue(recentImages[2]!.selected!, 'element 2 should be selected');
  });
});
