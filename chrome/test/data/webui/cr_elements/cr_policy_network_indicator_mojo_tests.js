// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-network-indicator-mojo. */
suite('cr-policy-network-indicator-mojo', function() {
  /** @type {!CrPolicyNetworkIndicatorMojoElement|undefined} */
  let indicator;

  function flushAsync() {
    Polymer.dom.flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-network-indicator-mojo');
    document.body.appendChild(indicator);
    Polymer.dom.flush();
    return new Promise(resolve => setTimeout(resolve));
  });

  teardown(function() {
    PolymerTest.clearBody();  // crbug.com/680169
  });

  test('hidden by default', function() {
    const icon = indicator.$$('cr-tooltip-icon');
    assertTrue(icon.hidden);
  });

  test('no policy', function() {
    indicator.property = {activeValue: 'foo'};
    return flushAsync().then(() => {
      const icon = indicator.$$('cr-tooltip-icon');
      assertTrue(icon.hidden);
    });
  });

  test('recommended', function() {
    indicator.property = {
      activeValue: 'foo',
      policySource:
          chromeos.networkConfig.mojom.PolicySource.kUserPolicyRecommended,
      policyValue: 'bar',
    };
    return flushAsync()
        .then(() => {
          const icon = indicator.$$('cr-tooltip-icon');
          assertFalse(icon.hidden);
          assertEquals('cr20:domain', icon.iconClass);
          assertEquals(
              CrPolicyStrings.controlledSettingRecommendedDiffers,
              icon.tooltipText);

          indicator.set('property.activeValue', 'bar');
          return flushAsync();
        })
        .then(() => {
          const icon = indicator.$$('cr-tooltip-icon');
          assertEquals(
              CrPolicyStrings.controlledSettingRecommendedMatches,
              icon.tooltipText);
        });
  });

  test('policy', function() {
    indicator.property = {
      activeValue: 'foo',
      policySource:
          chromeos.networkConfig.mojom.PolicySource.kDevicePolicyEnforced,
      policyValue: 'foo',
    };
    return flushAsync().then(() => {
      const icon = indicator.$$('cr-tooltip-icon');
      assertFalse(icon.hidden);
      assertEquals('cr20:domain', icon.iconClass);
      assertEquals(CrPolicyStrings.controlledSettingPolicy, icon.tooltipText);
    });
  });

  test('extension', function() {
    indicator.property = {
      activeValue: 'foo',
      policySource: chromeos.networkConfig.mojom.PolicySource.kActiveExtension,
    };
    return flushAsync().then(() => {
      const icon = indicator.$$('cr-tooltip-icon');
      assertEquals('cr:extension', icon.iconClass);
    });
  });
});
