// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import java.util.HashMap;
import java.util.Map;

/**
 * Autofill address information.
 * The creation and/or modification of an AutofillProfile is assumed to involve the user (e.g.
 * data reviewed by the user in the {@link
 * org.chromium.chrome.browser.autofill.settings.AddressEditor}), therefore all new values gain
 * {@link VerificationStatus.USER_VERIFIED} status.
 */
@JNINamespace("autofill")
public class AutofillProfile {
    private String mGUID;
    private @RecordType int mRecordType;
    private Map<Integer, ValueWithStatus> mFields;
    private String mLabel;
    private String mLanguageCode;

    @VisibleForTesting
    static class ValueWithStatus {
        static final ValueWithStatus EMPTY = new ValueWithStatus("", VerificationStatus.NO_STATUS);

        private final String mValue;
        private final @VerificationStatus int mStatus;

        ValueWithStatus(String value, @VerificationStatus int status) {
            mValue = value;
            mStatus = status;
        }

        String getValue() {
            return mValue;
        }

        @VerificationStatus
        int getStatus() {
            return mStatus;
        }
    }

    /** Builder for the {@link AutofillProfile}. */
    public static final class Builder {
        private String mGUID = "";
        private @RecordType int mRecordType = RecordType.LOCAL_OR_SYNCABLE;
        private ValueWithStatus mFullName = ValueWithStatus.EMPTY;
        private ValueWithStatus mCompanyName = ValueWithStatus.EMPTY;
        private ValueWithStatus mStreetAddress = ValueWithStatus.EMPTY;
        private ValueWithStatus mRegion = ValueWithStatus.EMPTY;
        private ValueWithStatus mLocality = ValueWithStatus.EMPTY;
        private ValueWithStatus mDependentLocality = ValueWithStatus.EMPTY;
        private ValueWithStatus mPostalCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mSortingCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mCountryCode = ValueWithStatus.EMPTY;
        private ValueWithStatus mPhoneNumber = ValueWithStatus.EMPTY;
        private ValueWithStatus mEmailAddress = ValueWithStatus.EMPTY;
        private String mLanguageCode = "";

        public Builder setGUID(String guid) {
            mGUID = guid;
            return this;
        }

        public Builder setRecordType(@RecordType int recordType) {
            mRecordType = recordType;
            return this;
        }

        public Builder setFullName(String fullName) {
            mFullName = new ValueWithStatus(fullName, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setFullName(String fullName, @VerificationStatus int status) {
            mFullName = new ValueWithStatus(fullName, status);
            return this;
        }

        public Builder setCompanyName(String companyName) {
            mCompanyName = new ValueWithStatus(companyName, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setCompanyName(String companyName, @VerificationStatus int status) {
            mCompanyName = new ValueWithStatus(companyName, status);
            return this;
        }

        public Builder setStreetAddress(String streetAddress) {
            mStreetAddress = new ValueWithStatus(streetAddress, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setStreetAddress(String streetAddress, @VerificationStatus int status) {
            mStreetAddress = new ValueWithStatus(streetAddress, status);
            return this;
        }

        public Builder setRegion(String region) {
            mRegion = new ValueWithStatus(region, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setRegion(String region, @VerificationStatus int status) {
            mRegion = new ValueWithStatus(region, status);
            return this;
        }

        public Builder setLocality(String locality) {
            mLocality = new ValueWithStatus(locality, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setLocality(String locality, @VerificationStatus int status) {
            mLocality = new ValueWithStatus(locality, status);
            return this;
        }

        public Builder setDependentLocality(String dependentLocality) {
            mDependentLocality =
                    new ValueWithStatus(dependentLocality, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setDependentLocality(
                String dependentLocality, @VerificationStatus int status) {
            mDependentLocality = new ValueWithStatus(dependentLocality, status);
            return this;
        }

        public Builder setPostalCode(String postalCode) {
            mPostalCode = new ValueWithStatus(postalCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setPostalCode(String postalCode, @VerificationStatus int status) {
            mPostalCode = new ValueWithStatus(postalCode, status);
            return this;
        }

        public Builder setSortingCode(String sortingCode) {
            mSortingCode = new ValueWithStatus(sortingCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setSortingCode(String sortingCode, @VerificationStatus int status) {
            mSortingCode = new ValueWithStatus(sortingCode, status);
            return this;
        }

        public Builder setCountryCode(String countryCode) {
            mCountryCode = new ValueWithStatus(countryCode, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setCountryCode(String countryCode, @VerificationStatus int status) {
            mCountryCode = new ValueWithStatus(countryCode, status);
            return this;
        }

        public Builder setPhoneNumber(String phoneNumber) {
            mPhoneNumber = new ValueWithStatus(phoneNumber, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setPhoneNumber(String phoneNumber, @VerificationStatus int status) {
            mPhoneNumber = new ValueWithStatus(phoneNumber, status);
            return this;
        }

        public Builder setEmailAddress(String emailAddress) {
            mEmailAddress = new ValueWithStatus(emailAddress, VerificationStatus.USER_VERIFIED);
            return this;
        }

        public Builder setEmailAddress(String emailAddress, @VerificationStatus int status) {
            mEmailAddress = new ValueWithStatus(emailAddress, status);
            return this;
        }

        public Builder setLanguageCode(String languageCode) {
            mLanguageCode = languageCode;
            return this;
        }

        public AutofillProfile build() {
            return new AutofillProfile(
                    mGUID,
                    mRecordType,
                    mFullName,
                    mCompanyName,
                    mStreetAddress,
                    mRegion,
                    mLocality,
                    mDependentLocality,
                    mPostalCode,
                    mSortingCode,
                    mCountryCode,
                    mPhoneNumber,
                    mEmailAddress,
                    mLanguageCode);
        }
    }

    public static Builder builder() {
        return new Builder();
    }

    @CalledByNative
    private AutofillProfile(
            @JniType("std::string") String guid,
            @RecordType int recordType,
            @JniType("std::string") String languageCode) {
        mGUID = guid;
        mRecordType = recordType;
        mLanguageCode = languageCode;
        mFields = new HashMap<>();
    }

    private AutofillProfile(
            String guid,
            @RecordType int recordType,
            ValueWithStatus fullName,
            ValueWithStatus companyName,
            ValueWithStatus streetAddress,
            ValueWithStatus region,
            ValueWithStatus locality,
            ValueWithStatus dependentLocality,
            ValueWithStatus postalCode,
            ValueWithStatus sortingCode,
            ValueWithStatus countryCode,
            ValueWithStatus phoneNumber,
            ValueWithStatus emailAddress,
            String languageCode) {
        this(guid, recordType, languageCode);
        mFields.put(FieldType.NAME_FULL, fullName);
        mFields.put(FieldType.COMPANY_NAME, companyName);
        mFields.put(FieldType.ADDRESS_HOME_STREET_ADDRESS, streetAddress);
        mFields.put(FieldType.ADDRESS_HOME_STATE, region);
        mFields.put(FieldType.ADDRESS_HOME_CITY, locality);
        mFields.put(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY, dependentLocality);
        mFields.put(FieldType.ADDRESS_HOME_ZIP, postalCode);
        mFields.put(FieldType.ADDRESS_HOME_SORTING_CODE, sortingCode);
        mFields.put(FieldType.ADDRESS_HOME_COUNTRY, countryCode);
        mFields.put(FieldType.PHONE_HOME_WHOLE_NUMBER, phoneNumber);
        mFields.put(FieldType.EMAIL_ADDRESS, emailAddress);
    }

    /* Builds an AutofillProfile that is an exact copy of the one passed as parameter. */
    public AutofillProfile(AutofillProfile profile) {
        mGUID = profile.getGUID();
        mRecordType = profile.getRecordType();

        mFields = new HashMap<>(profile.mFields);

        mLanguageCode = profile.getLanguageCode();
        mLabel = profile.getLabel();
    }

    @CalledByNative
    private @JniType("std::vector<int32_t>") int[] getFieldTypes() {
        return mFields.keySet().stream().mapToInt(i -> i).toArray();
    }

    @CalledByNative
    public @JniType("std::string") String getGUID() {
        return mGUID;
    }

    @CalledByNative
    public @JniType("AutofillProfile::RecordType") @RecordType int getRecordType() {
        return mRecordType;
    }

    public String getLabel() {
        return mLabel;
    }

    @CalledByNative
    public @JniType("std::u16string") String getInfo(@FieldType int fieldType) {
        if (!mFields.containsKey(fieldType)) {
            return "";
        }
        return mFields.get(fieldType).getValue();
    }

    @CalledByNative
    public @JniType("VerificationStatus") @VerificationStatus int getInfoStatus(
            @FieldType int fieldType) {
        if (!mFields.containsKey(fieldType)) {
            return VerificationStatus.NO_STATUS;
        }
        return mFields.get(fieldType).getStatus();
    }

    public String getFullName() {
        return getInfo(FieldType.NAME_FULL);
    }

    @VisibleForTesting
    @VerificationStatus
    public int getFullNameStatus() {
        return getInfoStatus(FieldType.NAME_FULL);
    }

    public String getCompanyName() {
        return getInfo(FieldType.COMPANY_NAME);
    }

    @VerificationStatus
    int getCompanyNameStatus() {
        return getInfoStatus(FieldType.COMPANY_NAME);
    }

    public String getStreetAddress() {
        return getInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS);
    }

    @VisibleForTesting
    @VerificationStatus
    public int getStreetAddressStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_STREET_ADDRESS);
    }

    public String getRegion() {
        return getInfo(FieldType.ADDRESS_HOME_STATE);
    }

    @VisibleForTesting
    @VerificationStatus
    public int getRegionStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_STATE);
    }

    public String getLocality() {
        return getInfo(FieldType.ADDRESS_HOME_CITY);
    }

    @VisibleForTesting
    @VerificationStatus
    public int getLocalityStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_CITY);
    }

    public String getDependentLocality() {
        return getInfo(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY);
    }

    private @VerificationStatus int getDependentLocalityStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY);
    }

    public String getPostalCode() {
        return getInfo(FieldType.ADDRESS_HOME_ZIP);
    }

    @VisibleForTesting
    @VerificationStatus
    public int getPostalCodeStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_ZIP);
    }

    public String getSortingCode() {
        return getInfo(FieldType.ADDRESS_HOME_SORTING_CODE);
    }

    private @VerificationStatus int getSortingCodeStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_SORTING_CODE);
    }

    @CalledByNative
    public @JniType("std::string") String getCountryCode() {
        return getInfo(FieldType.ADDRESS_HOME_COUNTRY);
    }

    private @VerificationStatus int getCountryCodeStatus() {
        return getInfoStatus(FieldType.ADDRESS_HOME_COUNTRY);
    }

    public String getPhoneNumber() {
        return getInfo(FieldType.PHONE_HOME_WHOLE_NUMBER);
    }

    private @VerificationStatus int getPhoneNumberStatus() {
        return getInfoStatus(FieldType.PHONE_HOME_WHOLE_NUMBER);
    }

    public String getEmailAddress() {
        return getInfo(FieldType.EMAIL_ADDRESS);
    }

    private @VerificationStatus int getEmailAddressStatus() {
        return getInfoStatus(FieldType.EMAIL_ADDRESS);
    }

    @CalledByNative
    public @JniType("std::string") String getLanguageCode() {
        return mLanguageCode;
    }

    public void setGUID(String guid) {
        mGUID = guid;
    }

    public void setLabel(String label) {
        mLabel = label;
    }

    public void setRecordType(@RecordType int recordType) {
        mRecordType = recordType;
    }

    @CalledByNative
    public void setInfo(
            @FieldType int fieldType,
            @JniType("std::u16string") @Nullable String value,
            @VerificationStatus int status) {
        value = value == null ? "" : value;
        mFields.put(fieldType, new ValueWithStatus(value, status));
    }

    public void setInfo(@FieldType int fieldType, @Nullable String value) {
        setInfo(fieldType, value, VerificationStatus.USER_VERIFIED);
    }

    public void setFullName(String fullName) {
        setInfo(FieldType.NAME_FULL, fullName);
    }

    public void setCompanyName(String companyName) {
        setInfo(FieldType.COMPANY_NAME, companyName);
    }

    public void setStreetAddress(String streetAddress) {
        setInfo(FieldType.ADDRESS_HOME_STREET_ADDRESS, streetAddress);
    }

    public void setRegion(String region) {
        setInfo(FieldType.ADDRESS_HOME_STATE, region);
    }

    public void setLocality(String locality) {
        setInfo(FieldType.ADDRESS_HOME_CITY, locality);
    }

    public void setDependentLocality(String dependentLocality) {
        setInfo(FieldType.ADDRESS_HOME_DEPENDENT_LOCALITY, dependentLocality);
    }

    public void setPostalCode(String postalCode) {
        setInfo(FieldType.ADDRESS_HOME_ZIP, postalCode);
    }

    public void setSortingCode(String sortingCode) {
        setInfo(FieldType.ADDRESS_HOME_SORTING_CODE, sortingCode);
    }

    public void setCountryCode(String countryCode) {
        setInfo(FieldType.ADDRESS_HOME_COUNTRY, countryCode);
    }

    public void setPhoneNumber(String phoneNumber) {
        setInfo(FieldType.PHONE_HOME_WHOLE_NUMBER, phoneNumber);
    }

    public void setEmailAddress(String emailAddress) {
        setInfo(FieldType.EMAIL_ADDRESS, emailAddress);
    }

    public void setLanguageCode(String languageCode) {
        mLanguageCode = languageCode;
    }

    /** Used by ArrayAdapter in credit card settings. */
    @Override
    public String toString() {
        return mLabel;
    }
}
