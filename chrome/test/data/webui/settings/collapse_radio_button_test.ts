// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://settings/lazy_load.js';

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
    return microtasksFinished();
  });

  test('openOnSelection', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertFalse(collapse.opened);
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertTrue(collapse.opened);
  });

  test('closeOnDeselect', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertTrue(collapse.opened);
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertFalse(collapse.opened);
  });

  // Button should remain closed when noAutomaticCollapse flag is set.
  test('closedWhenInitiallyClosedAndNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertFalse(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertFalse(collapse.opened);

    collapseRadioButton.updateCollapsed();
    await microtasksFinished();
    assertTrue(collapse.opened);
  });

  // Button should remain opened when noAutomaticCollapse flag is set.
  test('openedWhenInitiallyOpenedAndNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertTrue(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertTrue(collapse.opened);

    collapseRadioButton.updateCollapsed();
    await microtasksFinished();
    assertFalse(collapse.opened);
  });

  // When the button is not selected clicking the expand icon should still
  // open the iron collapse.
  test('openOnExpandHit', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertFalse(collapse.opened);
    collapseRadioButton.$.expandButton.click();
    await microtasksFinished();
    assertTrue(collapse.opened);
  });

  // When the button is selected clicking the expand icon should still close
  // the iron collapse.
  test('closeOnExpandHitWhenSelected', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertTrue(collapse.opened);
    collapseRadioButton.$.expandButton.click();
    await microtasksFinished();
    assertFalse(collapse.opened);
  });

  // When the noAutomaticCollapse flag if set, the expand arrow should expand
  // the radio button immediately.
  test('openOnExpandHitWhenNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = false;
    await microtasksFinished();
    assertFalse(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    await microtasksFinished();
    assertFalse(collapse.opened);

    collapseRadioButton.$.expandButton.click();
    await microtasksFinished();
    assertTrue(collapse.opened);
  });

  // When the noAutomaticCollapse flag if set, the expand arrow should collapse
  // the radio button immediately.
  test('closeOnExpandHitWhenSelectedWhenNoAutomaticCollapse', async function() {
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;
    collapseRadioButton.checked = true;
    await microtasksFinished();
    assertTrue(collapse.opened);

    collapseRadioButton.noAutomaticCollapse = true;
    await microtasksFinished();
    assertTrue(collapse.opened);

    collapseRadioButton.$.expandButton.click();
    await microtasksFinished();
    assertFalse(collapse.opened);
  });

  test('expansionHiddenWhenNoCollapseSet', async function() {
    assertTrue(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertTrue(isChildVisible(collapseRadioButton, '.separator'));

    collapseRadioButton.noCollapse = true;
    await microtasksFinished();
    assertFalse(isChildVisible(collapseRadioButton, 'cr-expand-button'));
    assertFalse(isChildVisible(collapseRadioButton, '.separator'));
  });

  test('openOnExpandHitWhenDisabled', async function() {
    collapseRadioButton.checked = false;
    collapseRadioButton.disabled = true;
    const collapse =
        collapseRadioButton.shadowRoot.querySelector('cr-collapse')!;

    await microtasksFinished();
    assertFalse(collapse.opened);
    collapseRadioButton.$.expandButton.click();

    await microtasksFinished();
    assertTrue(collapse.opened);
  });

  test('iconVisibleWhenSet', async function() {
    assertFalse(isChildVisible(collapseRadioButton, '#buttonIcon'));

    collapseRadioButton.icon = 'cr:location-on-filled';
    await microtasksFinished();
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
