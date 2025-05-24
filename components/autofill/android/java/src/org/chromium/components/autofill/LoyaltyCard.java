// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

import java.util.List;

/** Represents a loyalty card coming from the the Google Wallet. */
@JNINamespace("autofill")
@NullMarked
public class LoyaltyCard {
    private final String mLoyaltyCardId;
    private final String mMerchantName;
    private final String mProgramName;
    private final GURL mProgramLogo;
    private final String mLoyaltyCardNumber;
    private final List<GURL> mMerchantDomains;

    @CalledByNative
    public LoyaltyCard(
            @JniType("std::string") String loyaltyCardId,
            @JniType("std::string") String merchantName,
            @JniType("std::string") String programName,
            @JniType("GURL") GURL programLogo,
            @JniType("std::string") String loyaltyCardNumber,
            @JniType("std::vector<GURL>") List<GURL> merchantDomains) {
        mLoyaltyCardId = loyaltyCardId;
        mMerchantName = merchantName;
        mProgramName = programName;
        mProgramLogo = programLogo;
        mLoyaltyCardNumber = loyaltyCardNumber;
        mMerchantDomains = merchantDomains;
    }

    public String getLoyaltyCardId() {
        return mLoyaltyCardId;
    }

    public String getMerchantName() {
        return mMerchantName;
    }

    public String getProgramName() {
        return mProgramName;
    }

    public GURL getProgramLogo() {
        return mProgramLogo;
    }

    public String getLoyaltyCardNumber() {
        return mLoyaltyCardNumber;
    }

    public List<GURL> getMerchantDomains() {
        return mMerchantDomains;
    }
}
