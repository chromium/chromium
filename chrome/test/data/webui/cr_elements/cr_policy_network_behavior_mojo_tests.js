// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */
suite('CrPolicyNetworkBehaviorMojo', function() {
  suiteSetup(function() {
    Polymer({
      is: 'test-behavior',

      behaviors: [CrPolicyNetworkBehaviorMojo],
    });
  });

  let mojom;
  let testBehavior;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;
    PolymerTest.clearBody();
    testBehavior = document.createElement('test-behavior');
    document.body.appendChild(testBehavior);
  });

  test('active', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kNone,
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
      policySource: mojom.PolicySource.kUserPolicyRecommended,
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
      policySource: mojom.PolicySource.kDevicePolicyRecommended,
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
      policySource: mojom.PolicySource.kUserPolicyEnforced,
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
      policySource: mojom.PolicySource.kDevicePolicyEnforced,
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
      policySource: mojom.PolicySource.kActiveExtension,
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
