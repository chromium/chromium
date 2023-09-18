// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info_sync_util.h"

#include "base/hash/hash.h"
#include "base/strings/to_string.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
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
  AutofillProfile profile(kGuid, AutofillProfile::Source::kAccount);

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
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"123 Fake St. Premise Marcos y Oliva\n"
      u"Apt. 10 Floor 2 Red tree",
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
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                           u"Fake St. 123",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK, u"Red tree",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS,
                                           u"Marcos y Oliva",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca",
                                           VerificationStatus::kObserved);

  // All of the following types don't store verification statuses.
  // Set email, phone and company values.
  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");

  // Set birthdate-related values.
  profile.SetRawInfoAsInt(BIRTHDATE_DAY, 14);
  profile.SetRawInfoAsInt(BIRTHDATE_MONTH, 3);
  profile.SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, 1997);

  // Add some `ProfileTokenQuality` observations.
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted,
                      ProfileTokenQualityTestApi::FormSignatureHash(12));
  test_api(profile.token_quality())
      .AddObservation(ADDRESS_HOME_CITY,
                      ProfileTokenQuality::ObservationType::kEditedFallback,
                      ProfileTokenQualityTestApi::FormSignatureHash(21));

  return profile;
}

// Helper function to set ContactInfoSpecifics::String- and IntegerToken
// together with their verification status and value_hash.
template <typename TokenType, typename Value>
void SetToken(TokenType* token,
              const Value& value,
              ContactInfoSpecifics::VerificationStatus status) {
  token->set_value(value);
  ContactInfoSpecifics::TokenMetadata* metadata = token->mutable_metadata();
  metadata->set_status(status);
  metadata->set_value_hash(base::PersistentHash(base::ToString(value)));
}

// Returns ContactInfoSpecifics with all fields set. Contains identical data to
// the profile returned from `ConstructCompleteProfile()`.
ContactInfoSpecifics ConstructCompleteSpecifics() {
  ContactInfoSpecifics specifics;

  specifics.set_guid(kGuid);
  specifics.set_use_count(123);
  specifics.set_use_date_unix_epoch_seconds(kUseDate.ToTimeT());
  specifics.set_date_modified_unix_epoch_seconds(kModificationDate.ToTimeT());
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
           "123 Fake St. Premise Marcos y Oliva\n"
           "Apt. 10 Floor 2 Red tree",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_sorting_code(), "CEDEX",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_dependent_locality(), "Santa Clara",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_thoroughfare_name(), "Fake St.",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_thoroughfare_number(), "123",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_street_location(), "Fake St. 123",
           ContactInfoSpecifics::FORMATTED);
  SetToken(specifics.mutable_address_subpremise_name(), "Apt. 10 Floor 2",
           ContactInfoSpecifics::OBSERVED);
  SetToken(specifics.mutable_address_apt_num(), "10",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_floor(), "2",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_landmark(), "Red tree",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_between_streets(), "Marcos y Oliva",
           ContactInfoSpecifics::PARSED);
  SetToken(specifics.mutable_address_admin_level_2(), "Oxaca",
           ContactInfoSpecifics::OBSERVED);
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

  // Add some `ProfileTokenQuality` observations.
  ContactInfoSpecifics::Observation* observation =
      specifics.mutable_name_first()->mutable_metadata()->add_observations();
  observation->set_type(
      static_cast<int>(ProfileTokenQuality::ObservationType::kAccepted));
  observation->set_form_hash(12);
  observation =
      specifics.mutable_address_city()->mutable_metadata()->add_observations();
  observation->set_type(
      static_cast<int>(ProfileTokenQuality::ObservationType::kEditedFallback));
  observation->set_form_hash(21);

  return specifics;
}

}  // namespace

class ContactInfoSyncUtilTest : public testing::Test {
 public:
  ContactInfoSyncUtilTest() {
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForLandmark,
         features::kAutofillEnableSupportForBetweenStreets,
         features::kAutofillEnableSupportForAdminLevel2,
         features::kAutofillTrackProfileTokenQuality},
        {});
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Test that converting AutofillProfile -> ContactInfoSpecifics works.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile) {
  base::test::ScopedFeatureList honorific_prefixes_feature;
  honorific_prefixes_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  AutofillProfile profile = ConstructCompleteProfile();
  ContactInfoSpecifics specifics = ConstructCompleteSpecifics();

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateContactInfoEntityDataFromAutofillProfile(
          profile, /*base_contact_info_specifics=*/{});
  ASSERT_TRUE(entity_data != nullptr);
  EXPECT_EQ(entity_data->name, profile.guid());
  EXPECT_EQ(specifics.SerializeAsString(),
            entity_data->specifics.contact_info().SerializeAsString());
}

// Test that only profiles with valid GUID are converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile_InvalidGUID) {
  AutofillProfile profile(kInvalidGuid, AutofillProfile::Source::kAccount);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{}),
            nullptr);
}

// Test that AutofillProfiles with invalid source are not converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateContactInfoEntityDataFromAutofillProfile_InvalidSource) {
  AutofillProfile profile(kGuid, AutofillProfile::Source::kLocalOrSyncable);
  EXPECT_EQ(CreateContactInfoEntityDataFromAutofillProfile(
                profile, /*base_contact_info_specifics=*/{}),
            nullptr);
}

// Test that supported fields and nested messages are successfully trimmed.
TEST_F(ContactInfoSyncUtilTest, TrimAllSupportedFieldsFromRemoteSpecifics) {
  sync_pb::ContactInfoSpecifics contact_info_specifics;
  contact_info_specifics.mutable_address_city()->set_value("City");
  contact_info_specifics.mutable_address_city()->mutable_metadata()->set_status(
      sync_pb::ContactInfoSpecifics::VerificationStatus::
          ContactInfoSpecifics_VerificationStatus_OBSERVED);

  sync_pb::ContactInfoSpecifics empty_contact_info_specifics;
  EXPECT_EQ(TrimContactInfoSpecificsDataForCaching(contact_info_specifics)
                .SerializeAsString(),
            empty_contact_info_specifics.SerializeAsString());
}

// Test that supported fields and nested messages are successfully trimmed but
// that unsupported fields are preserved.
TEST_F(ContactInfoSyncUtilTest,
       TrimAllSupportedFieldsFromRemoteSpecifics_PreserveUnsupportedFields) {
  sync_pb::ContactInfoSpecifics contact_info_specifics_with_only_unknown_fields;

  // Set an unsupported field in both the top-level message and also in a nested
  // message.
  *contact_info_specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unsupported_fields";
  *contact_info_specifics_with_only_unknown_fields.mutable_address_city()
       ->mutable_unknown_fields() = "unsupported_field_in_nested_message";

  // Create a copy and set a value to the same nested message that already
  // contains an unsupported field.
  sync_pb::ContactInfoSpecifics
      contact_info_specifics_with_known_and_unknown_fields =
          contact_info_specifics_with_only_unknown_fields;
  contact_info_specifics_with_known_and_unknown_fields.mutable_address_city()
      ->set_value("City");

  EXPECT_EQ(TrimContactInfoSpecificsDataForCaching(
                contact_info_specifics_with_known_and_unknown_fields)
                .SerializeAsString(),
            contact_info_specifics_with_only_unknown_fields
                .SerializePartialAsString());
}

// Test that the conversion of a profile to specifics preserve the unsupported
// fields.
TEST_F(ContactInfoSyncUtilTest, ContactInfoSpecificsFromAutofillProfile) {
  // If this feature is not available the honorific prefix will be lost in the
  // back and forth conversion.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      autofill::features::kAutofillEnableSupportForHonorificPrefixes);

  // Create the base message that only contains unsupported fields in both the
  // top-level and a nested message.
  sync_pb::ContactInfoSpecifics contact_info_specifics_with_only_unknown_fields;
  *contact_info_specifics_with_only_unknown_fields.mutable_unknown_fields() =
      "unsupported_fields";
  *contact_info_specifics_with_only_unknown_fields.mutable_address_city()
       ->mutable_unknown_fields() = "unsupported_field_in_nested_message";

  ContactInfoSpecifics contact_info_specifics_from_profile =
      ContactInfoSpecificsFromAutofillProfile(
          ConstructCompleteProfile(),
          contact_info_specifics_with_only_unknown_fields);

  // Test that the unknown fields are preserved and that the rest of the
  // specifics match the expectations.
  sync_pb::ContactInfoSpecifics expected_contact_info =
      ConstructCompleteSpecifics();
  *expected_contact_info.mutable_unknown_fields() = "unsupported_fields";
  *expected_contact_info.mutable_address_city()->mutable_unknown_fields() =
      "unsupported_field_in_nested_message";

  EXPECT_EQ(contact_info_specifics_from_profile.SerializeAsString(),
            expected_contact_info.SerializeAsString());
}

// Test that converting ContactInfoSpecifics -> AutofillProfile works.
TEST_F(ContactInfoSyncUtilTest, CreateAutofillProfileFromContactInfoSpecifics) {
  ContactInfoSpecifics specifics = ConstructCompleteSpecifics();
  AutofillProfile profile = ConstructCompleteProfile();

  std::unique_ptr<AutofillProfile> converted_profile =
      CreateAutofillProfileFromContactInfoSpecifics(specifics);
  ASSERT_TRUE(converted_profile != nullptr);
  EXPECT_TRUE(profile.EqualsIncludingUsageStatsForTesting(*converted_profile));
}

// Test that only specifics with valid GUID are converted.
TEST_F(ContactInfoSyncUtilTest,
       CreateAutofillProfileFromContactInfoSpecifics_InvalidGUID) {
  ContactInfoSpecifics specifics;
  specifics.set_guid(kInvalidGuid);
  EXPECT_EQ(CreateAutofillProfileFromContactInfoSpecifics(specifics), nullptr);
}

// Tests that if a token's `value` changes by external means, its observations
// are reset.
TEST_F(ContactInfoSyncUtilTest, ObservationResetting) {
  // Create a profile and collect an observation for NAME_FIRST.
  AutofillProfile profile = test::GetFullProfile();
  profile.set_source_for_testing(AutofillProfile::Source::kAccount);
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted);

  // Simulate sending the `profile` to Sync and modifying its NAME_FIRST by an
  // external integrator. Since metadata is opaque to external integrators, the
  // metadata's `value_hash` is not updated.
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateContactInfoEntityDataFromAutofillProfile(
          profile, /*base_contact_info_specifics=*/{});
  ASSERT_NE(entity_data, nullptr);
  ContactInfoSpecifics* specifics =
      entity_data->specifics.mutable_contact_info();
  specifics->mutable_name_first()->set_value("different " +
                                             specifics->name_first().value());

  // Simulate syncing the `specifics` back to Autofill. Expect that the
  // NAME_FIRST observations are cleared.
  std::unique_ptr<AutofillProfile> updated_profile =
      CreateAutofillProfileFromContactInfoSpecifics(*specifics);
  ASSERT_NE(updated_profile, nullptr);
  EXPECT_TRUE(updated_profile->token_quality()
                  .GetObservationTypesForFieldType(NAME_FIRST)
                  .empty());
}

}  // namespace autofill
