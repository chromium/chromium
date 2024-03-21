// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill.payments;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.url.GURL;

import java.util.Objects;

/**
 * Data model for bank accounts used for facilitated payments.
 *
 * <p>This is the Java version of the C++ data model for bank account. An object of this type will
 * be created from the C++ side via JNI.
 */
@JNINamespace("autofill")
public class BankAccount extends PaymentInstrument {
    private final String mBankName;
    private final String mAccountNumberSuffix;
    private final @AccountType int mAccountType;

    private BankAccount(
            long instrumentId,
            String nickname,
            GURL displayIconUrl,
            @PaymentRail int[] supportedPaymentRails,
            String bankName,
            String accountNumberSuffix,
            @AccountType int accountType) {
        super(instrumentId, nickname, displayIconUrl, supportedPaymentRails);
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
                                .build())
                .setBankName(bankName)
                .setAccountNumberSuffix(accountNumberSuffix)
                .setAccountType(accountType)
                .build();
    }

    /** Returns the bank name for the bank account. */
    @CalledByNative
    public String getBankName() {
        return mBankName;
    }

    /** Returns the account number suffix that can be used to identify the bank account. */
    @CalledByNative
    public String getAccountNumberSuffix() {
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

    /** Builder for {@link BankAccount}. */
    public static final class Builder {
        private String mBankName;
        private String mAccountNumberSuffix;
        private PaymentInstrument mPaymentInstrument;
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
                    mPaymentInstrument.getInstrumentId(),
                    mPaymentInstrument.getNickname(),
                    mPaymentInstrument.getDisplayIconUrl(),
                    mPaymentInstrument.getSupportedPaymentRails(),
                    mBankName,
                    mAccountNumberSuffix,
                    mAccountType);
        }
    }
}
