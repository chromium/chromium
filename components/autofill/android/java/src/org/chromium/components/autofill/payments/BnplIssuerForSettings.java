// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Autofill BNPL issuer for settings information. */
@JNINamespace("autofill")
@NullMarked
public class BnplIssuerForSettings {
    private final String mIssuerId;
    private final long mInstrumentId;
    private final String mDisplayName;

    /**
     * Constructs a new BnplIssuerForSettings.
     *
     * @param issuerId The ID of the BNPL issuer.
     * @param instrumentId The payment instrument ID of the issuer.
     * @param displayName The name of the issuer to be displayed.
     */
    @CalledByNative
    public BnplIssuerForSettings(
            @JniType("std::string") String issuerId,
            long instrumentId,
            @JniType("std::u16string") String displayName) {
        mIssuerId = issuerId;
        mInstrumentId = instrumentId;
        mDisplayName = displayName;
    }

    /** Returns the ID of the BNPL issuer. */
    public String getIssuerId() {
        return mIssuerId;
    }

    /** Returns the payment instrument ID of the issuer. */
    public long getInstrumentId() {
        return mInstrumentId;
    }

    /** Returns the name of the issuer to be displayed. */
    public String getDisplayName() {
        return mDisplayName;
    }
}
