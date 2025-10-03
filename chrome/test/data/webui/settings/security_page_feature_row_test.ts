// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import  'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SecurityPageFeatureRowElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';

// clang-format on

suite('securityPageFeatureRow', function() {
  let securityPageFeatureRow: SecurityPageFeatureRowElement;
  let settingsPrefs: SettingsPrefsElement;

  suiteSetup(function() {
    CrSettingsPrefs.deferInitialization = true;
  });

  setup(async function() {
    const fakePref = [{
      key: 'test',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    }];
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    settingsPrefs = document.createElement('settings-prefs');
    settingsPrefs.initialize(new FakeSettingsPrivate(fakePref));
    document.body.appendChild(settingsPrefs);
    await CrSettingsPrefs.initialized;

    securityPageFeatureRow = document.createElement('security-page-feature-row');
    securityPageFeatureRow.pref = settingsPrefs.get('prefs.test');

    document.body.appendChild(securityPageFeatureRow);
    flush();
  });

  test('RowClickExpandsAndCollapses', async function() {
    const collapse =
        securityPageFeatureRow.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);

    // Expand the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);
    assertTrue(collapse.opened);

    // Collapse the feature row.
    securityPageFeatureRow.$.expandButton.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);
  });

  test('ToggleClickEnablesAndDisablesFeature', async function() {
    assertFalse(securityPageFeatureRow.pref.value);

    // Enable the feature.
    securityPageFeatureRow.$.toggleButton.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.pref.value);

    // Disable the feature again.
    securityPageFeatureRow.$.toggleButton.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.pref.value);
  });
});
