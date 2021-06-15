// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {Router, routes, CrSettingsPrefs} from 'chrome://os-settings/chromeos/os_settings.js';
// #import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
// #import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
// #import {waitAfterNextRender} from 'chrome://test/test_util.m.js';
// clang-format on

let darkModePage = null;

function createDarkModePage() {
  PolymerTest.clearBody();

  const prefElement = document.createElement('settings-prefs');
  prefElement.set('prefs', {
    ash: {
      dark_mode: {
        enabled: {
          value: false,
        }
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
    const dark_mode_pref_string = 'ash.dark_mode.enabled.value';

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
});
