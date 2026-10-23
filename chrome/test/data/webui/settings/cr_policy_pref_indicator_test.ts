// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import 'chrome://settings/settings.js';

import type {CrPolicyPrefIndicatorElement} from 'chrome://settings/settings.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {CrPolicyStrings} from 'chrome://webui-test/cr_elements/cr_policy_strings.js';
import {microtasksFinished} from 'chrome://webui-test/test_util.js';
// clang-format on

/** @fileoverview Suite of tests for cr_policy-pref-indicator. */
suite('CrPolicyPrefIndicator', function() {
  let indicator: CrPolicyPrefIndicatorElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    indicator = document.createElement('cr-policy-pref-indicator');
    document.body.appendChild(indicator);
  });

  test('none', function() {
    const icon = indicator.$.tooltipIcon;
    assertTrue(icon.hidden);
  });

  test('pref', async function() {
    indicator.pref = {
      key: 'foo',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
    };
    await microtasksFinished();

    const icon = indicator.$.tooltipIcon;
    assertTrue(icon.hidden);

    // <if expr="is_chromeos">
    // Check indicator behavior for a preference controlled by the device owner.
    indicator.pref = Object.assign({}, indicator.pref, {
      controlledBy: chrome.settingsPrivate.ControlledBy.OWNER,
      controlledByName: 'owner_name',
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
    });
    await microtasksFinished();
    assertFalse(icon.hidden);
    assertEquals('cr:person-filled', icon.iconClass);
    assertEquals('owner: owner_name', icon.tooltipText);
    // </if>

    // Check indicator behavior for a preference with a recommended value that
    // is different from the current value of the preference.
    indicator.pref = Object.assign({}, indicator.pref, {
      value: 'foo',
      recommendedValue: 'bar',
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
    });
    await microtasksFinished();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    // Check indicator behavior for a preference with a recommended value that
    // is the same as the current value of the preference.
    indicator.pref = Object.assign({}, indicator.pref, {value: 'bar'});
    await microtasksFinished();
    assertEquals('matches', icon.tooltipText);

    // <if expr="is_chromeos">
    // Check indicator behavior for a preference that is enforced for a
    // supervised user.
    indicator.pref = Object.assign({}, indicator.pref, {
      enforcement: chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED,
    });
    await microtasksFinished();
    assertFalse(icon.hidden);
    assertEquals('cr20:family-link', icon.iconClass);
    assertEquals(CrPolicyStrings.controlledSettingParent, icon.tooltipText);
    // </if>

    // Check indicator behavior for a preference that is enforced by device
    // policy.
    indicator.pref = Object.assign({}, indicator.pref, {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.DEVICE_POLICY,
    });
    await microtasksFinished();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals(CrPolicyStrings.controlledSettingPolicy, icon.tooltipText);

    // Check indicator behavior for an preference that is enforced whilst also
    // having a recommended value.
    const indicatorPrefValue = 1;
    const differentPrefValue = 2;
    indicator.associatedValue = indicatorPrefValue;
    indicator.pref = Object.assign({}, indicator.pref, {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      userSelectableValues: [indicatorPrefValue, differentPrefValue],
    });
    await microtasksFinished();
    assertTrue(icon.hidden);

    indicator.pref = Object.assign({}, indicator.pref, {
      recommendedValue: differentPrefValue,
      value: differentPrefValue,
    });
    await microtasksFinished();
    assertTrue(icon.hidden);

    indicator.pref = Object.assign({}, indicator.pref, {
      recommendedValue: indicatorPrefValue,
    });
    await microtasksFinished();
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.pref =
        Object.assign({}, indicator.pref, {value: indicatorPrefValue});
    await microtasksFinished();
    assertEquals('matches', icon.tooltipText);

    // Check indicator behavior for an preference that is recommended whilst the
    // indicator has an associated value.
    indicator.pref = Object.assign({}, indicator.pref, {
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      userSelectableValues: [],
      value: differentPrefValue,
      recommendedValue: differentPrefValue,
    });
    await microtasksFinished();
    assertTrue(icon.hidden);

    indicator.pref = Object.assign({}, indicator.pref, {
      recommendedValue: indicatorPrefValue,
    });
    await microtasksFinished();
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.pref =
        Object.assign({}, indicator.pref, {value: indicatorPrefValue});
    await microtasksFinished();
    assertEquals('matches', icon.tooltipText);
  });

  test('controlledBySafeBrowsingIsOff', async function() {
    indicator.pref = {
      key: 'foo',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: false,
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      controlledBy: chrome.settingsPrivate.ControlledBy.SAFE_BROWSING_OFF,
    };
    await microtasksFinished();
    assertEquals('none', indicator.indicatorType);
  });
});
