// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.policy;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeClassQualifiedName;
import org.jni_zero.NativeMethods;

/** Wrapper of the native PolicyMap class in the Java. */
@JNINamespace("policy::android")
public class PolicyMap {
    private long mNativePolicyMap;

    /**
     * Returns the value of integer policy.
     * @param policy The name of policy.
     */
    public Integer getIntValue(String policy) {
        // Return type of native getIntValue doesn't support nullable value, check if policy exist
        // first.
        if (!PolicyMapJni.get().hasValue(mNativePolicyMap, PolicyMap.this, policy)) {
            return null;
        }
        return PolicyMapJni.get().getIntValue(mNativePolicyMap, PolicyMap.this, policy);
    }

    /**
     * Returns the value of boolean policy.
     * @param policy The name of policy.
     */
    public Boolean getBooleanValue(String policy) {
        // Return type of native getIntValue doesn't support nullable value, check if policy exist
        // first.
        if (!PolicyMapJni.get().hasValue(mNativePolicyMap, PolicyMap.this, policy)) {
            return null;
        }
        return PolicyMapJni.get().getBooleanValue(mNativePolicyMap, PolicyMap.this, policy);
    }

    /**
     * Returns the value of string policy.
     * @param policy The name of policy.
     */
    public String getStringValue(String policy) {
        return PolicyMapJni.get().getStringValue(mNativePolicyMap, PolicyMap.this, policy);
    }

    /**
     * Returns tha JSON string of list policy.
     * @param policy The name of policy.
     */
    public String getListValueAsString(String policy) {
        return PolicyMapJni.get().getListValue(mNativePolicyMap, PolicyMap.this, policy);
    }

    /**
     * Returns tha JSON string of dictionary policy.
     * @param policy The name of policy.
     */
    public String getDictValueAsString(String policy) {
        return PolicyMapJni.get().getDictValue(mNativePolicyMap, PolicyMap.this, policy);
    }

    public boolean isEqual(PolicyMap other) {
        if (this == other) return true;
        return PolicyMapJni.get().equals(mNativePolicyMap, PolicyMap.this, other.mNativePolicyMap);
    }

    @CalledByNative
    private PolicyMap(long nativePolicyMap) {
        mNativePolicyMap = nativePolicyMap;
    }

    @NativeMethods
    public interface Natives {
        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean hasValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        int getIntValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean getBooleanValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        String getStringValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        String getListValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        String getDictValue(long nativePolicyMap, PolicyMap caller, String policy);

        @NativeClassQualifiedName("PolicyMapAndroid")
        boolean equals(long nativePolicyMap, PolicyMap caller, long nativeOtherPolicyMap);
    }
}
