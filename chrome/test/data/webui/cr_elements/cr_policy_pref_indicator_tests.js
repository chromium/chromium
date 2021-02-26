// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import './cr_policy_strings.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
// clang-format on

/** @fileoverview Suite of tests for cr_policy-pref-indicator. */
suite('CrPolicyPrefIndicator', function() {
  /** @type {!CrPolicyPrefIndicatorElement} */
  let indicator;

  /** @type {!CrTooltipIconElement} */
  let icon;

  setup(function() {
    document.body.innerHTML = '';

    indicator = /** @type {!CrPolicyPrefIndicatorElement} */ (
        document.createElement('cr-policy-pref-indicator'));
    document.body.appendChild(indicator);
    icon =
        /** @type {!CrTooltipIconElement} */ (indicator.$$('cr-tooltip-icon'));
  });

  test('none', function() {
    assertTrue(icon.hidden);
  });

  test('pref', function() {
    /** @type {!chrome.settingsPrivate.PrefObject} */
    indicator.pref = {
      key: 'foo',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    flush();
    assertTrue(icon.hidden);

    // Check indicator behavior for a preference controlled by the device owner.
    indicator.set(
        'pref.controlledBy', chrome.settingsPrivate.ControlledBy.OWNER);
    indicator.set('pref.controlledByName', 'owner_name');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    flush();
    assertFalse(icon.hidden);
    assertEquals('cr:person', icon.iconClass);
    assertEquals('owner: owner_name', icon.tooltipText);

    // Check indicator behavior for a preference with a recommended value that
    // is different from the current value of the preference.
    indicator.set('pref.value', 'foo');
    indicator.set('pref.recommendedValue', 'bar');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.RECOMMENDED);
    flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    // Check indicator behavior for a preference with a recommended value that
    // is the same as the current value of the preference.
    indicator.set('pref.value', 'bar');
    flush();
    assertEquals('matches', icon.tooltipText);

    // Check indicator behavior for a preference that is enforced for a
    // supervised user.
    indicator.set(
        'pref.enforcement',
        chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED);
    flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals(
        window.CrPolicyStrings.controlledSettingParent, icon.tooltipText);

    // Check indicator behavior for a preference that is enforced by device
    // policy.
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    indicator.set(
        'pref.controlledBy', chrome.settingsPrivate.ControlledBy.DEVICE_POLICY);
    flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals(
        window.CrPolicyStrings.controlledSettingPolicy, icon.tooltipText);

    // Check indicator behavior for an preference that is enforced whilst also
    // having a recommended value.
    const indicatorPrefValue = 1;
    const differentPrefValue = 2;
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);

    indicator.set('associatedValue', indicatorPrefValue);
    indicator.set(
        'pref.userSelectableValues', [indicatorPrefValue, differentPrefValue]);
    flush();
    assertTrue(icon.hidden);

    indicator.set('pref.recommendedValue', differentPrefValue);
    indicator.set('pref.value', differentPrefValue);
    flush();
    assertTrue(icon.hidden);

    indicator.set('pref.recommendedValue', indicatorPrefValue);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.set('pref.value', indicatorPrefValue);
    flush();
    assertEquals('matches', icon.tooltipText);

    // Check indicator behavior for an preference that is recommended whilst the
    // indicator has an associated value.
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.RECOMMENDED);
    indicator.set('pref.userSelectableValues', []);
    indicator.set('pref.value', differentPrefValue);

    indicator.set('pref.recommendedValue', differentPrefValue);
    flush();
    assertTrue(icon.hidden);

    indicator.set('pref.recommendedValue', indicatorPrefValue);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.set('pref.value', indicatorPrefValue);
    flush();
    assertEquals('matches', icon.tooltipText);

  });
});
