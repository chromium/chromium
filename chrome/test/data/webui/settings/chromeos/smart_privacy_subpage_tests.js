// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('SmartPrivacySubpageTests', function() {
  /** @type {SettingsSmartPrivacyPage} */
  let smartPrivacySubpage = null;

  /**
   * Generate preferences for the smart privacy page that either enable or
   * disable the quick dim or snooping protection feature.
   * @ param {boolean} quickDimState To enable or disable quick dim.
   * @ param {boolean} snoopingState To enable or disable snooping protection.
   * @return {!CrSettingsPrefs} The corresponding pref dictionary.
   * @private
   */
  function makePrefs(quickDimState, snoopingState) {
    return {
      'ash': {
        'privacy': {
          'snooping_protection_enabled': {
            value: snoopingState,
          },
        },
      },
      'power': {
        'quick_dim_enabled': {
          value: quickDimState,
        },
      },
    };
  }

  setup(async () => {
    // Options aren't shown unless the feature is enabled.
    loadTimeData.overrideValues({
      isQuickDimEnabled: true,
      isSnoopingProtectionEnabled: true,
    });

    PolymerTest.clearBody();
    smartPrivacySubpage = document.createElement('settings-smart-privacy-page');
    assertTrue(!!smartPrivacySubpage);
    smartPrivacySubpage.prefs = makePrefs(false, false);
    document.body.appendChild(smartPrivacySubpage);
  });

  teardown(async () => {
    smartPrivacySubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Snooping radio list visibility tied to pref', async () => {
    // The $ method won't find elements inside templates.
    /** @type {?HTMLElement} */
    const collapse = smartPrivacySubpage.shadowRoot.querySelector(
        '#snoopingProtectionOptions');

    // Default pref value is false.
    assertFalse(collapse.opened);

    // Atomic reassign to so that Polymer notices the change.
    smartPrivacySubpage.prefs = makePrefs(false, true);
    flush();

    assertTrue(collapse.opened);
  });

  test('Quick dim slider visibility tied to pref', async () => {
    // The $ method won't find elements inside templates.
    /** @type {?HTMLElement} */
    const collapse =
        smartPrivacySubpage.shadowRoot.querySelector('#quickDimOptions');

    // Default pref value is false.
    assertFalse(collapse.opened);

    // Atomic reassign to so that Polymer notices the change.
    smartPrivacySubpage.prefs = makePrefs(true, false);
    flush();

    assertTrue(collapse.opened);
  });
});
