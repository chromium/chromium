// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

import org.chromium.base.annotations.CalledByNative;

/**
 * A profile, similar to the one used by the PersonalDataManager.
 */
public class FastCheckoutAutofillProfile {
    private final String mGUID;
    private final String mOrigin;
    private final boolean mIsLocal;
    private final String mHonorificPrefix;
    private final String mFullName;
    private final String mCompanyName;
    private final String mStreetAddress;
    private final String mRegion;
    private final String mLocality;
    private final String mDependentLocality;
    private final String mPostalCode;
    private final String mSortingCode;
    private final String mCountryCode;
    private final String mCountryName;
    private final String mPhoneNumber;
    private final String mEmailAddress;
    private final String mLanguageCode;

    @CalledByNative
    public FastCheckoutAutofillProfile(String guid, String origin, boolean isLocal,
            String honorificPrefix, String fullName, String companyName, String streetAddress,
            String region, String locality, String dependentLocality, String postalCode,
            String sortingCode, String countryCode, String countryName, String phoneNumber,
            String emailAddress, String languageCode) {
        mGUID = guid;
        mOrigin = origin;
        mIsLocal = isLocal;
        mHonorificPrefix = honorificPrefix;
        mFullName = fullName;
        mCompanyName = companyName;
        mStreetAddress = streetAddress;
        mRegion = region;
        mLocality = locality;
        mDependentLocality = dependentLocality;
        mPostalCode = postalCode;
        mSortingCode = sortingCode;
        mCountryCode = countryCode;
        mCountryName = countryName;
        mPhoneNumber = phoneNumber;
        mEmailAddress = emailAddress;
        mLanguageCode = languageCode;
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

    public String getHonorificPrefix() {
        return mHonorificPrefix;
    }

    public String getFullName() {
        return mFullName;
    }

    public String getCompanyName() {
        return mCompanyName;
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

    public String getSortingCode() {
        return mSortingCode;
    }

    public String getCountryCode() {
        return mCountryCode;
    }

    public String getCountryName() {
        return mCountryName;
    }

    public String getPhoneNumber() {
        return mPhoneNumber;
    }

    public String getEmailAddress() {
        return mEmailAddress;
    }

    public String getLanguageCode() {
        return mLanguageCode;
    }
}
