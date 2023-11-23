// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;

/** A class used to normalize addresses. */
@JNINamespace("autofill")
public class AddressNormalizer {
    /** Callback for normalized addresses. */
    public interface NormalizedAddressRequestDelegate {
        /**
         * Called when the address has been successfully normalized.
         *
         * @param profile The profile with the normalized address.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onAddressNormalized(AutofillProfile profile);

        /**
         * Called when the address could not be normalized.
         *
         * @param profile The non normalized profile.
         */
        @CalledByNative("NormalizedAddressRequestDelegate")
        void onCouldNotNormalize(AutofillProfile profile);
    }

    private static int sRequestTimeoutSeconds = 5;
    private final long mNativePtr;

    @CalledByNative
    private AddressNormalizer(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /**
     * Starts loading the address validation rules for the specified {@code regionCode}.
     *
     * @param regionCode The code of the region for which to load the rules.
     */
    public void loadRulesForAddressNormalization(String regionCode) {
        ThreadUtils.assertOnUiThread();
        AddressNormalizerJni.get().loadRulesForAddressNormalization(mNativePtr, regionCode);
    }

    /**
     * Normalizes the address of the profile associated with the {@code guid} if the rules
     * associated with the profile's region are done loading. Otherwise sets up the callback to
     * start normalizing the address when the rules are loaded. The normalized profile will be sent
     * to the {@code delegate}. If the profile is not normalized in the specified
     * {@code sRequestTimeoutSeconds}, the {@code delegate} will be notified.
     *
     * @param profile The profile to normalize.
     * @param delegate The object requesting the normalization.
     */
    public void normalizeAddress(
            AutofillProfile profile, NormalizedAddressRequestDelegate delegate) {
        ThreadUtils.assertOnUiThread();
        AddressNormalizerJni.get()
                .startAddressNormalization(mNativePtr, profile, sRequestTimeoutSeconds, delegate);
    }

    public static void setRequestTimeoutForTesting(int timeout) {
        var oldValue = sRequestTimeoutSeconds;
        sRequestTimeoutSeconds = timeout;
        ResettersForTesting.register(() -> sRequestTimeoutSeconds = oldValue);
    }

    @NativeMethods
    interface Natives {
        void loadRulesForAddressNormalization(long nativeAddressNormalizerImpl, String regionCode);

        void startAddressNormalization(
                long nativeAddressNormalizerImpl,
                AutofillProfile profile,
                int timeoutSeconds,
                NormalizedAddressRequestDelegate delegate);
    }
}
