// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for cr_policy-network-indicator-mojo. */

import 'chrome://resources/ash/common/network/cr_policy_network_indicator_mojo.js';
import 'chrome://webui-test/chromeos/network/cr_policy_strings.js';

import {PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('cr-policy-network-indicator-mojo', function() {
  /** @type {!CrPolicyNetworkIndicatorMojoElement|undefined} */
  let indicator;

  function flushAsync() {
    flush();
    // Use setTimeout to wait for the next macrotask.
    return new Promise(resolve => setTimeout(resolve));
  }

  setup(function() {
    PolymerTest.clearBody();

    indicator = document.createElement('cr-policy-network-indicator-mojo');
    document.body.appendChild(indicator);
    flush();
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
      policySource: PolicySource.kUserPolicyRecommended,
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
      policySource: PolicySource.kDevicePolicyEnforced,
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
      policySource: PolicySource.kActiveExtension,
    };
    return flushAsync().then(() => {
      const icon = indicator.$$('cr-tooltip-icon');
      assertEquals('cr:extension', icon.iconClass);
    });
  });
});
