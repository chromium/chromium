// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {WallpaperBrowserProxyImpl, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

let personalizationPage = null;

/** @type {?settings.TestWallpaperBrowserProxy} */
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
  Polymer.dom.flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    WallpaperBrowserProxy = new settings.TestWallpaperBrowserProxy();
    settings.WallpaperBrowserProxyImpl.instance_ = WallpaperBrowserProxy;
    createPersonalizationPage();
  });

  teardown(function() {
    personalizationPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
  });

  test('wallpaperManager', async () => {
    WallpaperBrowserProxy.setIsWallpaperPolicyControlled(false);
    // TODO(dschuyler): This should notice the policy change without needing
    // the page to be recreated.
    createPersonalizationPage();
    await WallpaperBrowserProxy.whenCalled('isWallpaperPolicyControlled');
    const button = personalizationPage.$.wallpaperButton;
    assertTrue(!!button);
    assertFalse(button.disabled);
    button.click();
    await WallpaperBrowserProxy.whenCalled('openWallpaperManager');
  });

  test('wallpaperSettingVisible', function() {
    personalizationPage.showWallpaperRow_ = false;
    Polymer.dom.flush();
    assertTrue(personalizationPage.$$('#wallpaperButton').hidden);
  });

  test('wallpaperPolicyControlled', async () => {
    // Should show the wallpaper policy indicator and disable the toggle
    // button if the wallpaper is policy controlled.
    WallpaperBrowserProxy.setIsWallpaperPolicyControlled(true);
    createPersonalizationPage();
    await WallpaperBrowserProxy.whenCalled('isWallpaperPolicyControlled');
    Polymer.dom.flush();
    assertFalse(personalizationPage.$$('#wallpaperPolicyIndicator').hidden);
    assertTrue(personalizationPage.$$('#wallpaperButton').disabled);
  });

  test('Deep link to open wallpaper button', async () => {
    loadTimeData.overrideValues({isDeepLinkingEnabled: true});
    assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

    const params = new URLSearchParams;
    params.append('settingId', '500');
    settings.Router.getInstance().navigateTo(
        settings.routes.PERSONALIZATION, params);

    const deepLinkElement = personalizationPage.$.wallpaperButton.$$('#icon');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Wallpaper button should be focused for settingId=500.');
  });

  test('changePicture', function() {
    const row = personalizationPage.$.changePictureRow;
    assertTrue(!!row);
    row.click();
    assertEquals(
        settings.routes.CHANGE_PICTURE,
        settings.Router.getInstance().getCurrentRoute());
  });

  test('ambientMode', function() {
    const isGuest = loadTimeData.getBoolean('isGuest');
    const isAmbientModeEnabled = loadTimeData.getBoolean('isAmbientModeEnabled');

    if(!isGuest && isAmbientModeEnabled){
      const row = personalizationPage.$$('#ambientModeRow');
      assertTrue(!!row);
      row.click();
      assertEquals(
          settings.routes.AMBIENT_MODE,
          settings.Router.getInstance().getCurrentRoute());
    }
  });

  suite('PersonalizationTest_ReleaseOnly', function() {
    test('Deep link to change account picture', async () => {
      loadTimeData.overrideValues({isDeepLinkingEnabled: true});
      assertTrue(loadTimeData.getBoolean('isDeepLinkingEnabled'));

      const params = new URLSearchParams;
      params.append('settingId', '503');
      settings.Router.getInstance().navigateTo(
          settings.routes.CHANGE_PICTURE, params);

      Polymer.dom.flush();

      await test_util.waitAfterNextRender(personalizationPage);

      const changePicturePage =
          personalizationPage.$$('settings-change-picture');
      assertTrue(!!changePicturePage);
      const deepLinkElement = changePicturePage.$$('#pictureList')
                                  .$$('#selector')
                                  .$$('[class="iron-selected"]');
      await test_util.waitAfterNextRender(deepLinkElement);
      assertEquals(
          deepLinkElement, getDeepActiveElement(),
          'Account picture elem should be focused for settingId=503.');
    });
  });
});
