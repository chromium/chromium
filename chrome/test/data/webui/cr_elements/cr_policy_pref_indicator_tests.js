// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-pref-indicator. */
suite('CrPolicyPrefIndicator', function() {
  /** @type {!CrPolicyPrefIndicatorElement|undefined} */
  let indicator;

  /** @type {!CrTooltipIconElement|undefined} */
  let icon;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-pref-indicator');
    document.body.appendChild(indicator);
    icon = indicator.$$('cr-tooltip-icon');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
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
    Polymer.dom.flush();
    assertTrue(icon.hidden);

    indicator.set(
        'pref.controlledBy', chrome.settingsPrivate.ControlledBy.OWNER);
    indicator.set('pref.controlledByName', 'owner_name');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.ENFORCED);
    Polymer.dom.flush();
    assertFalse(icon.hidden);
    assertEquals('cr:person', icon.iconClass);
    assertEquals('owner: owner_name', icon.tooltipText);

    indicator.set('pref.value', 'foo');
    indicator.set('pref.recommendedValue', 'bar');
    indicator.set(
        'pref.enforcement', chrome.settingsPrivate.Enforcement.RECOMMENDED);
    Polymer.dom.flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.set('pref.value', 'bar');
    Polymer.dom.flush();
    assertEquals('matches', icon.tooltipText);

    indicator.set(
        'pref.enforcement',
        chrome.settingsPrivate.Enforcement.PARENT_SUPERVISED);
    Polymer.dom.flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:kite', icon.iconClass);
    assertEquals(CrPolicyStrings.controlledSettingParent, icon.tooltipText);
  });
});
