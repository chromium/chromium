// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_profile_import.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {

namespace {

// Matches two AutofillProfiles and expects that they `Compare()` equal. This
// means that their values and verification statuses match for every field type,
// but their GUID, usage data, etc might differ.
MATCHER(ProfilesCompareEqual, "") {
  return std::get<0>(arg).Compare(std::get<1>(arg)) == 0;
}

}  // namespace

// Tests that profiles are converted correctly.
TEST(ManualTestingProfileImportTest, AutofillProfilesFromJSON_Valid) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"({
    "profiles" : [
      {
        "source" : "localOrSyncable",
        "NAME_FULL" : "first last",
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last"
      },
      {
        "source" : "account",
        "ADDRESS_HOME_STREET_ADDRESS" : "street 123",
        "ADDRESS_HOME_STREET_NAME" : "street",
        "ADDRESS_HOME_HOUSE_NUMBER" : "123"
      }
    ]
  })");
  ASSERT_TRUE(json);

  AutofillProfile expected_profile1(AutofillProfile::Source::kLocalOrSyncable);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"first last", VerificationStatus::kUserVerified);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"first", VerificationStatus::kUserVerified);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"last", VerificationStatus::kUserVerified);

  AutofillProfile expected_profile2(AutofillProfile::Source::kAccount);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"street 123",
      VerificationStatus::kUserVerified);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"street", VerificationStatus::kUserVerified);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123", VerificationStatus::kUserVerified);

  base::expected<std::vector<AutofillProfile>, std::string> profiles =
      AutofillProfilesFromJSON(*json);
  ASSERT_TRUE(profiles.has_value()) << profiles.error();
  EXPECT_THAT(*profiles,
              testing::Pointwise(ProfilesCompareEqual(),
                                 {expected_profile1, expected_profile2}));
}

// Tests that the conversion fails when an unrecognized field type is present.
TEST(ManualTestingProfileImportTest,
     AutofillProfilesFromJSON_UnrecognizedType) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"({
    "profiles" : [
      {
        "NAME_FULLLLL" : "..."
      }
    ]
  })");
  ASSERT_TRUE(json);
  EXPECT_FALSE(AutofillProfilesFromJSON(*json).has_value());
}

// Tests that the conversion fails when the "source" has an unrecognized value.
TEST(ManualTestingProfileImportTest,
     AutofillProfilesFromJSON_UnrecognizedSource) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"({
    "profiles" : [
      {
        "source" : "invalid"
      }
    ]
  })");
  ASSERT_TRUE(json);
  EXPECT_FALSE(AutofillProfilesFromJSON(*json).has_value());
}

// Tests that the conversion fails for non-fully structured profiles.
TEST(ManualTestingProfileImportTest,
     AutofillProfilesFromJSON_NotFullyStructured) {
  absl::optional<base::Value> json = base::JSONReader::Read(R"({
    "profiles" : [
      {
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last"
      }
    ]
  })");
  ASSERT_TRUE(json);
  EXPECT_FALSE(AutofillProfilesFromJSON(*json).has_value());
}

}  // namespace autofill
