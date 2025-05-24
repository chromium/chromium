// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout.data;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.fast_checkout.R;
import org.chromium.components.autofill.RecordType;

/** A profile, similar to the one used by the PersonalDataManager. */
@NullMarked
public class FastCheckoutAutofillProfile {
    private final String mGUID;
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
    private final @RecordType int mRecordType;

    @CalledByNative
    public FastCheckoutAutofillProfile(
            String guid,
            String fullName,
            String companyName,
            String streetAddress,
            String region,
            String locality,
            String dependentLocality,
            String postalCode,
            String sortingCode,
            String countryCode,
            String countryName,
            String phoneNumber,
            String emailAddress,
            String languageCode,
            @RecordType int recordType) {
        mGUID = guid;
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
        mRecordType = recordType;
    }

    @CalledByNative
    public String getGUID() {
        return mGUID;
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

    public String getCountryName() {
        return mCountryName;
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

    public @RecordType int getRecordType() {
        return mRecordType;
    }

    public int getAddressHomeAndWorkIconId() {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_ENABLE_SUPPORT_FOR_HOME_AND_WORK)) {
            return R.drawable.location_on_logo;
        }

        @RecordType int recordType = getRecordType();
        switch (recordType) {
            case RecordType.ACCOUNT_HOME:
                return R.drawable.home_logo;
            case RecordType.ACCOUNT_WORK:
                return R.drawable.work_logo;
            default:
                return R.drawable.location_on_logo;
        }
    }
}
