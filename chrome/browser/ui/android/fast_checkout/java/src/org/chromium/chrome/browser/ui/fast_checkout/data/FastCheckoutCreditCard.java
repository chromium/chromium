// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

/**
 * A credit card, similar to the one used by the PersonalDataManager.
 */
public class FastCheckoutCreditCard {
    private final String mGUID;
    private final String mOrigin;
    private final String mName;
    private final String mNumber;
    private final String mObfuscatedNumber;
    private final String mMonth;
    private final String mYear;
    private final String mBasicCardIssuerNetwork;
    private final String mIssuerIconString;
    private final String mNickname;

    public FastCheckoutCreditCard(String guid, String origin, String name, String number,
            String obfuscatedNumber, String month, String year, String basicCardIssuerNetwork,
            String issuerIconString, String nickname) {
        mGUID = guid;
        mOrigin = origin;
        mName = name;
        mNumber = number;
        mObfuscatedNumber = obfuscatedNumber;
        mMonth = month;
        mYear = year;
        mBasicCardIssuerNetwork = basicCardIssuerNetwork;
        mIssuerIconString = issuerIconString;
        mNickname = nickname;
    }

    public String getGUID() {
        return mGUID;
    }

    public String getOrigin() {
        return mOrigin;
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

    public String getNickname() {
        return mNickname;
    }
}
