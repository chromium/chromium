// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.url.GURL;

/**
 * A credit card, similar to the one used by the PersonalDataManager.
 */
public class FastCheckoutCreditCard {
    private final String mGUID;
    private final String mOrigin;
    private final boolean mIsLocal;
    private final boolean mIsCached;
    private final String mName;
    private final String mNumber;
    private final String mObfuscatedNumber;
    private final String mMonth;
    private final String mYear;
    private final String mBasicCardIssuerNetwork;
    private final String mIssuerIconString;
    private final String mBillingAddressId;
    private final String mServerId;
    private final long mInstrumentId;
    private final String mNickname;
    private final GURL mCardArtUrl;
    private final @VirtualCardEnrollmentState int mVirtualCardEnrollmentState;
    private final String mProductDescription;

    @CalledByNative
    public FastCheckoutCreditCard(String guid, String origin, boolean isLocal, boolean isCached,
            String name, String number, String obfuscatedNumber, String month, String year,
            String basicCardIssuerNetwork, String issuerIconString, String billingAddressId,
            String serverId, long instrumentId, String nickname, GURL cardArtUrl,
            @VirtualCardEnrollmentState int virtualCardEnrollmentState, String productDescription) {
        mGUID = guid;
        mOrigin = origin;
        mIsLocal = isLocal;
        mIsCached = isCached;
        mName = name;
        mNumber = number;
        mObfuscatedNumber = obfuscatedNumber;
        mMonth = month;
        mYear = year;
        mBasicCardIssuerNetwork = basicCardIssuerNetwork;
        mIssuerIconString = issuerIconString;
        mBillingAddressId = billingAddressId;
        mServerId = serverId;
        mInstrumentId = instrumentId;
        mNickname = nickname;
        mCardArtUrl = cardArtUrl;
        mVirtualCardEnrollmentState = virtualCardEnrollmentState;
        mProductDescription = productDescription;
    }

    public String getGUID() {
        return mGUID;
    }

    public String getOrigin() {
        return mOrigin;
    }

    public boolean getIsLocal() {
        return mIsLocal;
    }

    public boolean getIsCached() {
        return mIsCached;
    }

    public String getName() {
        return mName;
    }

    public String getNumber() {
        return mNumber;
    }

    public String getObfuscatedNumber() {
        return mObfuscatedNumber;
    }

    public String getMonth() {
        return mMonth;
    }

    public String getYear() {
        return mYear;
    }

    public String getBasicCardIssuerNetwork() {
        return mBasicCardIssuerNetwork;
    }

    public String getIssuerIconString() {
        return mIssuerIconString;
    }

    public String getBillingAddressId() {
        return mBillingAddressId;
    }

    public String getServerId() {
        return mServerId;
    }

    public long getInstrumentId() {
        return mInstrumentId;
    }

    public String getNickname() {
        return mNickname;
    }

    public GURL getCardArtUrl() {
        return mCardArtUrl;
    }

    public @VirtualCardEnrollmentState int getVirtualCardEnrollmentState() {
        return mVirtualCardEnrollmentState;
    }

    public String getProductDescription() {
        return mProductDescription;
    }
}
