// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Wrapper of the native PolicyMap class in the Java. */
@JNINamespace("policy::android")
@NullMarked
public class PolicyMap {
    private final long mNativePolicyMap;

    /**
     * Returns the value of integer policy.
     * @param policy The name of policy.
     */
    public @Nullable Integer getIntValue(String policy) {
        // Return type of native getIntValue doesn't support nullable value, check if policy exist
        // first.
        if (!PolicyMapJni.get().hasValue(mNativePolicyMap, policy)) {
            return null;
        }
        return PolicyMapJni.get().getIntValue(mNativePolicyMap, policy);
    }

    /**
     * Returns the value of boolean policy.
     * @param policy The name of policy.
     */
    public @Nullable Boolean getBooleanValue(String policy) {
        // Return type of native getIntValue doesn't support nullable value, check if policy exist
        // first.
        if (!PolicyMapJni.get().hasValue(mNativePolicyMap, policy)) {
            return null;
        }
        return PolicyMapJni.get().getBooleanValue(mNativePolicyMap, policy);
    }

    /**
     * Returns the value of string policy.
     *
     * @param policy The name of policy.
     */
    public @Nullable String getStringValue(String policy) {
        return PolicyMapJni.get().getStringValue(mNativePolicyMap, policy);
    }

    /**
     * Returns tha JSON string of list policy.
     *
     * @param policy The name of policy.
     */
    public @Nullable String getListValueAsString(String policy) {
        return PolicyMapJni.get().getListValue(mNativePolicyMap, policy);
    }

    /**
     * Returns tha JSON string of dictionary policy.
     *
     * @param policy The name of policy.
     */
    public @Nullable String getDictValueAsString(String policy) {
        return PolicyMapJni.get().getDictValue(mNativePolicyMap, policy);
    }

    public boolean isEqual(PolicyMap other) {
        if (this == other) return true;
        return PolicyMapJni.get().equals(mNativePolicyMap, other.mNativePolicyMap);
    }

    @CalledByNative
    private PolicyMap(long nativePolicyMap) {
        mNativePolicyMap = nativePolicyMap;
    }

    @NativeMethods
    public interface Natives {
        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean hasValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        int getIntValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean getBooleanValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        @Nullable String getStringValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        @Nullable String getListValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        @Nullable String getDictValue(long nativePolicyMap, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean equals(long nativePolicyMap, long nativeOtherPolicyMap);
    }
}
