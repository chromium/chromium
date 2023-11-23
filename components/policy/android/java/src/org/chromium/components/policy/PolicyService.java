// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;

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
    private static final String TAG = "PolicyService";

    private long mNativePolicyService;
    private final ObserverList<Observer> mObservers = new ObserverList<Observer>();

    /**
     * Observer interface for observing PolicyService change for Chrome policy
     * domain.
     *
     * The default method below may increase method count with Desugar. If there
     * are more than 10+ observer implementations, please consider use
     * EmptyObserver instead for default behavior.
     */
    public interface Observer {
        /**
         * Invoked when Chrome policy is modified. The native class of both
         * |previous| and |current| become invalid once the method
         * returns. Do not use their references outside the method.
         * @param previous PolicyMap contains values before the update.
         * @param current PolicyMap contains values after the update.
         */
        default void onPolicyUpdated(PolicyMap previous, PolicyMap current) {}

        /**
         * Invoked when Chome policy domain is initialized. Observer must be
         * added before the naitve PolicyService initialization being finished.
         * Use {@link #isInitializationComplete} to check the initialization
         * state before listening to this event.
         */
        default void onPolicyServiceInitialized() {}
    }

    /**
     * @param observer The {@link Observer} to be notified for Chrome policy
     * update.
     */
    public void addObserver(Observer observer) {
        if (mObservers.isEmpty()) {
            PolicyServiceJni.get().addObserver(mNativePolicyService, PolicyService.this);
        }
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The {@link Observer} to no longer be notified for Chrome
     * policy update.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
        if (mObservers.isEmpty()) {
            PolicyServiceJni.get().removeObserver(mNativePolicyService, PolicyService.this);
        }
    }

    /** Returns true if Chrome policy domain has been initialized. */
    public boolean isInitializationComplete() {
        return PolicyServiceJni.get()
                .isInitializationComplete(mNativePolicyService, PolicyService.this);
    }

    /** Returns {@link PolicyMap} that contains all Chrome policies. */
    public PolicyMap getPolicies() {
        return PolicyServiceJni.get().getPolicies(mNativePolicyService, PolicyService.this);
    }

    /** Pass the onPolicyServiceInitialized event to the |mObservers|. */
    @CalledByNative
    private void onPolicyServiceInitialized() {
        Log.i(TAG, "#onPolicyServiceInitialized()");
        for (Observer observer : mObservers) {
            observer.onPolicyServiceInitialized();
        }
    }

    /** Pass the onPolicyUpdated event to the |mObservers|. */
    @CalledByNative
    private void onPolicyUpdated(PolicyMap previous, PolicyMap current) {
        for (Observer observer : mObservers) {
            observer.onPolicyUpdated(previous, current);
        }
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
