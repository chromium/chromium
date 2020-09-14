// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test;

import static org.mockito.Mockito.times;

import org.junit.Assert;
import org.mockito.Mockito;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.components.policy.PolicyService;

import java.util.ArrayList;
import java.util.List;

/**
 * Native unit test helper for class {@link PolicyService}
 *
 * It's used by native unit tests in:
 * components/policy/android/core/common/policy_service_android_unittest.cc
 */
@JNINamespace("policy::android")
public class PolicyServiceTestSupporter {
    private List<PolicyService.Observer> mObservers = new ArrayList<>();

    PolicyService mPolicyService;

    @CalledByNative
    private PolicyServiceTestSupporter(PolicyService policyService) {
        mPolicyService = policyService;
    }

    @CalledByNative
    private void verifyIsInitalizationComplete(boolean expected) {
        Assert.assertEquals(expected, mPolicyService.isInitializationComplete());
    }

    @CalledByNative
    private int addObserver() {
        mObservers.add(Mockito.mock(PolicyService.Observer.class));
        mPolicyService.addObserver(mObservers.get(mObservers.size() - 1));
        return mObservers.size() - 1;
    }

    @CalledByNative
    private void removeObserver(int index) {
        mPolicyService.removeObserver(mObservers.get(index));
    }

    @CalledByNative
    private void verifyObserverCalled(int index, int cnt) {
        Mockito.verify(mObservers.get(index), times(cnt)).onPolicyServiceInitialized();
    }

    @CalledByNative
    private void verifyNoMoreInteractions() {
        for (PolicyService.Observer observer : mObservers) {
            Mockito.verifyNoMoreInteractions(observer);
        }
    }
}
