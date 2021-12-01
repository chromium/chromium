// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {PersonalizationHubBrowserProxyImpl, WallpaperBrowserProxyImpl, routes, Router} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {TestPersonalizationHubBrowserProxy} from './test_personalization_hub_browser_proxy.m.js';
// #import {TestWallpaperBrowserProxy} from './test_wallpaper_browser_proxy.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {flushTasks, waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

let personalizationPage = null;

/** @type {?settings.TestPersonalizationHubBrowserProxy} */
let PersonalizationHubBrowserProxy = null;

/** @type {?settings.TestWallpaperBrowserProxy} */
let WallpaperBrowserProxy = null;

function createPersonalizationPage() {
  PersonalizationHubBrowserProxy.reset();
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
    ash: {
      dark_mode: {
        enabled: {
          value: true,
        }
      }
    }
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
    PersonalizationHubBrowserProxy =
        new settings.TestPersonalizationHubBrowserProxy();
    settings.PersonalizationHubBrowserProxyImpl.instance_ =
        PersonalizationHubBrowserProxy;
    createPersonalizationPage();
  });

  teardown(async function() {
    personalizationPage.remove();
    settings.Router.getInstance().resetRouteForTesting();
    await test_util.flushTasks();
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
    const params = new URLSearchParams;
    params.append('settingId', '500');
    settings.Router.getInstance().navigateTo(
        settings.routes.PERSONALIZATION, params);

    const deepLinkElement =
        personalizationPage.shadowRoot.getElementById('wallpaperButton')
            .shadowRoot.querySelector('#icon');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Wallpaper button should be focused for settingId=500.');
  });

  test('changePicture', function() {
    const row =
        personalizationPage.shadowRoot.getElementById('changePictureRow');
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

  test('darkMode', function() {
    const isGuest = loadTimeData.getBoolean('isGuest');
    // Enable dark mode feature and guest mode, so dark mode row should be
    // hidden due to no personalization section show in the guest mode.
    loadTimeData.overrideValues({isDarkModeAllowed: true, isGuest: true});
    assertTrue(loadTimeData.getBoolean('isDarkModeAllowed'));
    Polymer.dom.flush();
    let row = personalizationPage.$$('#darkModeRow');
    assertTrue(!row);

    // Disable guest mode and check that dark mode row shows up.
    loadTimeData.overrideValues({isDarkModeAllowed: true, isGuest: false});
    assertFalse(loadTimeData.getBoolean('isGuest'));
    createPersonalizationPage();
    Polymer.dom.flush();
    row = personalizationPage.$$('#darkModeRow');
    assertFalse(!row);
    row.click();
    assertTrue(
        settings.routes.DARK_MODE ===
        settings.Router.getInstance().getCurrentRoute());

    // Disable dark mode feature and check that dark mode row is hidden.
    loadTimeData.overrideValues({isDarkModeAllowed: false, isGuest: false});
    assertFalse(loadTimeData.getBoolean('isDarkModeAllowed'));
    createPersonalizationPage();
    personalizationPage.prefs.ash.dark_mode.enabled.value = false;
    Polymer.dom.flush();
    row = personalizationPage.$$('#darkModeRow');
    assertTrue(!row);
  });

  test('Deep link to change account picture', async () => {
    const params = new URLSearchParams;
    params.append('settingId', '503');
    settings.Router.getInstance().navigateTo(
        settings.routes.CHANGE_PICTURE, params);

    Polymer.dom.flush();

    await test_util.waitAfterNextRender(personalizationPage);

    const changePicturePage = personalizationPage.$$('settings-change-picture');
    assertTrue(!!changePicturePage);
    const deepLinkElement = changePicturePage.$$('#pictureList')
                                .$$('#selector')
                                .$$('[class="iron-selected"]');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Account picture elem should be focused for settingId=503.');
  });

  test('Personalization hub feature shows only link to hub', async () => {
    loadTimeData.overrideValues({isPersonalizationHubEnabled: true});
    assertTrue(loadTimeData.getBoolean('isPersonalizationHubEnabled'));
    createPersonalizationPage();
    Polymer.dom.flush();
    await test_util.waitAfterNextRender(personalizationPage);

    const crLinks =
        personalizationPage.shadowRoot.querySelectorAll('cr-link-row');

    assertEquals(1, crLinks.length);
    assertEquals('personalizationHubButton', crLinks[0].id);
  });

  test('Opens personalization hub when clicked', async () => {
    loadTimeData.overrideValues({isPersonalizationHubEnabled: true});
    assertTrue(loadTimeData.getBoolean('isPersonalizationHubEnabled'));
    createPersonalizationPage();
    Polymer.dom.flush();
    await test_util.waitAfterNextRender(personalizationPage);

    const hubLink = personalizationPage.shadowRoot.getElementById(
        'personalizationHubButton');
    hubLink.click();

    await PersonalizationHubBrowserProxy.whenCalled('openPersonalizationHub');
  });
});
