// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeClassQualifiedName;
import org.chromium.base.annotations.NativeMethods;

import java.util.HashSet;
import java.util.Set;

/**
 * Wrapper of the native PolicyService class in the Java layer.
 *
 * It only supports the Chrome policy domain but not any extension policy. More
 * documentation can be found in
 * //components/policy/core/common/policy_service.h
 *
 * Native pointer is owned by the C++ instance.
 *
 * The class only provides a subset of native class features for now, other
 * functions will be added once needed.
 */
@JNINamespace("policy::android")
public class PolicyService {
    private long mNativePolicyService;
    private final Set<Observer> mObservers = new HashSet<Observer>();

    /**
     * Observer interface for observing PolicyService change for Chrome policy
     * domain.
     */
    public interface Observer {
        /**
         * Invoked when Chome policy domain is initialized. Observer must be
         * added before the naitve PolicyService initialization being finished.
         * Use {@link #isInitializationComplete} to check the initialization
         * state before listening to this event.
         */
        void onPolicyServiceInitialized();
    }

    /**
     * @param observer The {@link Observer} to be notified for Chrome policy
     * update.
     */
    public void addObserver(Observer observer) {
        if (mObservers.isEmpty()) {
            PolicyServiceJni.get().addObserver(mNativePolicyService, PolicyService.this);
        }
        mObservers.add(observer);
    }

    /**
     * @param observer The {@link Observer} to no longer be notified for Chrome
     * policy update.
     */
    public void removeObserver(Observer observer) {
        mObservers.remove(observer);
        if (mObservers.isEmpty()) {
            PolicyServiceJni.get().removeObserver(mNativePolicyService, PolicyService.this);
        }
    }

    /**
     * Returns true if Chrome policy domain has been initialized.
     */
    public boolean isInitializationComplete() {
        return PolicyServiceJni.get().isInitializationComplete(
                mNativePolicyService, PolicyService.this);
    }

    /**
     * Returns {@link PolicyMap} that contains all Chrome policies.
     */
    public PolicyMap getPolicies() {
        return PolicyServiceJni.get().getPolicies(mNativePolicyService, PolicyService.this);
    }

    /**
     * Pass the onPolicyServiceInitialized event to the |mObservers|.
     */
    @CalledByNative
    private void onPolicyServiceInitialized() {
        CollectionUtil.forEach(mObservers, observer -> observer.onPolicyServiceInitialized());
    }

    @CalledByNative
    private PolicyService(long nativePolicyService) {
        mNativePolicyService = nativePolicyService;
    }

    @NativeMethods
    public interface Natives {
        @NativeClassQualifiedName("PolicyServiceAndroid")
        void addObserver(long nativePolicyService, PolicyService caller);
        @NativeClassQualifiedName("PolicyServiceAndroid")
        void removeObserver(long nativePolicyService, PolicyService caller);
        @NativeClassQualifiedName("PolicyServiceAndroid")
        boolean isInitializationComplete(long nativePolicyService, PolicyService caller);
        @NativeClassQualifiedName("PolicyServiceAndroid")
        PolicyMap getPolicies(long nativePolicyService, PolicyService caller);
    }
}
