// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/lazy_load.js';

import {SettingsSmartPrivacySubpage} from 'chrome://os-settings/lazy_load.js';
import {IronCollapseElement, Router} from 'chrome://os-settings/os_settings.js';
import {assert} from 'chrome://resources/js/assert.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('<settings-smart-privacy-subpage>', () => {
  let smartPrivacySubpage: SettingsSmartPrivacySubpage;

  /**
   * Generate preferences for the smart privacy page that either enable or
   * disable the quick dim or snooping protection feature.
   * @param quickDimState To enable or disable quick dim.
   * @param snoopingState To enable or disable snooping protection.
   * @return The corresponding pref dictionary.
   */
  function makePrefs(quickDimState: boolean, snoopingState: boolean) {
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

    smartPrivacySubpage =
        document.createElement('settings-smart-privacy-subpage');
    smartPrivacySubpage.prefs = makePrefs(false, false);
    document.body.appendChild(smartPrivacySubpage);
  });

  teardown(async () => {
    smartPrivacySubpage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Snooping radio list visibility tied to pref', async () => {
    // The $ method won't find elements inside templates.
    const collapse =
        smartPrivacySubpage.shadowRoot!.querySelector<IronCollapseElement>(
            '#snoopingProtectionOptions');
    assert(collapse);

    // Default pref value is false.
    assertFalse(collapse.opened);

    // Atomic reassign to so that Polymer notices the change.
    smartPrivacySubpage.prefs = makePrefs(false, true);
    flush();

    assertTrue(collapse.opened);
  });

  test('Quick dim slider visibility tied to pref', async () => {
    // The $ method won't find elements inside templates.
    const collapse =
        smartPrivacySubpage.shadowRoot!.querySelector<IronCollapseElement>(
            '#quickDimOptions');
    assert(collapse);

    // Default pref value is false.
    assertFalse(collapse.opened);

    // Atomic reassign to so that Polymer notices the change.
    smartPrivacySubpage.prefs = makePrefs(true, false);
    flush();

    assertTrue(collapse.opened);
  });
});
