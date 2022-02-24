// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * A profile, similar to the one used by the PersonalDataManager.
 */
@JNINamespace("autofill_assistant")
public class AssistantAutofillProfile {
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
    private final String mPhoneNumber;
    private final String mEmailAddress;
    private final String mLanguageCode;

    @CalledByNative
    public AssistantAutofillProfile(String guid, String origin, boolean isLocal,
            String honorificPrefix, String fullName, String companyName, String streetAddress,
            String region, String locality, String dependentLocality, String postalCode,
            String sortingCode, String countryCode, String phoneNumber, String emailAddress,
            String languageCode) {
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
        mPhoneNumber = phoneNumber;
        mEmailAddress = emailAddress;
        mLanguageCode = languageCode;
    }

    @CalledByNative
    public String getGUID() {
        return mGUID;
    }

    @CalledByNative
    public String getOrigin() {
        return mOrigin;
    }

    public boolean getIsLocal() {
        return mIsLocal;
    }

    @CalledByNative
    public String getHonorificPrefix() {
        return mHonorificPrefix;
    }

    @CalledByNative
    public String getFullName() {
        return mFullName;
    }

    @CalledByNative
    public String getCompanyName() {
        return mCompanyName;
    }

    @CalledByNative
    public String getStreetAddress() {
        return mStreetAddress;
    }

    @CalledByNative
    public String getRegion() {
        return mRegion;
    }

    @CalledByNative
    public String getLocality() {
        return mLocality;
    }

    @CalledByNative
    public String getDependentLocality() {
        return mDependentLocality;
    }

    @CalledByNative
    public String getPostalCode() {
        return mPostalCode;
    }

    @CalledByNative
    public String getSortingCode() {
        return mSortingCode;
    }

    @CalledByNative
    public String getCountryCode() {
        return mCountryCode;
    }

    @CalledByNative
    public String getPhoneNumber() {
        return mPhoneNumber;
    }

    @CalledByNative
    public String getEmailAddress() {
        return mEmailAddress;
    }

    @CalledByNative
    public String getLanguageCode() {
        return mLanguageCode;
    }
}
