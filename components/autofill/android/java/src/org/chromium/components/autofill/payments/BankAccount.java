// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Data model for bank accounts used for facilitated payments.
 *
 * <p>This is the Java version of the C++ data model for bank account. An object of this type will
 * be created from the C++ side via JNI.
 */
@JNINamespace("autofill")
@NullMarked
public class BankAccount extends PaymentInstrument {
    private final @Nullable String mBankName;
    private final @Nullable String mAccountNumberSuffix;
    private final @AccountType int mAccountType;

    private BankAccount(
            long instrumentId,
            @Nullable String nickname,
            @Nullable GURL displayIconUrl,
            @PaymentRail int[] supportedPaymentRails,
            boolean isFidoEnrolled,
            @Nullable String bankName,
            @Nullable String accountNumberSuffix,
            @AccountType int accountType) {
        super(instrumentId, nickname, displayIconUrl, supportedPaymentRails, isFidoEnrolled);
        mBankName = bankName;
        mAccountNumberSuffix = accountNumberSuffix;
        mAccountType = accountType;
    }

    @CalledByNative
    static BankAccount create(
            long instrumentId,
            String nickname,
            GURL displayIconUrl,
            @PaymentRail int[] supportedPaymentRails,
            boolean isFidoEnrolled,
            String bankName,
            String accountNumberSuffix,
            @AccountType int accountType) {
        return new BankAccount.Builder()
                .setPaymentInstrument(
                        new PaymentInstrument.Builder()
                                .setInstrumentId(instrumentId)
                                .setNickname(nickname)
                                .setDisplayIconUrl(displayIconUrl)
                                .setSupportedPaymentRails(supportedPaymentRails)
                                .setIsFidoEnrolled(isFidoEnrolled)
                                .build())
                .setBankName(bankName)
                .setAccountNumberSuffix(accountNumberSuffix)
                .setAccountType(accountType)
                .build();
    }

    /** Returns the bank name for the bank account. */
    @CalledByNative
    public @Nullable String getBankName() {
        return mBankName;
    }

    /** Returns the account number suffix that can be used to identify the bank account. */
    @CalledByNative
    public @Nullable String getAccountNumberSuffix() {
        return mAccountNumberSuffix;
    }

    /** Returns the account type for the bank account. */
    @CalledByNative
    public @AccountType int getAccountType() {
        return mAccountType;
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == null) {
            return false;
        }

        if (!(obj instanceof BankAccount)) {
            return false;
        }

        BankAccount other = (BankAccount) obj;
        return super.equals(obj)
                && Objects.equals(mBankName, other.getBankName())
                && Objects.equals(mAccountNumberSuffix, other.getAccountNumberSuffix())
                && mAccountType == other.getAccountType();
    }

    /**
     * Returns the obfuscated account number with visible suffix like '••••12'. The bank account
     * number is displayed to the user in this format.
     */
    public @Nullable String getObfuscatedAccountNumber() {
        if (mAccountNumberSuffix == null || mAccountNumberSuffix.isEmpty()) {
            return null;
        }

        return "••••" + mAccountNumberSuffix;
    }

    /** Builder for {@link BankAccount}. */
    public static final class Builder {
        private @Nullable String mBankName;
        private @Nullable String mAccountNumberSuffix;
        private @Nullable PaymentInstrument mPaymentInstrument;
        private @AccountType int mAccountType;

        /** Set the bank name on the BankAccount. */
        public Builder setBankName(String bankName) {
            mBankName = bankName;
            return this;
        }

        /** Set the account number suffix on the BankAccount. */
        public Builder setAccountNumberSuffix(String accountNumberSuffix) {
            mAccountNumberSuffix = accountNumberSuffix;
            return this;
        }

        /** Set the payment instrument on the BankAccount. */
        public Builder setPaymentInstrument(PaymentInstrument paymentInstrument) {
            mPaymentInstrument = paymentInstrument;
            return this;
        }

        /** Set the account type on the BankAccount. */
        public Builder setAccountType(@AccountType int accountType) {
            mAccountType = accountType;
            return this;
        }

        public BankAccount build() {
            return new BankAccount(
                    assumeNonNull(mPaymentInstrument).getInstrumentId(),
                    mPaymentInstrument.getNickname(),
                    mPaymentInstrument.getDisplayIconUrl(),
                    mPaymentInstrument.getSupportedPaymentRails(),
                    mPaymentInstrument.getIsFidoEnrolled(),
                    assumeNonNull(mBankName),
                    assumeNonNull(mAccountNumberSuffix),
                    mAccountType);
        }
    }
}
