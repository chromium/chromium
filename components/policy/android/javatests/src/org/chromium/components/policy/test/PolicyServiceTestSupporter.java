// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy.test;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.times;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.junit.Assert;
import org.mockito.Mockito;

import org.chromium.components.policy.PolicyMap;
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
    private List<Boolean> mPolicyUpdated = new ArrayList<>();

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
        mPolicyUpdated.add(false);
        return mObservers.size() - 1;
    }

    @CalledByNative
    private void removeObserver(int index) {
        mPolicyService.removeObserver(mObservers.get(index));
    }

    @CalledByNative
    private void verifyInitializationEvent(int index, int cnt) {
        Mockito.verify(mObservers.get(index), times(cnt)).onPolicyServiceInitialized();
    }

    @CalledByNative
    private void verifyPolicyUpdatedEvent(int index, int cnt) {
        Mockito.verify(mObservers.get(index), times(cnt))
                .onPolicyUpdated(any(PolicyMap.class), any(PolicyMap.class));
    }

    @CalledByNative
    private void setupPolicyUpdatedEventWithValues(
            int index, PolicyMap expectPrevious, PolicyMap expectCurrent) {
        // The native PolicyMapAndroid instance is only available inside the
        // PolicyService.Observer.onPolicUpdated() function. Hence we have to setup the argument
        // expectation before the event being triggered.
        mPolicyUpdated.set(index, false);
        Mockito.doAnswer(invocation -> mPolicyUpdated.set(index, true))
                .when(mObservers.get(index))
                .onPolicyUpdated(
                        argThat(actualPrevious -> expectPrevious.isEqual(actualPrevious)),
                        argThat(actualCurrent -> expectCurrent.isEqual(actualCurrent)));
    }

    @CalledByNative
    private void verifyPolicyUpdatedEventWithValues(int index, int cnt) {
        Assert.assertTrue(mPolicyUpdated.get(index));
        verifyPolicyUpdatedEvent(index, cnt);
    }

    @CalledByNative
    private void verifyNoMoreInteractions() {
        for (PolicyService.Observer observer : mObservers) {
            Mockito.verifyNoMoreInteractions(observer);
        }
    }
}
