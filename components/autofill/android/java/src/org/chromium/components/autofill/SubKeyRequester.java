// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import android.text.format.DateUtils;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/** A class used handle SubKey requests. */
@JNINamespace("autofill")
public class SubKeyRequester {
    /** Callback for subKeys request. */
    public interface GetSubKeysRequestDelegate {
        /**
         * Called when the subkeys are received successfully.
         * Here the subkeys are admin areas.
         *
         * @param subKeysCodes The subkeys' codes.
         * @param subKeysNames The subkeys' names.
         */
        @CalledByNative("GetSubKeysRequestDelegate")
        void onSubKeysReceived(String[] subKeysCodes, String[] subKeysNames);
    }

    private static int sRequestTimeoutSeconds = 5;
    private final long mNativePtr;

    @CalledByNative
    private SubKeyRequester(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /**
     * Starts loading the sub-key request rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForSubKeys(String regionCode) {
        ThreadUtils.assertOnUiThread();
        SubKeyRequesterJni.get().loadRulesForSubKeys(mNativePtr, regionCode);
    }

    /**
     * Starts requesting the subkeys for the specified {@code regionCode}, if the rules
     * associated with the {@code regionCode} are done loading. Otherwise sets up the callback to
     * start loading the subkeys when the rules are loaded. The received subkeys will be sent
     * to the {@code delegate}. If the subkeys are not received in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param regionCode The code of the region for which to load the subkeys.
     * @param delegate The object requesting the subkeys.
     */
    public void getRegionSubKeys(String regionCode, GetSubKeysRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        SubKeyRequesterJni.get()
                .startRegionSubKeysRequest(
                        mNativePtr, regionCode, sRequestTimeoutSeconds, delegate);
    }

    /** Cancels the pending subkeys request. */
    public void cancelPendingGetSubKeys() {
        ThreadUtils.assertOnUiThread();
        SubKeyRequesterJni.get().cancelPendingGetSubKeys(mNativePtr);
    }

    public static void setRequestTimeoutForTesting(int timeout) {
        var oldValue = sRequestTimeoutSeconds;
        sRequestTimeoutSeconds = timeout;
        ResettersForTesting.register(() -> sRequestTimeoutSeconds = oldValue);
    }

    /** @return The sub-key request timeout in milliseconds. */
    public static long getRequestTimeoutMS() {
        return DateUtils.SECOND_IN_MILLIS * sRequestTimeoutSeconds;
    }

    @NativeMethods
    interface Natives {
        void loadRulesForSubKeys(long nativeSubKeyRequester, String regionCode);

        void startRegionSubKeysRequest(
                long nativeSubKeyRequester,
                String regionCode,
                int timeoutSeconds,
                GetSubKeysRequestDelegate delegate);

        void cancelPendingGetSubKeys(long nativeSubKeyRequester);
    }
}
