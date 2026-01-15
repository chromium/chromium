// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Data model for ewallet accounts used for facilitated payments.
 *
 * <p>This is the Java version of the C++ data model for ewallet account. An object of this type
 * will be created from the C++ side via JNI.
 */
@JNINamespace("autofill")
@NullMarked
public class Ewallet extends PaymentInstrument {
    private final String mEwalletName;
    private final String mAccountDisplayName;

    private Ewallet(
            long instrumentId,
            @Nullable String nickname,
            @Nullable GURL displayIconUrl,
            @PaymentRail int[] supportedPaymentRails,
            boolean isFidoEnrolled,
            String ewalletName,
            String accountDisplayName) {
        super(instrumentId, nickname, displayIconUrl, supportedPaymentRails, isFidoEnrolled);
        mEwalletName = ewalletName;
        mAccountDisplayName = accountDisplayName;
    }

    @CalledByNative
    static Ewallet create(
            long instrumentId,
            @JniType("std::u16string") String nickname,
            // Cannot use @JniType("GURL") here, because this class handles null GURL in a
            // different way than the GURL's FromJniType()/ToJniType().
            GURL displayIconUrl,
            @JniType("std::vector<int32_t>") @PaymentRail int[] supportedPaymentRails,
            boolean isFidoEnrolled,
            @JniType("std::u16string") String ewalletName,
            @JniType("std::u16string") String accountDisplayName) {
        return new Ewallet.Builder()
                .setPaymentInstrument(
                        new PaymentInstrument.Builder()
                                .setInstrumentId(instrumentId)
                                .setNickname(nickname)
                                .setDisplayIconUrl(displayIconUrl)
                                .setSupportedPaymentRails(supportedPaymentRails)
                                .setIsFidoEnrolled(isFidoEnrolled)
                                .build())
                .setEwalletName(ewalletName)
                .setAccountDisplayName(accountDisplayName)
                .build();
    }

    /** Returns the name of the eWallet provider. */
    @CalledByNative
    public @JniType("std::u16string") String getEwalletName() {
        return mEwalletName;
    }

    /** Returns the account display name that can be used to identify the eWallet account. */
    @CalledByNative
    public @JniType("std::u16string") String getAccountDisplayName() {
        return mAccountDisplayName;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null) {
            return false;
        }

        if (!(obj instanceof Ewallet)) {
            return false;
        }

        Ewallet other = (Ewallet) obj;
        return super.equals(obj)
                && Objects.equals(mEwalletName, other.getEwalletName())
                && Objects.equals(mAccountDisplayName, other.getAccountDisplayName());
    }

    /** Builder for {@link Ewallet}. */
    public static final class Builder {
        private @Nullable String mEwalletName;
        private @Nullable String mAccountDisplayName;
        private @Nullable PaymentInstrument mPaymentInstrument;

        /** Set the eWallet name on the Ewallet. */
        public Builder setEwalletName(String ewalletName) {
            mEwalletName = ewalletName;
            return this;
        }

        /** Set the account display name on the Ewallet. */
        public Builder setAccountDisplayName(String accountDisplayName) {
            mAccountDisplayName = accountDisplayName;
            return this;
        }

        /** Set the payment instrument on the BankAccount. */
        public Builder setPaymentInstrument(PaymentInstrument paymentInstrument) {
            mPaymentInstrument = paymentInstrument;
            return this;
        }

        public Ewallet build() {
            // The asserts are only checked in tests and in some Canary builds but not in
            // production. This is intended as we don't want to crash Chrome production for the
            // below checks and instead let the clients who call the getters on the Ewallet
            // decide on whether to crash or not.
            assert mEwalletName != null && !mEwalletName.isEmpty()
                    : "Ewallet name cannot be null or empty.";
            assert mAccountDisplayName != null && !mAccountDisplayName.isEmpty()
                    : "Account display name cannot be null or empty.";
            return new Ewallet(
                    assumeNonNull(mPaymentInstrument).getInstrumentId(),
                    mPaymentInstrument.getNickname(),
                    mPaymentInstrument.getDisplayIconUrl(),
                    mPaymentInstrument.getSupportedPaymentRails(),
                    mPaymentInstrument.getIsFidoEnrolled(),
                    mEwalletName,
                    mAccountDisplayName);
        }
    }
}
