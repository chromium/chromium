// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  Polymer.dom.flush();
}

suite('PersonalizationHandler', function() {
  suiteSetup(function() {
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    WallpaperBrowserProxy = new TestWallpaperBrowserProxy();
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
    assertEquals(settings.routes.CHANGE_PICTURE, settings.getCurrentRoute());
  });
});
