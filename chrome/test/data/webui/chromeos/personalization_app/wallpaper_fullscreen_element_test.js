// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-fullscreen component.  */

import {WallpaperFullscreen} from 'chrome://personalization/trusted/wallpaper_fullscreen_element.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';
import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestWallpaperProvider} from './test_mojo_interface_provider.js';
import {TestPersonalizationStore} from './test_personalization_store.js';

export function WallpaperFullscreenTest() {
  /** @type {?HTMLElement} */
  let wallpaperFullscreenElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /** @type {!chromeos.personalizationApp.mojom.CurrentWallpaper} */
  const customImage = {
    attribution: ['Custom image'],
    layout: chromeos.personalizationApp.mojom.WallpaperLayout.kCenter,
    key: 'testing',
    type: chromeos.personalizationApp.mojom.WallpaperType.kCustomized,
    url: {url: 'data://testing'},
  };


  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    personalizationStore = mocks.personalizationStore;
  });

  teardown(async () => {
    if (wallpaperFullscreenElement) {
      wallpaperFullscreenElement.remove();
    }
    wallpaperFullscreenElement = null;
    await flushTasks();
  });

  function mockFullscreenApis() {
    const container =
        wallpaperFullscreenElement.shadowRoot.getElementById('container');


    let fullscreenElement = null;

    const requestFullscreenPromise = new Promise((resolve) => {
      container.requestFullscreen = () => {
        fullscreenElement = container;
        container.dispatchEvent(new Event('fullscreenchange'));
        resolve();
        return requestFullscreenPromise;
      };
    });

    wallpaperFullscreenElement.getFullscreenElement = () => fullscreenElement;
    const exitFullscreenPromise = new Promise((resolve) => {
      wallpaperFullscreenElement.exitFullscreen = () => {
        assertTrue(!!fullscreenElement);
        fullscreenElement = null;
        container.dispatchEvent(new Event('fullscreenchange'));
        resolve();
        return exitFullscreenPromise;
      };
    });

    return {requestFullscreenPromise, exitFullscreenPromise};
  }

  test('toggles element visibility on full screen change', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    const container =
        wallpaperFullscreenElement.shadowRoot.getElementById('container');
    assertTrue(container.hidden);

    personalizationStore.data.fullscreen = true;
    personalizationStore.data.currentSelected = customImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertFalse(container.hidden);

    personalizationStore.data.fullscreen = false;
    personalizationStore.notifyObservers();

    await exitFullscreenPromise;

    assertTrue(container.hidden);
  });

  test('exits full screen on exit button click', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.fullscreen = true;
    personalizationStore.data.currentSelected = customImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    const exitButton =
        wallpaperFullscreenElement.shadowRoot.getElementById('exit');
    exitButton.click();

    await exitFullscreenPromise;
  });

  test('shows layout options for custom images', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals(
        null,
        wallpaperFullscreenElement.shadowRoot.getElementById('layoutButtons'));

    personalizationStore.data.currentSelected = customImage;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperFullscreenElement);

    assertTrue(!!wallpaperFullscreenElement.shadowRoot.getElementById(
        'layoutButtons'));
  });

  test('clicking layout option updates layout', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.currentSelected = customImage;
    personalizationStore.data.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    wallpaperFullscreenElement.shadowRoot
        .querySelector('cr-button[data-layout="FILL"]')
        .click();

    const fillLayout =
        await wallpaperProvider.whenCalled('setCustomWallpaperLayout');
    wallpaperProvider.reset();

    assertEquals(
        chromeos.personalizationApp.mojom.WallpaperLayout.kCenterCropped,
        fillLayout);

    // Change the layout type to something other than |kCenter|.
    personalizationStore.data.currentSelected = {
      ...customImage,
      layout: chromeos.personalizationApp.mojom.WallpaperLayout.kCenterCropped,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperFullscreenElement);

    wallpaperFullscreenElement.shadowRoot
        .querySelector('cr-button[data-layout="CENTER"]')
        .click();

    const centerLayout =
        await wallpaperProvider.whenCalled('setCustomWallpaperLayout');

    assertEquals(
        chromeos.personalizationApp.mojom.WallpaperLayout.kCenter,
        centerLayout);
  });

  test('aria selected set for chosen layout option', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.currentSelected = customImage;
    personalizationStore.data.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    // Current image is kCenter.
    assertEquals(
        chromeos.personalizationApp.mojom.WallpaperLayout.kCenter,
        personalizationStore.data.currentSelected.layout);

    const center = wallpaperFullscreenElement.shadowRoot.querySelector(
        'cr-button[data-layout="CENTER"]');
    const fill = wallpaperFullscreenElement.shadowRoot.querySelector(
        'cr-button[data-layout="FILL"]');

    assertEquals('true', center.getAttribute('aria-selected'));
    assertEquals('false', fill.getAttribute('aria-selected'));

    personalizationStore.data.currentSelected = {
      ...personalizationStore.data.currentSelected,
      layout: chromeos.personalizationApp.mojom.WallpaperLayout.kCenterCropped,
    };
    personalizationStore.notifyObservers();
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals('false', center.getAttribute('aria-selected'));
    assertEquals('true', fill.getAttribute('aria-selected'));
  });

  test('clicking set as wallpaper confirms wallpaper', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.fullscreen = true;
    personalizationStore.data.currentSelected = {
      ...personalizationStore.data.currentSelected,
      type: chromeos.personalizationApp.mojom.WallpaperType.kDaily,
    };
    personalizationStore.data.dailyRefresh.collectionId =
        wallpaperProvider.collections[0].id;
    personalizationStore.data.pendingSelected = wallpaperProvider.images[1];
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperFullscreenElement);

    const setAsWallpaperButton =
        wallpaperFullscreenElement.shadowRoot.getElementById('confirm');
    setAsWallpaperButton.click();

    await wallpaperProvider.whenCalled('confirmPreviewWallpaper');
  });
}
