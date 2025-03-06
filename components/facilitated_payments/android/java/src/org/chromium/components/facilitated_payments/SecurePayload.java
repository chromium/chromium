// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.facilitated_payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;
import java.util.List;

/**
 * Class containing the action token and the decrypted secure data returned from payments backend.
 * Both are used to trigger a UI flow within Google Play Services.
 */
@JNINamespace("payments::facilitated")
@NullMarked
class SecurePayload {
    private final byte[] mActionToken;
    private final List<SecureData> mSecureData;

    private SecurePayload(byte[] actionToken, @JniType("std::vector") Object[] secureData) {
        this.mActionToken = actionToken;
        this.mSecureData = (List<SecureData>) (List<?>) Arrays.asList(secureData);
    }

    /**
     * Creates a SecurePayload object.
     *
     * @param actionToken the action token used to trigger Google play services.
     * @param secureData decrypted secure data passed along with the action token.
     * @return null if either of the params are null
     */
    @CalledByNative
    public static @Nullable SecurePayload create(
            byte[] actionToken, @JniType("std::vector") Object[] secureData) {
        if (actionToken == null || secureData == null) {
            return null;
        }
        return new SecurePayload(actionToken, secureData);
    }

    /** Returns an action token that can be used to trigger a UI flow in Google Play Services. */
    public byte[] getActionToken() {
        return mActionToken;
    }

    /**
     * Returns a list of {@link SecureData} that can be passed in conjunction to the action token
     * while triggering a UI flow in Google Play Services.
     */
    public List<SecureData> getSecureData() {
        return mSecureData;
    }
}
