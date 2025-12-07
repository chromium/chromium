// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import androidx.annotation.DrawableRes;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;

/** Autofill BNPL issuer for settings information. */
@JNINamespace("autofill")
@NullMarked
public class BnplIssuerForSettings {
    private final @DrawableRes int mIconId;
    private final long mInstrumentId;
    private final String mDisplayName;

    /**
     * Constructs a new BnplIssuerForSettings.
     *
     * @param iconId The resource ID of the issuer's icon.
     * @param instrumentId The payment instrument ID of the issuer.
     * @param displayName The name of the issuer to be displayed.
     */
    @CalledByNative
    public BnplIssuerForSettings(
            @DrawableRes int iconId,
            long instrumentId,
            @JniType("std::u16string") String displayName) {
        mIconId = iconId;
        mInstrumentId = instrumentId;
        mDisplayName = displayName;
    }

    /** Returns the resource ID of the issuer's icon. */
    public @DrawableRes int getIconId() {
        return mIconId;
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
