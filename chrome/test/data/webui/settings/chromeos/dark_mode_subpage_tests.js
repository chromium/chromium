// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes, CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.js';
// clang-format on

let darkModePage = null;

function createDarkModePage() {
  PolymerTest.clearBody();

  const prefElement = document.createElement('settings-prefs');
  prefElement.set('prefs', {
    ash: {
      dark_mode: {
        enabled: {
          key: 'ash.dark_mode.enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        color_mode_themed: {
          key: 'ash.dark_mode.color_mode_themed',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
      }
    }
  });
  document.body.appendChild(prefElement);

  darkModePage = document.createElement('settings-dark-mode-subpage');
  darkModePage.prefs = prefElement.prefs;
  document.body.appendChild(darkModePage);
  Polymer.dom.flush();
}

suite('DarkModeHandler', function() {
  suiteSetup(function() {
    PolymerTest.clearBody();
    loadTimeData.overrideValues({
      isDarkModeAllowed: true,
      isGuest: false,
    });
    assertTrue(loadTimeData.getBoolean('isDarkModeAllowed'));
  });

  setup(function() {
    createDarkModePage();
  });

  teardown(function() {
    darkModePage.remove();
  });

  test('toggleDarkMode', () => {
    const button = darkModePage.$$('#darkModeToggleButton');
    assertTrue(!!button);
    assertFalse(button.disabled);

    const getPrefValue = () => {
      return darkModePage.getPref('ash.dark_mode.enabled').value;
    };

    // Dark mode starts disabled.
    assertFalse(button.checked);
    assertFalse(getPrefValue());

    // Click the button, and dark mode should be enabled.
    button.click();
    Polymer.dom.flush();
    assertTrue(getPrefValue());

    // Click the button again to disable dark mode again.
    button.click();
    Polymer.dom.flush();
    assertFalse(getPrefValue());
  });

  test('toggleDarkModeThemed', () => {
    const darkModeThemedRadioGroup =
        darkModePage.$$('#darkModeThemedRadioGroup');
    assertTrue(!!darkModeThemedRadioGroup);

    const getPrefValue = () => {
      return darkModePage.getPref('ash.dark_mode.color_mode_themed').value;
    };

    // Dark mode themed starts disabled.
    assertFalse(getPrefValue());
    assertEquals('false', darkModeThemedRadioGroup.selected);

    // Enable theming from pref and expect an update to a radio button group.
    darkModePage.setPrefValue('ash.dark_mode.color_mode_themed', true);
    Polymer.dom.flush();
    assertEquals('true', darkModeThemedRadioGroup.selected);

    // Disable theming from pref and expect an update to a radio button group.
    darkModePage.setPrefValue('ash.dark_mode.color_mode_themed', false);
    Polymer.dom.flush();
    assertEquals('false', darkModeThemedRadioGroup.selected);

    // Clicking the 'on' radio should updates the theming pref to on.
    const darkModeThemedOn = darkModePage.$$('#darkModeThemedOn');
    darkModeThemedOn.click();
    Polymer.dom.flush();
    assertTrue(getPrefValue());

    // Clicking the 'off' radio should updates the theming pref to off.
    const darkModeThemedOff = darkModePage.$$('#darkModeThemedOff');
    darkModeThemedOff.click();
    Polymer.dom.flush();
    assertFalse(getPrefValue());
  });

  test('Deep link to dark mode toggle button', async () => {
    const params = new URLSearchParams;
    params.append('settingId', '505');
    settings.Router.getInstance().navigateTo(settings.routes.DARK_MODE, params);

    Polymer.dom.flush();

    const deepLinkElement = darkModePage.$$('#darkModeToggleButton')
                                .shadowRoot.querySelector('cr-toggle');

    await test_util.waitAfterNextRender(deepLinkElement);

    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Dark mode toggle should be focused for settingId=505.');
  });


  test('Deep link to dark mode theme radio group', async () => {
    const darkModeThemedRadioGroup =
        darkModePage.$$('#darkModeThemedRadioGroup');
    assertTrue(!!darkModeThemedRadioGroup);

    const getPrefValue = () => {
      return darkModePage.getPref('ash.dark_mode.color_mode_themed').value;
    };

    // Enable theming from pref and expect deep link to focus the themed-on
    // radio button.
    darkModePage.setPrefValue('ash.dark_mode.color_mode_themed', true);
    const params = new URLSearchParams;
    params.append('settingId', '506');
    settings.Router.getInstance().navigateTo(settings.routes.DARK_MODE, params);
    Polymer.dom.flush();
    let deepLinkElement = darkModePage.$$('#darkModeThemedOn').$$('#button');
    await test_util.waitAfterNextRender(deepLinkElement);

    assertEquals('true', darkModeThemedRadioGroup.selected);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Wallpaper (themed on) radio button should be focused for' +
            ' settingId=506 when dark mode is themed.');

    // Disable theming from pref and expect deep link to focus the themed-off
    // radio button.
    darkModePage.setPrefValue('ash.dark_mode.color_mode_themed', false);
    settings.Router.getInstance().navigateTo(settings.routes.DARK_MODE, params);
    Polymer.dom.flush();
    deepLinkElement = darkModePage.$$('#darkModeThemedOff').$$('#button');
    await test_util.waitAfterNextRender(deepLinkElement);

    assertEquals('false', darkModeThemedRadioGroup.selected);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Neutral (themed off) radio button should be focused for' +
            ' settingId=506 when dark mode is not themed.');
  });

});
