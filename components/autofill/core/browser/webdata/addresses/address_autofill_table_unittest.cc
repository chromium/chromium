// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/addresses/address_autofill_table.h"

#include <memory>
#include <optional>
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
// record types.
class AddressAutofillTableProfileTest
    : public AddressAutofillTableTest,
      public testing::WithParamInterface<AutofillProfile::RecordType> {
 public:
  AutofillProfile::RecordType record_type() const { return GetParam(); }

  // Creates an `AutofillProfile` of `record_type()`.
  AutofillProfile CreateAutofillProfile() const {
    return AutofillProfile(record_type(), AddressCountryCode("ES"));
  }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AddressAutofillTableProfileTest,
    testing::ValuesIn({AutofillProfile::RecordType::kLocalOrSyncable,
                       AutofillProfile::RecordType::kAccount}));

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
  std::optional<AutofillProfile> db_profile =
      table_.GetAutofillProfile(home_profile.guid());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(home_profile, *db_profile);

  // Remove the profile and expect that no profiles remain.
  EXPECT_TRUE(table_.RemoveAutofillProfile(home_profile.guid()));
  std::vector<AutofillProfile> profiles;
  EXPECT_TRUE(table_.GetAutofillProfiles({record_type()}, profiles));
  EXPECT_TRUE(profiles.empty());
}

// Tests that `GetAutofillProfiles(record_type, profiles)` clears `profiles` and
// only returns profiles from the correct `record_type`.
// Not part of the `AddressAutofillTableProfileTest` fixture, as it doesn't
// benefit from parameterization on the `record_type()`.
TEST_F(AddressAutofillTableTest, GetAutofillProfiles) {
  AutofillProfile local_profile(AutofillProfile::RecordType::kLocalOrSyncable,
                                AddressCountryCode("ES"));
  AutofillProfile account_profile(AutofillProfile::RecordType::kAccount,
                                  AddressCountryCode("ES"));
  EXPECT_TRUE(table_.AddAutofillProfile(local_profile));
  EXPECT_TRUE(table_.AddAutofillProfile(account_profile));

  std::vector<AutofillProfile> profiles;
  EXPECT_TRUE(table_.GetAutofillProfiles(
      {AutofillProfile::RecordType::kLocalOrSyncable}, profiles));
  EXPECT_THAT(profiles, UnorderedElementsAre(local_profile));
  EXPECT_TRUE(table_.GetAutofillProfiles(
      {AutofillProfile::RecordType::kAccount}, profiles));
  EXPECT_THAT(profiles, UnorderedElementsAre(account_profile));
  EXPECT_TRUE(table_.GetAutofillProfiles(
      DenseSet<AutofillProfile::RecordType>::all(), profiles));
  EXPECT_THAT(profiles, UnorderedElementsAre(local_profile, account_profile));
}

// Tests that `RemoveAllAutofillProfiles()` clears all profiles of the given
// record type.
TEST_P(AddressAutofillTableProfileTest, RemoveAllAutofillProfiles) {
  ASSERT_TRUE(table_.AddAutofillProfile(
      AutofillProfile(AutofillProfile::RecordType::kLocalOrSyncable,
                      i18n_model_definition::kLegacyHierarchyCountryCode)));
  ASSERT_TRUE(table_.AddAutofillProfile(
      AutofillProfile(AutofillProfile::RecordType::kAccount,
                      i18n_model_definition::kLegacyHierarchyCountryCode)));

  EXPECT_TRUE(table_.RemoveAllAutofillProfiles({record_type()}));

  // Expect that the profiles from `record_type()` are gone.
  std::vector<AutofillProfile> profiles;
  ASSERT_TRUE(table_.GetAutofillProfiles({record_type()}, profiles));
  EXPECT_TRUE(profiles.empty());

  // Expect that the profile from the opposite record_type remains.
  const auto other_record_type =
      record_type() == AutofillProfile::RecordType::kAccount
          ? AutofillProfile::RecordType::kLocalOrSyncable
          : AutofillProfile::RecordType::kAccount;
  ASSERT_TRUE(table_.GetAutofillProfiles({other_record_type}, profiles));
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
  profile = *table_.GetAutofillProfile(profile.guid());
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
  profile = *table_.GetAutofillProfile(profile.guid());
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
  profile = *table_.GetAutofillProfile(profile.guid());
  EXPECT_EQ(profile.use_date(1), initial_use_date);
  EXPECT_FALSE(profile.use_date(2).has_value());
  EXPECT_FALSE(profile.use_date(3).has_value());

  profile.RecordUseDate(initial_use_date + base::Days(1));
  profile.RecordUseDate(initial_use_date + base::Days(2));
  table_.UpdateAutofillProfile(profile);
  profile = *table_.GetAutofillProfile(profile.guid());
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
  std::optional<AutofillProfile> db_profile =
      table_.GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  table_.UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_.GetAutofillProfile(profile.guid());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
}

}  // namespace autofill
