// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::Time;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace autofill {

class AddressAutofillTableTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_.AddTable(&table_);
    ASSERT_EQ(sql::INIT_OK,
              db_.Init(temp_dir_.GetPath().AppendASCII("TestWebDatabase")));
  }

  base::ScopedTempDir temp_dir_;
  AddressAutofillTable table_;
  WebDatabase db_;
};

// Tests for the AutofillProfil CRUD interface are tested with both profile
// sources.
class AddressAutofillTableProfileTest
    : public AddressAutofillTableTest,
      public testing::WithParamInterface<AutofillProfile::Source> {
 public:
  void SetUp() override {
    AddressAutofillTableTest::SetUp();
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForLandmark,
         features::kAutofillEnableSupportForBetweenStreets,
         features::kAutofillEnableSupportForAdminLevel2,
         features::kAutofillEnableSupportForAddressOverflow,
         features::kAutofillEnableSupportForAddressOverflowAndLandmark,
         features::kAutofillEnableSupportForBetweenStreetsOrLandmark},
        {});
  }
  AutofillProfile::Source profile_source() const { return GetParam(); }

  // Creates an `AutofillProfile` with `profile_source()` as its source.
  AutofillProfile CreateAutofillProfile() const {
    return AutofillProfile(profile_source(), AddressCountryCode("ES"));
  }

  // Depending on the `profile_source()`, the AutofillProfiles are stored in a
  // different master table.
  std::string_view GetProfileTable() const {
    return profile_source() == AutofillProfile::Source::kLocalOrSyncable
               ? "local_addresses"
               : "contact_info";
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AddressAutofillTableProfileTest,
    testing::ValuesIn({AutofillProfile::Source::kLocalOrSyncable,
                       AutofillProfile::Source::kAccount}));

// Tests reading/writing name, email, company, address and phone number
// information.
TEST_P(AddressAutofillTableProfileTest, AutofillProfile) {
  AutofillProfile home_profile = CreateAutofillProfile();

  home_profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Q.",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"Agent",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"007",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Agent 007 Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"John Q. Agent 007 Smith", VerificationStatus::kObserved);

  home_profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  home_profile.SetRawInfo(COMPANY_NAME, u"Google");

  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"Street Name between streets House Number Premise APT 10 Floor 2 "
      u"Landmark",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STREET_LOCATION,
                                                u"Street Name House Number",
                                                VerificationStatus::kFormatted);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Street Name", VerificationStatus::kFormatted);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                                u"Dependent Locality",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"City",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"State",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SORTING_CODE,
                                                u"Sorting Code",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"ZIP",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"DE",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"House Number",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"APT 10 Floor 2",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                                VerificationStatus::kParsed);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                                VerificationStatus::kParsed);
  ASSERT_EQ(home_profile.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LANDMARK, u"Landmark", VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW,
                                                u"Andar 1, Apto. 12",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS,
                                                u"between streets",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca", VerificationStatus::kObserved);

  home_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  home_profile.set_language_code("en");

  // Add the profile to the table.
  EXPECT_TRUE(table_.AddAutofillProfile(home_profile));

  // Get the 'Home' profile from the table.
  std::unique_ptr<AutofillProfile> db_profile =
      table_.GetAutofillProfile(home_profile.guid(), home_profile.source());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(home_profile, *db_profile);

  // Remove the profile and expect that no profiles remain.
  EXPECT_TRUE(
      table_.RemoveAutofillProfile(home_profile.guid(), profile_source()));
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  EXPECT_TRUE(table_.GetAutofillProfiles(profile_source(), profiles));
  EXPECT_TRUE(profiles.empty());
}

// Tests that `GetAutofillProfiles(source, profiles)` clears `profiles` and
// only returns profiles from the correct `source`.
// Not part of the `AddressAutofillTableProfileTest` fixture, as it doesn't
// benefit from parameterization on the `profile_source()`.
TEST_F(AddressAutofillTableTest, GetAutofillProfiles) {
  AutofillProfile local_profile(AutofillProfile::Source::kLocalOrSyncable,
                                AddressCountryCode("ES"));
  AutofillProfile account_profile(AutofillProfile::Source::kAccount,
                                  AddressCountryCode("ES"));
  EXPECT_TRUE(table_.AddAutofillProfile(local_profile));
  EXPECT_TRUE(table_.AddAutofillProfile(account_profile));

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  EXPECT_TRUE(table_.GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, profiles));
  EXPECT_THAT(profiles, ElementsAre(testing::Pointee(local_profile)));
  EXPECT_TRUE(
      table_.GetAutofillProfiles(AutofillProfile::Source::kAccount, profiles));
  EXPECT_THAT(profiles, ElementsAre(testing::Pointee(account_profile)));
}

// Tests that `RemoveAllAutofillProfiles()` clears all profiles of the given
// source.
TEST_P(AddressAutofillTableProfileTest, RemoveAllAutofillProfiles) {
  ASSERT_TRUE(table_.AddAutofillProfile(
      AutofillProfile(AutofillProfile::Source::kLocalOrSyncable,
                      i18n_model_definition::kLegacyHierarchyCountryCode)));
  ASSERT_TRUE(table_.AddAutofillProfile(
      AutofillProfile(AutofillProfile::Source::kAccount,
                      i18n_model_definition::kLegacyHierarchyCountryCode)));

  EXPECT_TRUE(table_.RemoveAllAutofillProfiles(profile_source()));

  // Expect that the profiles from `profile_source()` are gone.
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  ASSERT_TRUE(table_.GetAutofillProfiles(profile_source(), profiles));
  EXPECT_TRUE(profiles.empty());

  // Expect that the profile from the opposite source remains.
  const auto other_source =
      profile_source() == AutofillProfile::Source::kAccount
          ? AutofillProfile::Source::kLocalOrSyncable
          : AutofillProfile::Source::kAccount;
  ASSERT_TRUE(table_.GetAutofillProfiles(other_source, profiles));
  EXPECT_EQ(profiles.size(), 1u);
}

// Tests that `ProfileTokenQuality` observations are read and written.
TEST_P(AddressAutofillTableProfileTest, ProfileTokenQuality) {
  AutofillProfile profile = CreateAutofillProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted,
                      ProfileTokenQualityTestApi::FormSignatureHash(12));

  // Add
  table_.AddAutofillProfile(profile);
  profile = *table_.GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQuality::ObservationType::kAccepted));
  EXPECT_THAT(
      test_api(profile.token_quality()).GetHashesForStoredType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQualityTestApi::FormSignatureHash(12)));

  // Update
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kEditedFallback,
                      ProfileTokenQualityTestApi::FormSignatureHash(21));
  table_.UpdateAutofillProfile(profile);
  profile = *table_.GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(
          ProfileTokenQuality::ObservationType::kAccepted,
          ProfileTokenQuality::ObservationType::kEditedFallback));
  EXPECT_THAT(
      test_api(profile.token_quality()).GetHashesForStoredType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQualityTestApi::FormSignatureHash(12),
                           ProfileTokenQualityTestApi::FormSignatureHash(21)));
}

// Tests that last use dates are persisted, if present.
TEST_P(AddressAutofillTableProfileTest, UseDates) {
  base::test::ScopedFeatureList feature{
      features::kAutofillTrackMultipleUseDates};
  AutofillProfile profile = CreateAutofillProfile();
  // Since the table stores time_ts, microseconds get lost in conversion.
  const base::Time initial_use_date =
      base::Time::FromTimeT(profile.use_date().ToTimeT());
  ASSERT_FALSE(profile.use_date(2).has_value());
  ASSERT_FALSE(profile.use_date(3).has_value());

  table_.AddAutofillProfile(profile);
  profile = *table_.GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_EQ(profile.use_date(1), initial_use_date);
  EXPECT_FALSE(profile.use_date(2).has_value());
  EXPECT_FALSE(profile.use_date(3).has_value());

  profile.RecordUseDate(initial_use_date + base::Days(1));
  profile.RecordUseDate(initial_use_date + base::Days(2));
  table_.UpdateAutofillProfile(profile);
  profile = *table_.GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_EQ(profile.use_date(1), initial_use_date + base::Days(2));
  EXPECT_EQ(profile.use_date(2), initial_use_date + base::Days(1));
  EXPECT_EQ(profile.use_date(3), initial_use_date);
}

TEST_P(AddressAutofillTableProfileTest, UpdateAutofillProfile) {
  // Add a profile to the db.
  AutofillProfile profile = CreateAutofillProfile();
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_MIDDLE, u"Q.");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"ES");
  profile.SetRawInfo(ADDRESS_HOME_OVERFLOW, u"Andar 1, Apto. 12");
  profile.SetRawInfo(ADDRESS_HOME_LANDMARK, u"Landmark");
  profile.SetRawInfo(ADDRESS_HOME_BETWEEN_STREETS, u"Marcos y Oliva");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  profile.set_language_code("en");
  profile.FinalizeAfterImport();
  table_.AddAutofillProfile(profile);

  // Get the profile.
  std::unique_ptr<AutofillProfile> db_profile =
      table_.GetAutofillProfile(profile.guid(), profile.source());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  table_.UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_.GetAutofillProfile(profile.guid(), profile.source());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
}

TEST_F(AddressAutofillTableTest, RemoveAutofillDataModifiedBetween) {
  // Populate the address tables.
  ASSERT_TRUE(db_.GetSQLConnection()->Execute(
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000000', 3, 'first name0');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000001', 3, 'first name1');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000002', 3, 'first name2');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000003', 3, 'first name3');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', 51);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000004', 3, 'first name4');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 61);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000005', 3, 'first name5');"));

  // Remove all entries modified in the bounded time range [17,41).
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  table_.RemoveAutofillDataModifiedBetween(Time::FromTimeT(17),
                                           Time::FromTimeT(41), profiles);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002", profiles[1]->guid());

  // Make sure that only the expected profiles are still present.
  sql::Statement s_autofill_profiles_bounded(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profiles_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(51, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(61, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_bounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_bounded(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profile_names_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name0", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name3", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name4", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name5", s_autofill_profile_names_bounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_bounded.Step());

  // Remove all entries modified on or after time 51 (unbounded range).
  table_.RemoveAutofillDataModifiedBetween(Time::FromTimeT(51), Time(),
                                           profiles);
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000004", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000005", profiles[1]->guid());

  // Make sure that only the expected profiles are still present.
  sql::Statement s_autofill_profiles_unbounded(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profiles_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_unbounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_unbounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_unbounded(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("first name0", s_autofill_profile_names_unbounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("first name3", s_autofill_profile_names_unbounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_unbounded.Step());

  // Remove all remaining entries.
  table_.RemoveAutofillDataModifiedBetween(Time(), Time(), profiles);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003", profiles[1]->guid());

  // Make sure there are no profiles remaining.
  sql::Statement s_autofill_profiles_empty(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses"));
  ASSERT_TRUE(s_autofill_profiles_empty.is_valid());
  EXPECT_FALSE(s_autofill_profiles_empty.Step());

  // Make sure there are no profile names remaining.
  sql::Statement s_autofill_profile_names_empty(
      db_.GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens"));
  ASSERT_TRUE(s_autofill_profile_names_empty.is_valid());
  EXPECT_FALSE(s_autofill_profile_names_empty.Step());
}

}  // namespace autofill
