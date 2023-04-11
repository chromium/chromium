// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test suite for wallpaper-fullscreen component.  */

import 'chrome://personalization/strings.m.js';
import 'chrome://webui-test/mojo_webui_test_support.js';

import {CurrentWallpaper, DailyRefreshType, DisplayableImage, GooglePhotosPhoto, OnlineImageType, WallpaperFullscreen, WallpaperImage, WallpaperLayout, WallpaperObserver, WallpaperType} from 'chrome://personalization/js/personalization_app.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {flushTasks, waitAfterNextRender} from 'chrome://webui-test/polymer_test_util.js';

import {baseSetup, initElement, toString16} from './personalization_app_test_utils.js';
import {TestPersonalizationStore} from './test_personalization_store.js';
import {TestWallpaperProvider} from './test_wallpaper_interface_provider.js';

suite('WallpaperFullscreenTest', function() {
  let wallpaperFullscreenElement: WallpaperFullscreen|null = null;
  let wallpaperProvider: TestWallpaperProvider;
  let personalizationStore: TestPersonalizationStore;

  const currentSelectedCustomImage: CurrentWallpaper = {
    attribution: ['Custom image'],
    descriptionContent: '',
    descriptionTitle: '',
    key: 'testing',
    layout: WallpaperLayout.kCenter,
    type: WallpaperType.kCustomized,
  };

  const pendingSelectedCustomImage:
      DisplayableImage = {path: '/local/image/path.jpg'};

  setup(() => {
    const mocks = baseSetup();
    wallpaperProvider = mocks.wallpaperProvider;
    wallpaperProvider.isInTabletModeResponse = true;
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
        wallpaperFullscreenElement!.shadowRoot!.getElementById('container');

    let fullscreenElement: HTMLElement|null = null;

    const requestFullscreenPromise = new Promise<void>((resolve) => {
      container!.requestFullscreen = () => {
        fullscreenElement = container;
        container!.dispatchEvent(new Event('fullscreenchange'));
        resolve();
        return requestFullscreenPromise;
      };
    });

    wallpaperFullscreenElement!.getFullscreenElement = () => fullscreenElement;
    const exitFullscreenPromise = new Promise<void>((resolve) => {
      wallpaperFullscreenElement!.exitFullscreen = () => {
        assertTrue(!!fullscreenElement);
        fullscreenElement = null;
        container!.dispatchEvent(new Event('fullscreenchange'));
        resolve();
        return exitFullscreenPromise;
      };
    });

    return {requestFullscreenPromise, exitFullscreenPromise};
  }

  test('toggles element visibility on full screen change', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    const container =
        wallpaperFullscreenElement.shadowRoot!.getElementById('container');
    assertTrue(container!.hidden);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertFalse(container!.hidden);

    personalizationStore.data.wallpaper.fullscreen = false;
    personalizationStore.notifyObservers();

    await exitFullscreenPromise;

    assertTrue(container!.hidden);
  });

  test('sets default layout option on when entering preview', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals(null, wallpaperFullscreenElement['selectedLayout_']);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    assertEquals(
        WallpaperLayout.kCenterCropped,
        wallpaperFullscreenElement['selectedLayout_']);

    personalizationStore.data.wallpaper.fullscreen = false;
    personalizationStore.notifyObservers();

    await exitFullscreenPromise;

    assertEquals(null, wallpaperFullscreenElement['selectedLayout_']);
  });

  test('sets fullscreen class on body when entering fullscreen', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
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
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    const exitButton =
        wallpaperFullscreenElement.shadowRoot!.getElementById('exit');
    exitButton!.click();

    await exitFullscreenPromise;
  });

  [{
    pendingSelectedImage: pendingSelectedCustomImage,
    shouldShow: true,
  },
   {
     pendingSelectedImage: {
       url: {url: ''},
       attribution: [],
       assetId: 0n,
       unitId: 0n,
       type: OnlineImageType.kUnknown,
     } as WallpaperImage,
     shouldShow: false,
   },
   {
     pendingSelectedImage: {
       id: 'test_id',
       name: 'asdf',
       date: toString16('February'),
       url: {url: ''},
     } as GooglePhotosPhoto,
     shouldShow: true,
   },
  ]
      .forEach(
          testCase => test(
              'shows layout options for custom and Google Photos images',
              async () => {
                wallpaperFullscreenElement = initElement(WallpaperFullscreen);
                await waitAfterNextRender(wallpaperFullscreenElement);

                assertEquals(
                    null,
                    wallpaperFullscreenElement.shadowRoot!.getElementById(
                        'layoutButtons'));

                // Select a wallpaper in preview mode from a starting state
                // where the layout buttons have not been created.
                personalizationStore.data.wallpaper.pendingSelected =
                    testCase.pendingSelectedImage;
                personalizationStore.notifyObservers();
                await waitAfterNextRender(wallpaperFullscreenElement);

                // Verify whether layout buttons are created.
                let layoutButtonsEl =
                    wallpaperFullscreenElement.shadowRoot!.getElementById(
                        'layoutButtons');
                assertEquals(!!layoutButtonsEl, testCase.shouldShow);

                // Select a custom wallpaper to make sure that layout buttons
                // are shown.
                personalizationStore.data.wallpaper.pendingSelected =
                    pendingSelectedCustomImage;
                personalizationStore.notifyObservers();
                await waitAfterNextRender(wallpaperFullscreenElement);

                layoutButtonsEl =
                    wallpaperFullscreenElement.shadowRoot!.getElementById(
                        'layoutButtons');
                assertTrue(!!layoutButtonsEl);

                // Select a wallpaper in preview mode from a starting state
                // where the layout buttons have been created.
                personalizationStore.data.wallpaper.pendingSelected =
                    testCase.pendingSelectedImage;
                personalizationStore.notifyObservers();
                await waitAfterNextRender(wallpaperFullscreenElement);

                // The layout buttons will still exist from having been added to
                // the shadow DOM already, so now we test whether they are
                // actually showing.
                layoutButtonsEl =
                    wallpaperFullscreenElement.shadowRoot!.getElementById(
                        'layoutButtons');
                assertTrue(!!layoutButtonsEl);
                assertEquals(
                    getComputedStyle(layoutButtonsEl).display,
                    testCase.shouldShow ? 'grid' : 'none');
              }));

  test('clicking layout option selects image with new layout', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    const {requestFullscreenPromise} = mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.currentSelected =
        currentSelectedCustomImage;
    personalizationStore.data.wallpaper.pendingSelected =
        pendingSelectedCustomImage;
    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    let button = wallpaperFullscreenElement.shadowRoot!.querySelector(
                     'cr-button[data-layout="FILL"]')! as HTMLButtonElement;
    button.click();

    const [fillImage, fillLayout, fillPreviewMode] =
        await wallpaperProvider.whenCalled('selectLocalImage');
    wallpaperProvider.reset();

    assertEquals(pendingSelectedCustomImage, fillImage);
    assertEquals(WallpaperLayout.kCenterCropped, fillLayout);
    assertTrue(fillPreviewMode);

    button = wallpaperFullscreenElement.shadowRoot!.querySelector(
                 'cr-button[data-layout="CENTER"]') as HTMLButtonElement;
    button.click();

    const [centerImage, centerLayout, centerPreviewMode] =
        await wallpaperProvider.whenCalled('selectLocalImage');

    assertEquals(pendingSelectedCustomImage, centerImage);
    assertEquals(WallpaperLayout.kCenter, centerLayout);
    assertTrue(centerPreviewMode);
  });

  test('aria pressed set for chosen layout option', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
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

    const center = wallpaperFullscreenElement.shadowRoot!.querySelector(
        'cr-button[data-layout="CENTER"]');
    const fill = wallpaperFullscreenElement.shadowRoot!.querySelector(
        'cr-button[data-layout="FILL"]');

    assertEquals('false', center!.getAttribute('aria-pressed'));
    assertEquals('true', fill!.getAttribute('aria-pressed'));

    wallpaperFullscreenElement['selectedLayout_'] = WallpaperLayout.kCenter;
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals('true', center!.getAttribute('aria-pressed'));
    assertEquals('false', fill!.getAttribute('aria-pressed'));
  });

  test('clicking set as wallpaper confirms wallpaper', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.data.wallpaper.currentSelected = {
      ...wallpaperProvider.currentWallpaper,
      type: WallpaperType.kDaily,
    };
    personalizationStore.data.wallpaper.dailyRefresh = {
      id: wallpaperProvider.collections![0]!.id,
      type: DailyRefreshType.BACKDROP,
    };
    personalizationStore.data.wallpaper.pendingSelected =
        wallpaperProvider.images![1]!;
    personalizationStore.notifyObservers();

    await waitAfterNextRender(wallpaperFullscreenElement);

    const setAsWallpaperButton =
        wallpaperFullscreenElement.shadowRoot!.getElementById('confirm') as
        HTMLButtonElement;
    setAsWallpaperButton.click();

    await wallpaperProvider.whenCalled('confirmPreviewWallpaper');
  });

  test('sets aria label on cr-button', async () => {
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    assertEquals(
        loadTimeData.getString('ariaLabelExitFullscreen'),
        wallpaperFullscreenElement.shadowRoot
            ?.querySelector('.fullscreen-button')
            ?.getAttribute('aria-label'),
        'exit button aria label is set');
  });

  test('exits fullscreen on popstate', async () => {
    WallpaperObserver.initWallpaperObserverIfNeeded();
    wallpaperFullscreenElement = initElement(WallpaperFullscreen);
    const {requestFullscreenPromise, exitFullscreenPromise} =
        mockFullscreenApis();
    await waitAfterNextRender(wallpaperFullscreenElement);

    // Add a history entry to pop later.
    window.history.pushState(null, '', window.location.href + '#test');

    personalizationStore.data.wallpaper.fullscreen = true;
    personalizationStore.notifyObservers();

    await requestFullscreenPromise;

    personalizationStore.setReducersEnabled(true);
    window.history.back();

    // Triggered by popstate.
    await wallpaperProvider.whenCalled('cancelPreviewWallpaper');

    // Simulate the response from wallpaper controller.
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperPreviewEnded();
    // Second |onWallpaperChanged| is generated by
    // |personalization_app_wallpaper_provider_impl.cc|.
    wallpaperProvider.wallpaperObserverRemote!.onWallpaperChanged(
        wallpaperProvider.currentWallpaper);

    // |exitFullscreenPromise| from wallpaper preview being canceled.
    await exitFullscreenPromise;
    WallpaperObserver.shutdown();
  });
});
