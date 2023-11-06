// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {SeaPenRecentWallpapersElement} from 'chrome://personalization/js/personalization_app.js';
import {assertEquals, assertFalse, assertNull, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, teardownElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('SeaPenRecentWallpapersElementTest', function() {
  let personalizationStore: TestPersonalizationStore;
  let wallpaperProvider: TestWallpaperProvider;
  let seaPenRecentWallpapersElement: SeaPenRecentWallpapersElement|null;

  setup(() => {
    const mocks = baseSetup();
    personalizationStore = mocks.personalizationStore;
    wallpaperProvider = mocks.wallpaperProvider;
  });

  teardown(async () => {
    await teardownElement(seaPenRecentWallpapersElement);
    seaPenRecentWallpapersElement = null;
  });

  test('displays recently used Sea Pen wallpapers', async () => {
    personalizationStore.data.wallpaper.seaPen.recentWallpapers =
        wallpaperProvider.seaPenWallpapers;

    // Initialize |seaPenRecentWallpapersElement|.
    seaPenRecentWallpapersElement = initElement(SeaPenRecentWallpapersElement);
    await waitAfterNextRender(seaPenRecentWallpapersElement);

    // Sea Pen wallpaper thumbnails should display.
    const recentWallpapers =
        seaPenRecentWallpapersElement.shadowRoot!.querySelectorAll(
            'div:not([hidden]) .recent-wallpaper');
    assertEquals(3, recentWallpapers!.length, 'should be 3 images available.');

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

  test('opens menu options for a Sea Pen wallpaper', async () => {
    personalizationStore.data.wallpaper.seaPen.recentWallpapers =
        wallpaperProvider.seaPenWallpapers;

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
        personalizationStore.data.wallpaper.seaPen.recentWallpapers =
            wallpaperProvider.seaPenWallpapers;

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
});
