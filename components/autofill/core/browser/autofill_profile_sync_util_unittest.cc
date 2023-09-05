// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_structured_address_component.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
namespace autofill {

namespace {
using base::ASCIIToUTF16;
using base::UTF16ToUTF8;
using base::UTF8ToUTF16;
using sync_pb::AutofillProfileSpecifics;
using syncer::EntityData;

// Some guids for testing.
const char kGuid[] = "EDC609ED-7EEE-4F27-B00C-423242A9C44A";
const char kGuidInvalid[] = "EDC609ED";

const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructCompleteSpecifics().
AutofillProfile ConstructCompleteProfile() {
  AutofillProfile profile(kGuid, AutofillProfile::Source::kLocalOrSyncable);

  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(1423182152));

  profile.set_profile_label("profile_label");

  // Set testing values and statuses for the name.
  profile.SetRawInfoWithVerificationStatus(NAME_HONORIFIC_PREFIX, u"Dr.",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(NAME_FULL_WITH_HONORIFIC_PREFIX,
                                           u"Dr. John K. Doe",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(NAME_FULL, u"John K. Doe",
                                           VerificationStatus::kUserVerified);
  profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"K.",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Doe",
                                           VerificationStatus::kFormatted);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"D",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"e",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"o",
                                           VerificationStatus::kParsed);

  // Set email, phone and company testing values.
  profile.SetRawInfo(EMAIL_ADDRESS, u"user@example.com");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"1.800.555.1234");
  profile.SetRawInfo(COMPANY_NAME, u"Google, Inc.");
  profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"123 Fake St. Premise Marcos y Oliva\n"
      u"Apt. 10 Floor 2 Red tree",
      VerificationStatus::kObserved);

  // Set testing values and statuses for the address.
  EXPECT_EQ(u"123 Fake St. Premise Marcos y Oliva",
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(u"Apt. 10 Floor 2 Red tree",
            profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"Mountain View",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"California",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"94043",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"US",
                                           VerificationStatus::kObserved);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_LANDMARK, u"Red tree",
                                           VerificationStatus::kParsed);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS,
                                           u"Marcos y Oliva",
                                           VerificationStatus::kParsed);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca",
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
                                           u"123 Fake St.",
                                           VerificationStatus::kFormatted);

  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SUBPREMISE,
                                           u"Apt. 10 Floor 2",
                                           VerificationStatus::kObserved);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                           VerificationStatus::kParsed);
  profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                           VerificationStatus::kParsed);
  profile.set_language_code("en");

  // Set testing values for the birthdate.
  profile.SetRawInfoAsInt(BIRTHDATE_DAY, 14);
  profile.SetRawInfoAsInt(BIRTHDATE_MONTH, 3);
  profile.SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, 1997);

  return profile;
}

// Returns AutofillProfileSpecifics with all Autofill profile fields set.
// Contains identical data to the data returned from ConstructCompleteProfile().
AutofillProfileSpecifics ConstructCompleteSpecifics() {
  AutofillProfileSpecifics specifics;

  specifics.set_guid(kGuid);
  // TODO(crbug.com/1441905): Remove. See comment in
  // `CreateEntityDataFromAutofillProfile()`.
  specifics.set_deprecated_origin(kSettingsOrigin);
  specifics.set_use_count(7);
  specifics.set_use_date(1423182152);
  specifics.set_profile_label("profile_label");

  // Set values and statuses for the names.
  specifics.add_name_honorific("Dr.");
  specifics.add_name_honorific_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.add_name_full_with_honorific("Dr. John K. Doe");
  specifics.add_name_full_with_honorific_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.add_name_first("John");
  specifics.add_name_first_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.add_name_middle("K.");
  specifics.add_name_middle_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.add_name_last("Doe");
  specifics.add_name_last_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.add_name_last_first("D");
  specifics.add_name_last_first_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_last_second("e");
  specifics.add_name_last_second_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_last_conjunction("o");
  specifics.add_name_last_conjunction_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.add_name_full("John K. Doe");
  specifics.add_name_full_status(
      AutofillProfileSpecifics::VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_USER_VERIFIED);

  // Set testing values for email, phone and company.
  specifics.add_email_address("user@example.com");
  specifics.add_phone_home_whole_number("1.800.555.1234");
  specifics.set_company_name("Google, Inc.");

  // Set values and statuses for the address.
  // Address lines are derived from the home street address and do not have an
  // independent status.
  specifics.set_address_home_line1("123 Fake St. Premise Marcos y Oliva");
  specifics.set_address_home_line2("Apt. 10 Floor 2 Red tree");
  specifics.set_address_home_street_address(
      "123 Fake St. Premise Marcos y Oliva\n"
      "Apt. 10 Floor 2 Red tree");
  specifics.set_address_home_street_address_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus::
          AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_thoroughfare_name("Fake St.");
  specifics.set_address_home_thoroughfare_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_thoroughfare_number("123");
  specifics.set_address_home_thoroughfare_number_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_street_location("123 Fake St.");
  specifics.set_address_home_street_location_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_FORMATTED);

  specifics.set_address_home_subpremise_name("Apt. 10 Floor 2");
  specifics.set_address_home_subpremise_name_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_apt_num("10");
  specifics.set_address_home_apt_num_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_floor("2");
  specifics.set_address_home_floor_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_city("Mountain View");
  specifics.set_address_home_city_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_state("California");
  specifics.set_address_home_state_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_zip("94043");
  specifics.set_address_home_zip_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_country("US");
  specifics.set_address_home_country_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_landmark("Red tree");
  specifics.set_address_home_landmark_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_between_streets("Marcos y Oliva");
  specifics.set_address_home_between_streets_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_PARSED);

  specifics.set_address_home_admin_level_2("Oxaca");
  specifics.set_address_home_admin_level_2_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_sorting_code("CEDEX");
  specifics.set_address_home_sorting_code_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_dependent_locality("Santa Clara");
  specifics.set_address_home_dependent_locality_status(
      sync_pb::AutofillProfileSpecifics_VerificationStatus_OBSERVED);

  specifics.set_address_home_language_code("en");

  // Set values for the birthdate.
  specifics.set_birthdate_day(14);
  specifics.set_birthdate_month(3);
  specifics.set_birthdate_year(1997);

  return specifics;
}

class AutofillProfileSyncUtilTest : public testing::Test {
 public:
  AutofillProfileSyncUtilTest() {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForLandmark,
         features::kAutofillEnableSupportForBetweenStreets,
         features::kAutofillEnableSupportForAdminLevel2},
        {});
  }

 private:
  autofill::TestAutofillClock test_clock_;
  base::test::ScopedFeatureList features_;
};

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncUtilTest, CreateEntityDataFromAutofillProfile) {
  base::test::ScopedFeatureList structured_names_feature;
  // With this feature enabled, the AutofillProfile supports all tokens
  // and statuses assignable in the specifics. If this feature is
  // disabled, for some tokens
  // AutofillProfile::GetRawInfo(AutofillProfile::SetRawInfo()) is not the
  // identify function. The same is true for the verification status.
  structured_names_feature.InitAndEnableFeature(
      features::kAutofillEnableSupportForHonorificPrefixes);

  AutofillProfile profile = ConstructCompleteProfile();
  AutofillProfileSpecifics specifics = ConstructCompleteSpecifics();

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);
  // The non-unique name should be set to the guid of the profile.
  EXPECT_EQ(entity_data->name, profile.guid());

  EXPECT_EQ(specifics.SerializeAsString(),
            entity_data->specifics.autofill_profile().SerializeAsString());
}

// Test that fields not set for the input are empty in the output.
TEST_F(AutofillProfileSyncUtilTest, CreateEntityDataFromAutofillProfile_Empty) {
  AutofillProfile profile(kGuid);
  ASSERT_FALSE(profile.HasRawInfo(NAME_FULL));
  ASSERT_FALSE(profile.HasRawInfo(COMPANY_NAME));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);
  EXPECT_EQ(1, entity_data->specifics.autofill_profile().name_full_size());
  EXPECT_EQ("", entity_data->specifics.autofill_profile().name_full(0));
  EXPECT_TRUE(entity_data->specifics.autofill_profile().has_company_name());
  EXPECT_EQ("", entity_data->specifics.autofill_profile().company_name());
}

// Test that long fields get trimmed.
TEST_F(AutofillProfileSyncUtilTest,
       CreateEntityDataFromAutofillProfile_Trimmed) {
  std::string kNameLong(AutofillTable::kMaxDataLength + 1, 'a');
  std::string kNameTrimmed(AutofillTable::kMaxDataLength, 'a');

  AutofillProfile profile(kGuid);
  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16(kNameLong));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);

  EXPECT_EQ(kNameTrimmed,
            entity_data->specifics.autofill_profile().name_full(0));
}

// Test that long non-ascii fields get correctly trimmed.
TEST_F(AutofillProfileSyncUtilTest,
       CreateEntityDataFromAutofillProfile_TrimmedNonASCII) {
  // Make the UTF8 string have odd number of bytes and append many 2-bytes
  // characters so that simple ASCII trimming would make the UTF8 string
  // invalid.
  std::string kNameLong("aä");
  std::string kNameTrimmed("a");

  for (unsigned int i = 0; i < AutofillTable::kMaxDataLength / 2 - 1; ++i) {
    kNameLong += "ä";
    kNameTrimmed += "ä";
  }

  AutofillProfile profile(kGuid);
  profile.SetRawInfo(NAME_FULL, UTF8ToUTF16(kNameLong));

  std::unique_ptr<EntityData> entity_data =
      CreateEntityDataFromAutofillProfile(profile);

  EXPECT_EQ(kNameTrimmed,
            entity_data->specifics.autofill_profile().name_full(0));
}

// Ensure that all profile fields are able to be synced down from the server to
// the client (and nothing gets uploaded back).
TEST_F(AutofillProfileSyncUtilTest, CreateAutofillProfileFromSpecifics) {
  // Fix a time for implicitly constructed use_dates in AutofillProfile.
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  AutofillProfileSpecifics specifics = ConstructCompleteSpecifics();
  AutofillProfile profile = ConstructCompleteProfile();

  std::unique_ptr<AutofillProfile> converted_profile =
      CreateAutofillProfileFromSpecifics(specifics);
  EXPECT_TRUE(profile.EqualsIncludingUsageStatsForTesting(*converted_profile));
}

// Test that fields not set for the input are also not set on the output.
TEST_F(AutofillProfileSyncUtilTest, CreateAutofillProfileFromSpecifics_Empty) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  std::unique_ptr<AutofillProfile> profile =
      CreateAutofillProfileFromSpecifics(specifics);

  EXPECT_FALSE(profile->HasRawInfo(NAME_FULL));
  EXPECT_FALSE(profile->HasRawInfo(COMPANY_NAME));
}

// Test that nullptr is produced if the input guid is invalid.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_Invalid) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuidInvalid);

  EXPECT_EQ(nullptr, CreateAutofillProfileFromSpecifics(specifics));
}

// Test that if conflicting info is set for address home, the (deprecated) line1
// & line2 fields get overwritten by the street_address field.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_HomeAddressWins) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  specifics.set_address_home_street_address(
      "123 New St.\n"
      "Apt. 42");
  specifics.set_address_home_line1("456 Old St.");
  specifics.set_address_home_line2("Apt. 43");

  std::unique_ptr<AutofillProfile> profile =
      CreateAutofillProfileFromSpecifics(specifics);

  EXPECT_EQ("123 New St.",
            UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_LINE1)));
  EXPECT_EQ("Apt. 42", UTF16ToUTF8(profile->GetRawInfo(ADDRESS_HOME_LINE2)));
}

// Test that country names (used in the past for the field) get correctly parsed
// into country code.
TEST_F(AutofillProfileSyncUtilTest,
       CreateAutofillProfileFromSpecifics_CountryNameParsed) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  specifics.set_address_home_country("Germany");
  EXPECT_EQ("DE", UTF16ToUTF8(
                      CreateAutofillProfileFromSpecifics(specifics)->GetRawInfo(
                          ADDRESS_HOME_COUNTRY)));

  specifics.set_address_home_country("united states");
  EXPECT_EQ("US", UTF16ToUTF8(
                      CreateAutofillProfileFromSpecifics(specifics)->GetRawInfo(
                          ADDRESS_HOME_COUNTRY)));
}

// Tests that guid is returned as storage key.
TEST_F(AutofillProfileSyncUtilTest, GetStorageKeyFromAutofillProfile) {
  AutofillProfile profile(kGuid);

  EXPECT_EQ(kGuid, GetStorageKeyFromAutofillProfile(profile));
}

// Tests that guid is returned as storage key.
TEST_F(AutofillProfileSyncUtilTest, GetStorageKeyFromAutofillProfileSpecifics) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuid);

  EXPECT_EQ(kGuid, GetStorageKeyFromAutofillProfileSpecifics(specifics));
}

// Tests that empty string is returned for entry with invalid guid.
TEST_F(AutofillProfileSyncUtilTest,
       GetStorageKeyFromAutofillProfileSpecifics_Invalid) {
  AutofillProfileSpecifics specifics;
  specifics.set_guid(kGuidInvalid);

  EXPECT_EQ(std::string(),
            GetStorageKeyFromAutofillProfileSpecifics(specifics));
}

}  // namespace
}  // namespace autofill
