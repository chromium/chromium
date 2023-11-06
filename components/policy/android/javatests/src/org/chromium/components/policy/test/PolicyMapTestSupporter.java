// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;

import org.chromium.components.policy.PolicyMap;

/**
 * Naitve unit test helper for class {@link PolicyMap}.
 *
 * It's used by native unit tests in:
 * components/policy/core/common/android/policy_map_android_unittest.cc
 */
@JNINamespace("policy::android")
public class PolicyMapTestSupporter {
    PolicyMap mPolicyMap;

    @CalledByNative
    private PolicyMapTestSupporter(PolicyMap policyMap) {
        mPolicyMap = policyMap;
    }

    @CalledByNative
    private void verifyIntPolicy(String policyName, boolean hasValue, int expectedValue) {
        if (!hasValue) {
            Assert.assertNull(mPolicyMap.getIntValue(policyName));
            return;
        }
        Assert.assertEquals(expectedValue, mPolicyMap.getIntValue(policyName).intValue());
    }

    @CalledByNative
    private void verifyBooleanPolicy(String policyName, boolean hasValue, boolean expectedValue) {
        if (!hasValue) {
            Assert.assertNull(mPolicyMap.getBooleanValue(policyName));
            return;
        }
        Assert.assertEquals(expectedValue, mPolicyMap.getBooleanValue(policyName).booleanValue());
    }

    @CalledByNative
    private void verifyStringPolicy(String policyName, String expectedValue) {
        if (expectedValue == null) {
            Assert.assertNull(mPolicyMap.getStringValue(policyName));
            return;
        }
        Assert.assertEquals(expectedValue, mPolicyMap.getStringValue(policyName));
    }

    @CalledByNative
    private void verifyListPolicy(String policyName, String expectedValue) {
        if (expectedValue == null) {
            Assert.assertNull(mPolicyMap.getListValueAsString(policyName));
            return;
        }
        Assert.assertEquals(expectedValue, mPolicyMap.getListValueAsString(policyName));
    }

    @CalledByNative
    private void verifyDictPolicy(String policyName, String expectedValue) {
        if (expectedValue == null) {
            Assert.assertNull(mPolicyMap.getDictValueAsString(policyName));
            return;
        }
        Assert.assertEquals(expectedValue, mPolicyMap.getDictValueAsString(policyName));
    }
}
