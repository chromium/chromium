// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */

import {CrPolicyIndicatorType} from 'chrome://resources/ash/common/cr_policy_indicator_behavior.js';
import {CrPolicyNetworkBehaviorMojo} from 'chrome://resources/ash/common/network/cr_policy_network_behavior_mojo.js';
import {PolicySource} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

suite('CrPolicyNetworkBehaviorMojo', function() {
  const TestElementBase =
      mixinBehaviors([CrPolicyNetworkBehaviorMojo], PolymerElement);

  class TestBehaviorElement extends TestElementBase {
    static get is() {
      return 'test-behavior';
    }
  }
  customElements.define(TestBehaviorElement.is, TestBehaviorElement);

  let testBehavior;

  setup(function() {
    PolymerTest.clearBody();
    testBehavior = document.createElement('test-behavior');
    document.body.appendChild(testBehavior);
  });

  test('active', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kNone,
    };
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertFalse(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
  });

  test('user_recommended', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kUserPolicyRecommended,
      policyValue: 'bar',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertTrue(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('device_recommended', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kDevicePolicyRecommended,
      policyValue: 'bar',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertTrue(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('user_enforced', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kUserPolicyEnforced,
      policyValue: 'foo',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertTrue(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('device_enforced', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kDevicePolicyEnforced,
      policyValue: 'foo',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertTrue(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('extension_controlled', function() {
    const property = {
      activeValue: 'foo',
      policySource: PolicySource.kActiveExtension,
    };
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertTrue(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));

    assertEquals(
        CrPolicyIndicatorType.EXTENSION,
        testBehavior.getPolicyIndicatorType(property));
  });
});
