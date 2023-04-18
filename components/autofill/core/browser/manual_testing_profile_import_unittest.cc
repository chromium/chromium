// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/manual_testing_profile_import.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
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

class ManualTestingProfileImportTest : public testing::Test {
  void SetUp() override { ASSERT_TRUE(scoped_temp_dir.CreateUniqueTempDir()); }

 public:
  base::FilePath GetFilePath() const {
    return scoped_temp_dir.GetPath().Append(
        FILE_PATH_LITERAL("test_file.json"));
  }
  base::ScopedTempDir scoped_temp_dir;
};

// Tests that profiles are converted correctly.
TEST_F(ManualTestingProfileImportTest, LoadProfilesFromFile_Valid) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "source" : "localOrSyncable",
        "NAME_FULL" : "first last",
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last"
      },
      {
        "source" : "account",
        "initial_creator_id": 999,
        "ADDRESS_HOME_STREET_ADDRESS" : "street 123",
        "ADDRESS_HOME_STREET_NAME" : "street",
        "ADDRESS_HOME_HOUSE_NUMBER" : "123"
      }
    ]
  })");

  AutofillProfile expected_profile1(AutofillProfile::Source::kLocalOrSyncable);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"first last", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_FIRST, u"first", VerificationStatus::kObserved);
  expected_profile1.SetRawInfoWithVerificationStatus(
      NAME_LAST, u"last", VerificationStatus::kObserved);

  AutofillProfile expected_profile2(AutofillProfile::Source::kAccount);
  expected_profile2.set_initial_creator_id(999);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS, u"street 123",
      VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"street", VerificationStatus::kObserved);
  expected_profile2.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"123", VerificationStatus::kObserved);

  EXPECT_THAT(
      LoadProfilesFromFile(file_path),
      testing::Optional(testing::Pointwise(
          ProfilesCompareEqual(), {expected_profile1, expected_profile2})));
}

// Tests that conversion fails when the file doesn't exist
TEST_F(ManualTestingProfileImportTest,
       AutofillProfileFromJSON_Invalid_FailedToOpenFile) {
  EXPECT_FALSE(LoadProfilesFromFile(GetFilePath()).has_value());
}

// Tests that the conversion fails when the passed JSON file isn't a dictionary
// at its top level.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_Invalid_NotDictionary) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"([
      "first last"
    ])");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the passed JSON file isn't valid.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_Invalid_FailedToParseJSON) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"([
      "first last"
    })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when an unrecognized field type is present.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_Invalid_UnrecognizedType) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FULLLLL" : "..."
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the "source" has an unrecognized value.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_Invalid_UnrecognizedSource) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "source" : "invalid"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

// Tests that the conversion fails when the "initial_creator_id" has
// an invalid value.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_InvalidInitialCreatorId) {
  base::FilePath file_path1 =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_file_1.json"));
  base::WriteFile(file_path1, R"({
    "profiles" : [
      {
        "initial_creator_id" : "123"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path1).has_value());

  base::FilePath file_path2 =
      scoped_temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_file_2.json"));
  base::WriteFile(file_path2, R"({
    "profiles" : [
      {
        "initial_creator_id" : true
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path2).has_value());
}

// Tests that the conversion fails for non-fully structured profiles.
TEST_F(ManualTestingProfileImportTest,
       LoadProfilesFromFile_Invalid_NotFullyStructured) {
  base::FilePath file_path = GetFilePath();
  base::WriteFile(file_path, R"({
    "profiles" : [
      {
        "NAME_FIRST" : "first",
        "NAME_LAST" : "last"
      }
    ]
  })");
  EXPECT_FALSE(LoadProfilesFromFile(file_path).has_value());
}

}  // namespace autofill
