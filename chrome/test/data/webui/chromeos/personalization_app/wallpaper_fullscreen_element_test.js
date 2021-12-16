// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-fullscreen component.  */

import {WallpaperLayout, WallpaperType} from 'chrome://personalization/trusted/personalization_app.mojom-webui.js';
import {WallpaperFullscreen} from 'chrome://personalization/trusted/wallpaper/wallpaper_fullscreen_element.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, waitAfterNextRender} from '../../test_util.js';

import {baseSetup, initElement} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

export function WallpaperFullscreenTest() {
  /** @type {?HTMLElement} */
  let wallpaperFullscreenElement = null;

  /** @type {?TestWallpaperProvider} */
  let wallpaperProvider = null;

  /** @type {?TestPersonalizationStore} */
  let personalizationStore = null;

  /** @type {!CurrentWallpaper} */
  const currentSelectedCustomImage = {
    attribution: ['Custom image'],
    layout: WallpaperLayout.kCenter,
    key: 'testing',
    type: WallpaperType.kCustomized,
    url: {url: 'data://testing'},
  };

  /** @type {!DisplayableImage} */
  const pendingSelectedCustomImage = {path: '/local/image/path.jpg'};

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    wallpaperProvider.isInTabletModeResponse = true;
    personalizationStore = mocks.personalizationStore;
    loadTimeData.overrideValues({fullScreenPreviewEnabled: true});
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

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertFalse(container.hidden);

    personalizationStore.data.wallpaper.fullscreen = false;
    personalizationStore.notifyObservers();

    await exitFullscreenPromise;

    assertTrue(container.hidden);
  });

  test('sets default layout option on when entering preview', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals(null, wallpaperFullscreenElement.selectedLayout_);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertEquals(
        WallpaperLayout.kCenterCropped,
        wallpaperFullscreenElement.selectedLayout_);

    personalizationStore.data.wallpaper.fullscreen = false;
    personalizationStore.notifyObservers();

    await exitFullscreenPromise;

    assertEquals(null, wallpaperFullscreenElement.selectedLayout_);
  });

  test('sets fullscreen class on body when entering fullscreen', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals('', document.body.className);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertEquals('fullscreen-preview', document.body.className);

    wallpaperFullscreenElement.exitFullscreen();

    await exitFullscreenPromise;

    assertEquals('', document.body.className);
  });

  test('exits full screen on exit button click', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
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

    personalizationStore.data.wallpaper.pendingSelected =
        pendingSelectedCustomImage;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperFullscreenElement);

    assertTrue(!!wallpaperFullscreenElement.shadowRoot.getElementById(
        'layoutButtons'));
  });

  test('clicking layout option selects image with new layout', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.data.wallpaper.pendingSelected =
        pendingSelectedCustomImage;
    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    wallpaperFullscreenElement.shadowRoot
        .querySelector('cr-button[data-layout="FILL"]')
        .click();

    const [fillImage, fillLayout, fillPreviewMode] =
        await wallpaperProvider.whenCalled('selectLocalImage');
    wallpaperProvider.reset();

    assertEquals(pendingSelectedCustomImage, fillImage);
    assertEquals(WallpaperLayout.kCenterCropped, fillLayout);
    assertTrue(fillPreviewMode);

    wallpaperFullscreenElement.shadowRoot
        .querySelector('cr-button[data-layout="CENTER"]')
        .click();

    const [centerImage, centerLayout, centerPreviewMode] =
        await wallpaperProvider.whenCalled('selectLocalImage');

    assertEquals(pendingSelectedCustomImage, centerImage);
    assertEquals(WallpaperLayout.kCenter, centerLayout);
    assertTrue(centerPreviewMode);
  });

  test('aria pressed set for chosen layout option', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.data.wallpaper.pendingSelected =
        pendingSelectedCustomImage;
    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    // Current image is kCenter but initial state should reset to default.
    assertEquals(
        WallpaperLayout.kCenter,
        personalizationStore.data.wallpaper.currentSelected.layout);

    const center = wallpaperFullscreenElement.shadowRoot.querySelector(
        'cr-button[data-layout="CENTER"]');
    const fill = wallpaperFullscreenElement.shadowRoot.querySelector(
        'cr-button[data-layout="FILL"]');

    assertEquals('false', center.getAttribute('aria-pressed'));
    assertEquals('true', fill.getAttribute('aria-pressed'));

    wallpaperFullscreenElement.selectedLayout_ = WallpaperLayout.kCenter;
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals('true', center.getAttribute('aria-pressed'));
    assertEquals('false', fill.getAttribute('aria-pressed'));
  });

  test('clicking set as wallpaper confirms wallpaper', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen.is);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected = {
      ...personalizationStore.data.wallpaper.currentSelected,
      type: WallpaperType.kDaily,
    };
    personalizationStore.data.wallpaper.dailyRefresh.collectionId =
        wallpaperProvider.collections[0].id;
    personalizationStore.data.wallpaper.pendingSelected =
        wallpaperProvider.images[1];
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperFullscreenElement);

    const setAsWallpaperButton =
        wallpaperFullscreenElement.shadowRoot.getElementById('confirm');
    setAsWallpaperButton.click();

    await wallpaperProvider.whenCalled('confirmPreviewWallpaper');
  });
}
