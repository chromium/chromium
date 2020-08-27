// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {WallpaperBrowserProxyImpl, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.m.js';
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
});
