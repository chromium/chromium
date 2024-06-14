// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import  'chrome://settings/lazy_load.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {SettingsCollapseRadioButtonElement} from 'chrome://settings/lazy_load.js';
import {assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {isChildVisible} from 'chrome://webui-test/test_util.js';

// clang-format on

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

  test('respectPreferenceState', function() {
    const togglePrefValue = 'pref_value';
    collapseRadioButton.name = togglePrefValue;
    collapseRadioButton.pref = {
      type: chrome.settingsPrivate.PrefType.NUMBER,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
      key: 'test',
      value: 0,
    };
    flush();
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertTrue(collapseRadioButton.disabled);

    collapseRadioButton.set('pref.userSelectableValues', ['unrelated-value']);
    flush();
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertTrue(collapseRadioButton.disabled);

    collapseRadioButton.set('pref.userSelectableValues', [togglePrefValue]);
    flush();
    assertFalse(
        isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);

    collapseRadioButton.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.RECOMMENDED);
    collapseRadioButton.set('pref.recommendedValue', 'unrelated-value');
    flush();
    assertFalse(
        isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);

    collapseRadioButton.set('pref.recommendedValue', togglePrefValue);
    assertTrue(isChildVisible(collapseRadioButton, 'cr-policy-pref-indicator'));
    assertFalse(collapseRadioButton.disabled);
  });

  test('iconVisibleWhenSet', function() {
    assertFalse(isChildVisible(collapseRadioButton, '#buttonIcon'));

    collapseRadioButton.set('icon', 'cr:location-on');
    assertTrue(isChildVisible(collapseRadioButton, '#buttonIcon'));
  });
});
