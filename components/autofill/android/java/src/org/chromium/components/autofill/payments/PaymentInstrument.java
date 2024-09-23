// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.Objects;

/** Base data model for any form of payment (FOP) synced via Google Payments. */
@JNINamespace("autofill")
public class PaymentInstrument {
    private final long mInstrumentId;
    private final String mNickname;
    private final GURL mDisplayIconUrl;
    private final @PaymentRail int[] mSupportedPaymentRails;

    protected PaymentInstrument(
            long instrumentId,
            String nickname,
            GURL displayIconUrl,
            @PaymentRail int[] supportedPaymentRails) {
        mInstrumentId = instrumentId;
        mNickname = nickname;
        mDisplayIconUrl = displayIconUrl;
        mSupportedPaymentRails = supportedPaymentRails;
    }

    /** Returns the instrument id for the payment instrument. */
    @CalledByNative
    public long getInstrumentId() {
        return mInstrumentId;
    }

    /** Returns the user-assigned nickname for the payment instrument, if one exists. */
    @CalledByNative
    public String getNickname() {
        return mNickname;
    }

    /**
     * Returns the URL to download the icon to be displayed for the payment instrument, if one
     * exists.
     */
    @CalledByNative
    public GURL getDisplayIconUrl() {
        return mDisplayIconUrl;
    }

    /** Returns an array of {@link PaymentRail} that are supported by the payment instrument. */
    public @PaymentRail int[] getSupportedPaymentRails() {
        return mSupportedPaymentRails;
    }

    /**
     * Returns true if the {@code paymentRail} is contained within the list of supported payment
     * rails for the payment instrument.
     */
    public boolean isSupported(@PaymentRail int paymentRail) {
        for (int i = 0; i < mSupportedPaymentRails.length; i++) {
            if (mSupportedPaymentRails[i] == paymentRail) {
                return true;
            }
        }
        return false;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null) {
            return false;
        }

        if (!(obj instanceof PaymentInstrument)) {
            return false;
        }

        PaymentInstrument other = (PaymentInstrument) obj;
        return mInstrumentId == other.getInstrumentId()
                && Objects.equals(mNickname, other.getNickname())
                && Objects.equals(mDisplayIconUrl, other.getDisplayIconUrl())
                && Arrays.equals(mSupportedPaymentRails, other.getSupportedPaymentRails());
    }

    /** Builder for {@link PaymentInstrument}. */
    public static class Builder {
        private long mInstrumentId;
        private String mNickname;
        private GURL mDisplayIconUrl;
        private @PaymentRail int[] mSupportedPaymentRails;

        /** Set the instrument id on the PaymentInstrument. */
        public Builder setInstrumentId(long instrumentId) {
            mInstrumentId = instrumentId;
            return this;
        }

        /** Set the nickname on the PaymentInstrument. */
        public Builder setNickname(String nickname) {
            mNickname = nickname;
            return this;
        }

        /** Set the display icon url on the PaymentInstrument. */
        public Builder setDisplayIconUrl(GURL displayIconUrl) {
            mDisplayIconUrl = displayIconUrl;
            return this;
        }

        /** Set the payment rails supported for the PaymentInstrument. */
        public Builder setSupportedPaymentRails(@PaymentRail int[] paymentRails) {
            mSupportedPaymentRails = paymentRails;
            return this;
        }

        public PaymentInstrument build() {
            // The asserts are only checked in tests and in some Canary builds but not in
            // production. This is intended as we don't want to crash Chrome production for the
            // below checks and instead let the clients who call the getters on the
            // PaymentInstrument decide on whether to crash or not.
            assert mInstrumentId != 0 : "InstrumentId must be set for the payment instrument.";
            assert mSupportedPaymentRails != null && mSupportedPaymentRails.length != 0
                    : "Payment instrument must support at least one payment rail.";
            return new PaymentInstrument(
                    mInstrumentId, mNickname, mDisplayIconUrl, mSupportedPaymentRails);
        }
    }
}
