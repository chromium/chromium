// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/lazy_load.js';

// #import {CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js'
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {assert} from 'chrome://resources/js/assert.m.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

function getFakePrefs() {
  return {
    settings: {
      restore_apps_and_pages: {
        key: 'settings.restore_apps_and_pages',
        type: chrome.settingsPrivate.PrefType.NUMBER,
        value: 1,
      }
    }
  };
}

suite('settings-on-startup-page', function() {
  let onstartupPage;

  setup(function() {
    PolymerTest.clearBody();
    onstartupPage = document.createElement('settings-on-startup-page');
    assertTrue(!!onstartupPage);
    onstartupPage.prefs = getFakePrefs();

    document.body.appendChild(onstartupPage);
    Polymer.dom.flush();
  });

  teardown(function() {
    settings.Router.getInstance().resetRouteForTesting();
    onstartupPage.remove();
  });

  test('MainPage', async () => {
    const onStartupRadioGroup =
        assert(onstartupPage.$$('#onStartupRadioGroup'));

    const onStartupAlways = assert(onstartupPage.$$('#onStartupAlways'));
    assertEquals(
        1, onstartupPage.getPref('settings.restore_apps_and_pages').value);

    // Set the restore on startup option by clicking the 'AskEveryTime' radio.
    const onStartupAskEveryTime =
        assert(onstartupPage.$$('#onStartupAskEveryTime'));
    onStartupAskEveryTime.click();
    assertEquals(
        2, onstartupPage.getPref('settings.restore_apps_and_pages').value);

    // Set the restore on startup option by clicking the 'DoNotRestore' radio.
    const onStartupDoNotRestore =
        assert(onstartupPage.$$('#onStartupDoNotRestore'));
    onStartupDoNotRestore.click();
    assertEquals(
        3, onstartupPage.getPref('settings.restore_apps_and_pages').value);
  });

  test('Deep link to the restore apps and pages radio group', async () => {
    loadTimeData.overrideValues({
      isDeepLinkingEnabled: true,
    });

    // Set the restore on startup option.
    onstartupPage.setPrefValue('settings.restore_apps_and_pages', 2);

    const params = new URLSearchParams;
    params.append('settingId', '1900');
    settings.Router.getInstance().navigateTo(
        settings.routes.ON_STARTUP, params);

    const deepLinkElement =
        onstartupPage.$$('#onStartupAskEveryTime').$$('#button');
    await test_util.waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Restore apps and pages radio group should be focused for settingId=1900.');
  });
});
