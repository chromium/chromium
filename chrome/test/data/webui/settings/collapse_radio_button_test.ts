// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement} from 'chrome://settings/lazy_load.js';
import {PrefsBrowserProxy, PrefService} from 'chrome://settings/settings.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible, microtasksFinished} from 'chrome://webui-test/test_util.js';

import {TestPrefsBrowserProxy} from './test_prefs_browser_proxy.js';

suite('CrCollapseRadioButton', function() {
  let collapseRadioButton: SettingsCollapseRadioButtonElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    collapseRadioButton =
        document.createElement('settings-collapse-radio-button');
    document.body.appendChild(collapseRadioButton);
    flush();
  });

  test('openOnSelection', function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
  });

  test('closeOnDeselect', function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
  });

  // Button should remain closed when noAutomaticCollapse flag is set.
  test('closedWhenInitiallyClosedAndNoAutomaticCollapse', function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    collapseRadioButton.checked = true;
    flush();
    assertFalse(collapse.opened);

    collapseRadioButton.updateCollapsed();
    flush();
    assertTrue(collapse.opened);
  });

  // Button should remain opened when noAutomaticCollapse flag is set.
  test('openedWhenInitiallyOpenedAndNoAutomaticCollapse', function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    collapseRadioButton.checked = false;
    flush();
    assertTrue(collapse.opened);

    collapseRadioButton.updateCollapsed();
    flush();
    assertFalse(collapse.opened);
  });

  // When the button is not selected clicking the expand icon should still
  // open the iron collapse.
  test('openOnExpandHit', async function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$.expandButton.click();
    await collapseRadioButton.$.expandButton.updateComplete;
    assertTrue(collapse.opened);
  });

  // When the button is selected clicking the expand icon should still close
  // the iron collapse.
  test('closeOnExpandHitWhenSelected', async function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);
    collapseRadioButton.$.expandButton.click();
    await collapseRadioButton.$.expandButton.updateComplete;
    assertFalse(collapse.opened);
  });

  // When the noAutomaticCollapse flag if set, the expand arrow should expand
  // the radio button immediately.
  test('openOnExpandHitWhenNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    flush();
    assertFalse(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    flush();
    assertFalse(collapse.opened);

    collapseRadioButton.$.expandButton.click();
    await collapseRadioButton.$.expandButton.updateComplete;
    assertTrue(collapse.opened);
  });

  // When the noAutomaticCollapse flag if set, the expand arrow should collapse
  // the radio button immediately.
  test('closeOnExpandHitWhenSelectedWhenNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    flush();
    assertTrue(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    flush();
    assertTrue(collapse.opened);

    collapseRadioButton.$.expandButton.click();
    await collapseRadioButton.$.expandButton.updateComplete;
    assertFalse(collapse.opened);
  });

  test('expansionHiddenWhenNoCollapseSet', function() {
    assertTrue(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertTrue(isChildVisible(collapseRadioButton, '.separator'));

    collapseRadioButton.noCollapse = true;
    flush();
    assertFalse(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertFalse(isChildVisible(collapseRadioButton, '.separator'));
  });

  test('openOnExpandHitWhenDisabled', async function() {
    collapseRadioButton.checked = false;
    collapseRadioButton.disabled = true;
    const collapse =
        collapseRadioButton.shadowRoot!.querySelector('cr-collapse')!;

    flush();
    assertFalse(collapse.opened);
    collapseRadioButton.$.expandButton.click();

    await collapseRadioButton.$.expandButton.updateComplete;
    assertTrue(collapse.opened);
  });

  test('iconVisibleWhenSet', function() {
    assertFalse(isChildVisible(collapseRadioButton, '#buttonIcon'));

    collapseRadioButton.set('icon', 'cr:location-on-filled');
    assertTrue(isChildVisible(collapseRadioButton, '#buttonIcon'));
  });
});

suite('CollapseRadioButtonPrefKey', () => {
  let collapseRadioButton: SettingsCollapseRadioButtonElement;
  let prefsBrowserProxy: TestPrefsBrowserProxy;

  const initialPrefs = [
    {
      key: 'test_boolean',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: true,
    },
    {
      key: 'test_number',
      type: chrome.settingsPrivate.PrefType.NUMBER,
      value: 0,
    },
  ];

  setup(async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    prefsBrowserProxy = new TestPrefsBrowserProxy(initialPrefs);
    PrefsBrowserProxy.setInstance(prefsBrowserProxy);
    PrefService.resetInstanceForTesting();
    await PrefService.getInstance().whenInitialized();

    collapseRadioButton =
        document.createElement('settings-collapse-radio-button');
    collapseRadioButton.prefKey = 'test_boolean';
    document.body.appendChild(collapseRadioButton);
  });

  test('disablesWhenPrefIsManaged', async () => {
    assertFalse(collapseRadioButton.disabled);

    // Make it managed.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_boolean',
      value: true,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.USER_POLICY,
    }]);
    await microtasksFinished();

    assertTrue(collapseRadioButton.disabled);
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
  });

  test('respectPreferenceState', async () => {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    collapseRadioButton =
        document.createElement('settings-collapse-radio-button');
    const togglePrefValue = 'pref_value';
    collapseRadioButton.name = togglePrefValue;
    collapseRadioButton.prefKey = 'test_number';
    document.body.appendChild(collapseRadioButton);

    // Case 1: Enforced by policy with no userSelectableValues; button is
    // disabled and policy indicator is visible.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_number',
      value: 0,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
    }]);
    await microtasksFinished();
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertTrue(collapseRadioButton.disabled);

    // Case 2: Enforced by policy with userSelectableValues excluding this
    // button's name; button is disabled and policy indicator is visible.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_number',
      value: 0,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      userSelectableValues: ['unrelated-value'],
    }]);
    await microtasksFinished();
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertTrue(collapseRadioButton.disabled);

    // Case 3: Enforced by policy with userSelectableValues including this
    // button's name; button is enabled and policy indicator is hidden.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_number',
      value: 0,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      userSelectableValues: [togglePrefValue],
    }]);
    await microtasksFinished();
    assertFalse(
        isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);

    // Case 4: Recommended by policy with recommendedValue not matching this
    // button's name; button is enabled and policy indicator is hidden.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_number',
      value: 0,
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      recommendedValue: 'unrelated-value',
    }]);
    await microtasksFinished();
    assertFalse(
        isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);

    // Case 5: Recommended by policy with recommendedValue matching this
    // button's name; button is enabled and policy indicator is visible.
    prefsBrowserProxy.fakeApi.sendPrefChanges([{
      key: 'test_number',
      value: 0,
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      recommendedValue: togglePrefValue,
    }]);
    await microtasksFinished();
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);
  });
});
