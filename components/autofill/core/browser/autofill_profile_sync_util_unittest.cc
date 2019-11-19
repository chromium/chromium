// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_profile_sync_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
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

const char kLocaleString[] = "en-US";
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

// Returns a profile with all fields set.  Contains identical data to the data
// returned from ConstructCompleteSpecifics().
AutofillProfile ConstructCompleteProfile() {
  AutofillProfile profile(kGuid, "https://www.example.com/");

  profile.set_use_count(7);
  profile.set_use_date(base::Time::FromTimeT(1423182152));

  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John K. Doe, Jr."));
  profile.SetRawInfo(NAME_FIRST, ASCIIToUTF16("John"));
  profile.SetRawInfo(NAME_MIDDLE, ASCIIToUTF16("K."));
  profile.SetRawInfo(NAME_LAST, ASCIIToUTF16("Doe"));

  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16("user@example.com"));
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1.800.555.1234"));

  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, ASCIIToUTF16("123 Fake St.\n"
                                                               "Apt. 42"));
  EXPECT_EQ(ASCIIToUTF16("123 Fake St."),
            profile.GetRawInfo(ADDRESS_HOME_LINE1));
  EXPECT_EQ(ASCIIToUTF16("Apt. 42"), profile.GetRawInfo(ADDRESS_HOME_LINE2));

  profile.SetRawInfo(COMPANY_NAME, ASCIIToUTF16("Google, Inc."));
  profile.SetRawInfo(ADDRESS_HOME_CITY, ASCIIToUTF16("Mountain View"));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, ASCIIToUTF16("94043"));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  profile.SetRawInfo(ADDRESS_HOME_SORTING_CODE, ASCIIToUTF16("CEDEX"));
  profile.SetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY,
                     ASCIIToUTF16("Santa Clara"));
  profile.set_language_code("en");
  profile.SetClientValidityFromBitfieldValue(1984);
  profile.set_is_client_validity_states_updated(true);

  return profile;
}

// Returns AutofillProfileSpecifics with all Autofill profile fields set.
// Contains identical data to the data returned from ConstructCompleteProfile().
AutofillProfileSpecifics ConstructCompleteSpecifics() {
  AutofillProfileSpecifics specifics;

  specifics.set_guid(kGuid);
  specifics.set_origin("https://www.example.com/");
  specifics.set_use_count(7);
  specifics.set_use_date(1423182152);

  specifics.add_name_first("John");
  specifics.add_name_middle("K.");
  specifics.add_name_last("Doe");
  specifics.add_name_full("John K. Doe, Jr.");

  specifics.add_email_address("user@example.com");

  specifics.add_phone_home_whole_number("1.800.555.1234");

  specifics.set_address_home_line1("123 Fake St.");
  specifics.set_address_home_line2("Apt. 42");
  specifics.set_address_home_street_address(
      "123 Fake St.\n"
      "Apt. 42");

  specifics.set_company_name("Google, Inc.");
  specifics.set_address_home_city("Mountain View");
  specifics.set_address_home_state("California");
  specifics.set_address_home_zip("94043");
  specifics.set_address_home_country("US");
  specifics.set_address_home_sorting_code("CEDEX");
  specifics.set_address_home_dependent_locality("Santa Clara");
  specifics.set_address_home_language_code("en");
  specifics.set_validity_state_bitfield(1984);
  specifics.set_is_client_validity_states_updated(true);

  return specifics;
}

class AutofillProfileSyncUtilTest : public testing::Test {
 public:
  AutofillProfileSyncUtilTest() {
    // Fix a time for implicitly constructed use_dates in AutofillProfile.
    test_clock_.SetNow(kJune2017);
    CountryNames::SetLocaleString(kLocaleString);
  }

 private:
  autofill::TestAutofillClock test_clock_;
};

// Ensure that all profile fields are able to be synced up from the client to
// the server.
TEST_F(AutofillProfileSyncUtilTest, CreateEntityDataFromAutofillProfile) {
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
  AutofillProfile profile(kGuid, std::string());
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

  AutofillProfile profile(kGuid, std::string());
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

  AutofillProfile profile(kGuid, std::string());
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
  CountryNames::SetLocaleString(kLocaleString);

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
  CountryNames::SetLocaleString(kLocaleString);

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
  AutofillProfile profile(kGuid, std::string());

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
