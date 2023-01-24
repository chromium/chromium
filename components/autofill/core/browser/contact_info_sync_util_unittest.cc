// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info_sync_util.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using sync_pb::ContactInfoSpecifics;

constexpr char kGuid[] = "00000000-0000-0000-0000-000000000001";
constexpr char kInvalidGuid[] = "1234";
constexpr int kNonChromeModifier = 1234;
const auto kUseDate = base::Time::FromDoubleT(123);
const auto kModificationDate = base::Time::FromDoubleT(456);

// Returns a profile with all fields set. Contains identical data to the data
// returned from `ConstructCompleteSpecifics()`.
AutofillProfile ConstructCompleteProfile() {
  AutofillProfile profile(kGuid, /*origin=*/"",
                          AutofillProfile::Source::kAccount);

  profile.set_use_count(123);
  profile.set_use_date(kUseDate);
  profile.set_modification_date(kModificationDate);
  profile.set_language_code("en");
  profile.set_profile_label("profile_label");
  profile.set_initial_creator_id(
      AutofillProfile::kInitialCreatorOrModifierChrome);
  profile.set_last_modifier_id(kNonChromeModifier);

  // Set name-related values and statuses.
  profile.SetRawInfoWithVerificationStatus(NAME_HONORIFIC_PREFIX, u"Dr.",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"K.",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Doe",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"D",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"o",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"e",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"John K. Doe",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(NAME_FULL_WITH_HONORIFIC_PREFIX,
                                           u"Dr. John K. Doe",
                                           VerificationStatus::kFormatted);

  // Set address-related values and statuses.
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Mountain View",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"California",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"94043",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"US",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_ADDRESS,
                                           u"123 Fake St. Dep Premise\n"
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SORTING_CODE, u"CEDEX",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                           u"Santa Clara",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Fake St.", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_HOUSE_NUMBER, u"123",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_STREET_NAME,
                                           u"Dep",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_AND_DEPENDENT_STREET_NAME, u"Fake St. Dep",
      VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_PREMISE_NAME, u"Premise", VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);

  // All of the following types don't store verification statuses.
  // Set email, phone and company values.
  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");

  // Set birthdate-related values.
  profile.SetRawInfoAsInt(BIRTHDATE_DAY, 14);
  profile.SetRawInfoAsInt(BIRTHDATE_MONTH, 3);
  profile.SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, 1997);

  return profile;
}

// Helper function to set ContactInfoSpecifics::String- and IntegerToken
// together with their verification status.
template <typename TokenType, typename Value>
void SetToken(TokenType* token,
              const Value& value,
              ContactInfoSpecifics::VerificationStatus status) {
  token->set_value(value);
  token->mutable_metadata()->set_status(status);
}

// Returns ContactInfoSpecifics with all fields set. Contains identical data to
// the profile returned from `ConstructCompleteProfile()`.
ContactInfoSpecifics ConstructCompleteSpecifics() {
  ContactInfoSpecifics specifics;

  specifics.set_guid(kGuid);
  specifics.set_use_count(123);
  specifics.set_use_date_windows_epoch_micros(kUseDate.ToTimeT());
  specifics.set_date_modified_windows_epoch_micros(kModificationDate.ToTimeT());
  specifics.set_language_code("en");
  specifics.set_profile_label("profile_label");
  specifics.set_initial_creator_id(
      AutofillProfile::kInitialCreatorOrModifierChrome);
  specifics.set_last_modifier_id(kNonChromeModifier);

  // Set name-related values and statuses.
  SetToken(specifics.mutable_name_honorific(), "Dr.",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_name_first(), "John",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_name_middle(), "K.",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_name_last(), "Doe",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_name_last_first(), "D",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_last_conjunction(), "o",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_last_second(), "e",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_name_full(), "John K. Doe",
           ContactInfoSpecifics::USER_VERIFIED);
  SetToken(specifics.mutable_name_full_with_honorific(), "Dr. John K. Doe",
           ContactInfoSpecifics::FORMATTED);

  // Set address-related values and statuses.
  SetToken(specifics.mutable_address_city(), "Mountain View",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_state(), "California",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_zip(), "94043",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_country(), "US",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_street_address(),
           "123 Fake St. Dep Premise\n"
           "Apt. 10 Floor 2",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_sorting_code(), "CEDEX",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(), "Santa Clara",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "Fake St.",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "123",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_dependent_thoroughfare_name(), "Dep",
           ContactInfoSpecifics::FORMATTED);
  SetToken(
      specifics.mutable_address_thoroughfare_and_dependent_thoroughfare_name(),
      "Fake St. Dep", ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_premise_name(), "Premise",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_subpremise_name(), "Apt. 10 Floor 2",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_apt_num(), "10",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_floor(), "2",
           ContactInfoSpecifics::PARSED);

  // All of the following types don't store verification statuses in
  // AutofillProfile. This corresponds to `VERIFICATION_STATUS_UNSPECIFIED`.
  // Set email, phone and company values and statuses.
  SetToken(specifics.mutable_email_address(), "user@example.com",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_company_name(), "Google, Inc.",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_phone_home_whole_number(), "1.800.555.1234",
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  // Set birthdate-related values and statuses.
  SetToken(specifics.mutable_birthdate_day(), 14,
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_birthdate_month(), 3,
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);
  SetToken(specifics.mutable_birthdate_year(), 1997,
           ContactInfoSpecifics::VERIFICATION_STATUS_UNSPECIFIED);

  return specifics;
}

}  // namespace

// Test that converting AutofillProfile -> ContactInfoSpecifics works.
TEST(ContactInfoSyncUtilTest, CreateContactInfoEntityDataFromAutofillProfile) {
  base::test::ScopedFeatureList honorific_prefixes_feature;
  honorific_prefixes_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  AutofillProfile profile = ConstructCompleteProfile();
  ContactInfoSpecifics specifics = ConstructCompleteSpecifics();

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateContactInfoEntityDataFromAutofillProfile(profile);
  ASSERT_TRUE(entity_data != nullptr);
  EXPECT_EQ(entity_data->name, profile.guid());
  EXPECT_EQ(specifics.SerializeAsString(),
            entity_data->specifics.contact_info().SerializeAsString());
}

// Test that only profiles with valid GUID are converted.
TEST(ContactInfoSyncUtilTest,
     CreateContactInfoEntityDataFromAutofillProfile_InvalidGUID) {
  AutofillProfile profile(kInvalidGuid, /*origin=*/"",
                          AutofillProfile::Source::kAccount);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(profile), nullptr);
}

// Test that AutofillProfiles with invalid source are not converted.
TEST(ContactInfoSyncUtilTest,
     CreateContactInfoEntityDataFromAutofillProfile_InvalidSource) {
  AutofillProfile profile(kGuid, /*origin=*/"",
                          AutofillProfile::Source::kLocalOrSyncable);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(profile), nullptr);
}

// Test that converting ContactInfoSpecifics -> AutofillProfile works.
TEST(ContactInfoSyncUtilTest, CreateAutofillProfileFromContactInfoSpecifics) {
  ContactInfoSpecifics specifics = ConstructCompleteSpecifics();
  AutofillProfile profile = ConstructCompleteProfile();

  std::unique_ptr<AutofillProfile> converted_profile =
      CreateAutofillProfileFromContactInfoSpecifics(specifics);
  ASSERT_TRUE(converted_profile != nullptr);
  EXPECT_TRUE(profile.EqualsIncludingUsageStatsForTesting(*converted_profile));
}

// Test that only specifics with valid GUID are converted.
TEST(ContactInfoSyncUtilTest,
     CreateAutofillProfileFromContactInfoSpecifics_InvalidGUID) {
  ContactInfoSpecifics specifics;
  specifics.set_guid(kInvalidGuid);
  EXPECT_EQ(CreateAutofillProfileFromContactInfoSpecifics(specifics), nullptr);
}

}  // namespace autofill
