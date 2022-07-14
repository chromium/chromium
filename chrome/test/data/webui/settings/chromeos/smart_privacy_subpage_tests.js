// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router} from 'chrome://os-settings/chromeos/os_settings.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';

suite('SmartPrivacySubpageTests', function() {
  /** @type {SettingsSmartPrivacyPage} */
  let smartPrivacySubpage = null;

  /**
   * Generate preferences for the smart privacy page that either enable or
   * disable the snooping protection feature.
   * @ param {boolean} state Whether to enable or disable.
   * @return {!CrSettingsPrefs} The corresponding pref dictionary.
   * @private
   */
  function makePrefs(snoopingState) {
    return {
      'ash': {
        'privacy': {
          'snooping_protection_enabled': {
            value: snoopingState,
          },
        },
      },
    };
  }

  setup(async () => {
    // Options aren't shown unless the snooping protection feature is enabled.
    loadTimeData.overrideValues({
      isSnoopingProtectionEnabled: true,
    });

    PolymerTest.clearBody();
    smartPrivacySubpage = document.createElement('settings-smart-privacy-page');
    assertTrue(!!smartPrivacySubpage);
    smartPrivacySubpage.prefs = makePrefs(false);
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
    smartPrivacySubpage.prefs = makePrefs(true);
    flush();

    assertTrue(collapse.opened);
  });
});
