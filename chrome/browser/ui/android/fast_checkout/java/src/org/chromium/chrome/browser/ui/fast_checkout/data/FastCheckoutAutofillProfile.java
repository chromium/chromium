// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

/**
 * A profile, similar to the one used by the PersonalDataManager.
 */
public class FastCheckoutAutofillProfile {
    private final String mGUID;
    private final String mFullName;
    private final String mStreetAddress;
    private final String mRegion;
    private final String mLocality;
    private final String mDependentLocality;
    private final String mPostalCode;
    private final String mCountryCode;
    private final String mPhoneNumber;
    private final String mEmailAddress;

    public FastCheckoutAutofillProfile(String guid, String fullName, String streetAddress,
            String region, String locality, String dependentLocality, String postalCode,
            String countryCode, String phoneNumber, String emailAddress) {
        mGUID = guid;
        mFullName = fullName;
        mStreetAddress = streetAddress;
        mRegion = region;
        mLocality = locality;
        mDependentLocality = dependentLocality;
        mPostalCode = postalCode;
        mCountryCode = countryCode;
        mPhoneNumber = phoneNumber;
        mEmailAddress = emailAddress;
    }

    public String getGUID() {
        return mGUID;
    }

    public String getFullName() {
        return mFullName;
    }

    public String getStreetAddress() {
        return mStreetAddress;
    }

    public String getRegion() {
        return mRegion;
    }

    public String getLocality() {
        return mLocality;
    }

    public String getDependentLocality() {
        return mDependentLocality;
    }

    public String getPostalCode() {
        return mPostalCode;
    }

    public String getCountryCode() {
        return mCountryCode;
    }

    public String getPhoneNumber() {
        return mPhoneNumber;
    }

    public String getEmailAddress() {
        return mEmailAddress;
    }
}
