// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-network-indicator. */
suite('cr-policy-network-indicator', function() {
  /** @type {!CrPolicyNetworkIndicatorElement|undefined} */
  let indicator;

  /** @type {!CrTooltipIconElement|undefined} */
  let icon;

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-network-indicator');
    document.body.appendChild(indicator);
    icon = indicator.$$('cr-tooltip-icon');
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('hidden by default', function() {
    assertTrue(icon.hidden);
  });

  test('no policy', function() {
    indicator.property = {Active: 'foo'};
    Polymer.dom.flush();
    assertTrue(icon.hidden);
  });

  test('recommended', function() {
    indicator.property = {
      Active: 'foo',
      UserPolicy: 'bar',
      UserEditable: true,
      Effective: 'UserPolicy',
    };
    Polymer.dom.flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('differs', icon.tooltipText);

    indicator.set('property.Active', 'bar');
    Polymer.dom.flush();
    assertEquals('matches', icon.tooltipText);
  });

  test('policy', function() {
    indicator.property = {
      DevicePolicy: 'foo',
      Effective: 'DevicePolicy',
    };
    Polymer.dom.flush();
    assertFalse(icon.hidden);
    assertEquals('cr20:domain', icon.iconClass);
    assertEquals('policy', icon.tooltipText);
  });

  test('extension', function() {
    indicator.property = {
      Active: 'foo',
      Effective: 'ActiveExtension',
    };
    Polymer.dom.flush();
    assertEquals('cr:extension', icon.iconClass);
  });
});
