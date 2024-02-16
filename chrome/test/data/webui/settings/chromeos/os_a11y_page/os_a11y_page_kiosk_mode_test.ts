// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {OsSettingsA11yPageElement, Router, routes} from 'chrome://os-settings/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse} from 'chrome://webui-test/chai_assert.js';
import {isVisible} from 'chrome://webui-test/test_util.js';

suite('OsSettingsA11yPageKioskModeTest', () => {
  let page: OsSettingsA11yPageElement;
  const defaultPrefs = {
    'settings': {
      'a11y': {
        'tablet_mode_shelf_nav_buttons_enabled': {
          key: 'settings.a11y.tablet_mode_shelf_nav_buttons_enabled',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: false,
        },
        'dictation': {
          key: 'prefs.settings.a11y.dictation',
          type: chrome.settingsPrivate.PrefType.BOOLEAN,
          value: true,
        },
        'dictation_locale': {
          key: 'prefs.settings.a11y.dictation_locale',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: 'en-US',
        },
      },
      'accessibility': {
        key: 'settings.accessibility',
        type: chrome.settingsPrivate.PrefType.BOOLEAN,
        value: false,
      },
    },
  };

  function initPage() {
    page = document.createElement('os-settings-a11y-page');
    page.prefs = defaultPrefs;
    document.body.appendChild(page);
  }

  setup(() => {
    loadTimeData.overrideValues({isKioskModeActive: true});
    Router.getInstance().navigateTo(routes.MANAGE_ACCESSIBILITY);
  });

  teardown(() => {
    page.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test(
      'Page loads without crashing when prefs are not yet initialized in kiosk mode',
      () => {
        loadTimeData.overrideValues({isKioskModeActive: true});
        page = document.createElement('os-settings-a11y-page');
        document.body.appendChild(page);

        // Intentionally set prefs after page is appended to DOM to simulate
        // asynchronicity of initializing prefs
        page.prefs = defaultPrefs;
      });

  test('some parts are hidden in kiosk mode', () => {
    loadTimeData.overrideValues({isKioskModeActive: true});
    initPage();
    flush();

    // Should stay on MANAGE_ACCESSIBILITY in kiosk mode.
    assertEquals(
        Router.getInstance().currentRoute, routes.MANAGE_ACCESSIBILITY);

    // Show accessibility options in Quick Settings toggle is not visible.
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#optionsInMenuToggle')));

    // Additional features link is not visible.
    assertFalse(
        isVisible(page.shadowRoot!.querySelector('#additionalFeaturesLink')));
  });

  test('Always redirect to MANAGE_ACCESSIBILITY route in kiosk mode', () => {
    loadTimeData.overrideValues({isKioskModeActive: true});
    Router.getInstance().navigateTo(routes.OS_ACCESSIBILITY);
    initPage();
    flush();

    assertEquals(
        Router.getInstance().currentRoute, routes.MANAGE_ACCESSIBILITY);
  });
});
