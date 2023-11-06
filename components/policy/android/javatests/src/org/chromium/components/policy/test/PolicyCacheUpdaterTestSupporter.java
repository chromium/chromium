// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;

import org.chromium.components.policy.PolicyCache;
import org.chromium.components.policy.PolicyCacheUpdater;

/**
 * Native unit test helper class for {@link PolicyCacheUpdater}
 *
 * It's used by the unit test in
 * //components/policy/core/browser/android/policy_cache_updater_android_unittest.cc
 *
 */
@JNINamespace("policy::android")
public class PolicyCacheUpdaterTestSupporter {
    @CalledByNative
    private PolicyCacheUpdaterTestSupporter() {}

    /** Checks value for {@code policy} is not cached. */
    @CalledByNative
    private void verifyIntPolicyNotCached(String policy) {
        PolicyCache policyCache = PolicyCache.get();
        policyCache.setReadableForTesting(true);
        Assert.assertNull(policyCache.getIntValue(policy));
    }

    /**
     * Checks value for {@code policy} is cached and the corresponding value is {@code
     * expectedValue}.
     */
    @CalledByNative
    private void verifyIntPolicyHasValue(String policy, int expectedValue) {
        PolicyCache policyCache = PolicyCache.get();
        policyCache.setReadableForTesting(true);
        Integer actualValue = policyCache.getIntValue(policy);
        Assert.assertNotNull(actualValue);
        Assert.assertEquals(expectedValue, actualValue.intValue());
    }

    /** Deletes all entries from the policy cache. */
    @CalledByNative
    private void resetPolicyCache() {
        PolicyCache.get().reset();
    }
}
