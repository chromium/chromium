// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Router, routes, WallpaperBrowserProxyImpl} from 'chrome://os-settings/chromeos/os_settings.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.js';

let personalizationPage = null;

/** @type {?TestWallpaperBrowserProxy} */
let WallpaperBrowserProxy = null;

function createPersonalizationPage() {
  WallpaperBrowserProxy.reset();
  PolymerTest.clearBody();

  personalizationPage = document.createElement('settings-personalization-page');
  personalizationPage.set('prefs', {
    extensions: {
      theme: {
        id: {
          value: '',
        },
        use_system: {
          value: false,
        },
      },
    },
  });

  personalizationPage.set('pageVisibility', {
    setWallpaper: true,
  });

  document.body.appendChild(personalizationPage);
  flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    assertFalse(
        loadTimeData.getBoolean('isPersonalizationHubEnabled'),
        'this test should only run with PersonalizationHub disabled');
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    WallpaperBrowserProxy = new TestWallpaperBrowserProxy();
    WallpaperBrowserProxyImpl.setInstanceForTesting(WallpaperBrowserProxy);
    createPersonalizationPage();
  });

  teardown(function() {
    personalizationPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('wallpaperManager', async () => {
    WallpaperBrowserProxy.setIsWallpaperPolicyControlled(false);
    // TODO(dschuyler): This should notice the policy change without needing
    // the page to be recreated.
    createPersonalizationPage();
    await WallpaperBrowserProxy.whenCalled('isWallpaperPolicyControlled');
    const button =
        personalizationPage.shadowRoot.getElementById('wallpaperButton');
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();
    await WallpaperBrowserProxy.whenCalled('openWallpaperManager');
  });

  test('wallpaperSettingVisible', function() {
    personalizationPage.showWallpaperRow_ = false;
    flush();
    assertTrue(personalizationPage.shadowRoot.querySelector('#wallpaperButton')
                   .hidden);
  });

  test('wallpaperPolicyControlled', async () => {
    // Should show the wallpaper policy indicator and disable the toggle
    // button if the wallpaper is policy controlled.
    WallpaperBrowserProxy.setIsWallpaperPolicyControlled(true);
    createPersonalizationPage();
    await WallpaperBrowserProxy.whenCalled('isWallpaperPolicyControlled');
    flush();
    assertFalse(personalizationPage.shadowRoot
                    .querySelector('#wallpaperPolicyIndicator')
                    .hidden);
    assertTrue(personalizationPage.shadowRoot.querySelector('#wallpaperButton')
                   .disabled);
  });

  test('Deep link to open wallpaper button', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '500');
    Router.getInstance().navigateTo(routes.PERSONALIZATION, params);

    const deepLinkElement =
        personalizationPage.shadowRoot.getElementById('wallpaperButton')
            .shadowRoot.querySelector('#icon');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Wallpaper button should be focused for settingId=500.');
  });

  test('changePicture', function() {
    const row =
        personalizationPage.shadowRoot.getElementById('changePictureRow');
    assertTrue(!!row);
    row.click();
    assertEquals(routes.CHANGE_PICTURE, Router.getInstance().getCurrentRoute());
  });

  test('ambientMode', function() {
    const isGuest = loadTimeData.getBoolean('isGuest');
    const isAmbientModeEnabled = loadTimeData.getBoolean('isAmbientModeEnabled');

    if(!isGuest && isAmbientModeEnabled){
      const row =
          personalizationPage.shadowRoot.querySelector('#ambientModeRow');
      assertTrue(!!row);
      row.click();
      assertEquals(routes.AMBIENT_MODE, Router.getInstance().getCurrentRoute());
    }
  });

  test('Deep link to change account picture', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '503');
    Router.getInstance().navigateTo(routes.CHANGE_PICTURE, params);

    flush();

    await waitAfterNextRender(personalizationPage);

    const changePicturePage =
        personalizationPage.shadowRoot.querySelector('settings-change-picture');
    assertTrue(!!changePicturePage);
    const deepLinkElement =
        changePicturePage.shadowRoot.querySelector('#pictureList')
            .shadowRoot.querySelector('#selector')
            .$$('[class="iron-selected"]');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Account picture elem should be focused for settingId=503.');
  });
});
