// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {CrExpandButtonElement, SecurityPageFeatureRowElement} from 'chrome://settings/lazy_load.js';
import type {SettingsPrefsElement, SettingsToggleButtonElement} from 'chrome://settings/settings.js';
import {CrSettingsPrefs} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {FakeSettingsPrivate} from 'chrome://webui-test/fake_settings_private.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

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

    securityPageFeatureRow =
        document.createElement('security-page-feature-row');
    securityPageFeatureRow.pref = settingsPrefs.get('prefs.test');

    document.body.appendChild(securityPageFeatureRow);
    flush();
  });

  function getExpandButton(): HTMLElement|null {
    return securityPageFeatureRow.shadowRoot!
        .querySelector<CrExpandButtonElement>('#expandButton');
  }

  function getToggleButton(): HTMLElement|null {
    return securityPageFeatureRow.shadowRoot!
        .querySelector<SettingsToggleButtonElement>('#toggleButton');
  }

  test('RowClickExpandsAndCollapses', async function() {
    const collapse =
        securityPageFeatureRow.shadowRoot!.querySelector('cr-collapse')!;
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);

    // Expand the feature row.
    getExpandButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);
    assertTrue(collapse.opened);

    // Collapse the feature row.
    getExpandButton()!.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(collapse.opened);
  });

  test('ToggleClickEnablesAndDisablesFeature', async function() {
    assertFalse(securityPageFeatureRow.pref.value);

    // Expand the feature row in order to see the toggle.
    getExpandButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // Enable the feature.
    getToggleButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.pref.value);

    // Disable the feature again.
    getToggleButton()!.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.pref.value);
  });

  test('RowClickShowsAndHidesToggle', async function() {
    // Since the row starts off collapsed, the toggle shouldn't be visible.
    assertFalse(securityPageFeatureRow.expanded);
    assertFalse(isChildVisible(securityPageFeatureRow, '#toggleButton'));

    // Expand the feature row.
    getExpandButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // Check that toggle is visible
    assertTrue(isChildVisible(securityPageFeatureRow, '#toggleButton'));

    // Collapse the feature row.
    getExpandButton()!.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);

    // Check that toggle is NOT visible.
    assertFalse(isChildVisible(securityPageFeatureRow, '#toggleButton'));
  });

  test('StateLabelIsVisibleWhenCollapsed', async function() {
    // The row is collapsed by default, so the state label should be visible.
    assertFalse(securityPageFeatureRow.expanded);
    let stateLabel =
        getExpandButton()!.querySelector<HTMLElement>('#stateLabel');
    assertTrue(!!stateLabel);
    assertTrue(stateLabel.offsetParent !== null);

    // Expand the feature row.
    getExpandButton()!.click();
    await microtasksFinished();
    assertTrue(securityPageFeatureRow.expanded);

    // The state label should now be hidden aka null.
    stateLabel = getExpandButton()!.shadowRoot!.querySelector<HTMLElement>(
        '#stateLabel');
    assertTrue(!stateLabel);

    // Collapse the feature row again.
    getExpandButton()!.click();
    await microtasksFinished();
    assertFalse(securityPageFeatureRow.expanded);

    // The state label should be visible again.
    stateLabel = getExpandButton()!.querySelector<HTMLElement>('#stateLabel');
    assertTrue(!!stateLabel);
    assertTrue(stateLabel.offsetParent !== null);
  });
});
