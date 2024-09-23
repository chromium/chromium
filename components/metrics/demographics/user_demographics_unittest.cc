// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/demographics/user_demographics.h"

#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace metrics {

namespace {

// Gets the now time used for testing demographics.
base::Time GetNowTime() {
  constexpr char kNowTimeInStringFormat[] = "22 Jul 2019 00:00:00 UDT";

  base::Time now;
  bool result = base::Time::FromString(kNowTimeInStringFormat, &now);
  DCHECK(result);
  return now;
}

}  // namespace

TEST(UserDemographicsTest, UserDemographicsResult_ForValue) {
  int user_birth_year = 1982;
  UserDemographicsProto_Gender user_gender = UserDemographicsProto::GENDER_MALE;

  UserDemographics user_demographics;
  user_demographics.birth_year = user_birth_year;
  user_demographics.gender = user_gender;
  UserDemographicsResult user_demographics_result =
      UserDemographicsResult::ForValue(std::move(user_demographics));

  EXPECT_TRUE(user_demographics_result.IsSuccess());
  EXPECT_EQ(UserDemographicsStatus::kSuccess,
            user_demographics_result.status());
  EXPECT_EQ(user_birth_year, user_demographics_result.value().birth_year);
  EXPECT_EQ(user_gender, user_demographics_result.value().gender);
}

TEST(UserDemographicsTest, UserDemographicsResult_ForStatus) {
  UserDemographicsStatus error_status =
      UserDemographicsStatus::kIneligibleDemographicsData;
  UserDemographicsResult user_demographics_result =
      UserDemographicsResult::ForStatus(error_status);

  EXPECT_FALSE(user_demographics_result.IsSuccess());
  EXPECT_EQ(error_status, user_demographics_result.status());
}

class UserDemographicsPrefsTest : public testing::Test {
 protected:
  UserDemographicsPrefsTest() {
    RegisterDemographicsLocalStatePrefs(pref_service_.registry());
    RegisterDemographicsProfilePrefs(pref_service_.registry());
  }

  void SetDemographics(int birth_year, UserDemographicsProto::Gender gender) {
    SetDemographicsImpl(kSyncDemographicsPrefName, birth_year, gender);
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetOsDemographics(int birth_year, UserDemographicsProto::Gender gender) {
    SetDemographicsImpl(kSyncOsDemographicsPrefName, birth_year, gender);
  }
#endif

  PrefService* GetLocalState() { return &pref_service_; }
  PrefService* GetProfilePrefs() { return &pref_service_; }

 private:
  void SetDemographicsImpl(const std::string& pref_name,
                           int birth_year,
                           UserDemographicsProto::Gender gender) {
    base::Value::Dict dict;
    dict.Set(kSyncDemographicsBirthYearPath, birth_year);
    dict.Set(kSyncDemographicsGenderPath, static_cast<int>(gender));
    GetProfilePrefs()->SetDict(pref_name, std::move(dict));
  }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
};

TEST_F(UserDemographicsPrefsTest, ReadDemographicsWithRandomOffset) {
  int user_demographics_birth_year = 1983;
  UserDemographicsProto_Gender user_demographics_gender =
      UserDemographicsProto::GENDER_MALE;

  // Set user demographic prefs.
  SetDemographics(user_demographics_birth_year, user_demographics_gender);

  int provided_birth_year;
  {
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
    EXPECT_EQ(user_demographics_gender, demographics_result.value().gender);
    // Verify that the provided birth year is within the range.
    provided_birth_year = demographics_result.value().birth_year;
    int delta = provided_birth_year - user_demographics_birth_year;
    EXPECT_LE(delta, kUserDemographicsBirthYearNoiseOffsetRange);
    EXPECT_GE(delta, -kUserDemographicsBirthYearNoiseOffsetRange);
  }

  // Verify that the offset is cached and that the randomized birth year is the
  // same when doing more that one read of the birth year.
  {
    ASSERT_TRUE(
        GetLocalState()->HasPrefPath(kUserDemographicsBirthYearOffsetPrefName));
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
    EXPECT_EQ(provided_birth_year, demographics_result.value().birth_year);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(UserDemographicsPrefsTest, ReadOsDemographicsWithRandomOffset) {
  int user_demographics_birth_year = 1983;
  UserDemographicsProto_Gender user_demographics_gender =
      UserDemographicsProto::GENDER_MALE;

  // Set user demographic prefs.
  SetOsDemographics(user_demographics_birth_year, user_demographics_gender);

  int provided_birth_year;
  {
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
    EXPECT_EQ(user_demographics_gender, demographics_result.value().gender);
    // Verify that the provided birth year is within the range.
    provided_birth_year = demographics_result.value().birth_year;
    int delta = provided_birth_year - user_demographics_birth_year;
    EXPECT_LE(delta, kUserDemographicsBirthYearNoiseOffsetRange);
    EXPECT_GE(delta, -kUserDemographicsBirthYearNoiseOffsetRange);
  }

  // Verify that the offset is cached and that the randomized birth year is the
  // same when doing more that one read of the birth year.
  {
    ASSERT_TRUE(
        GetLocalState()->HasPrefPath(kUserDemographicsBirthYearOffsetPrefName));
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
    EXPECT_EQ(provided_birth_year, demographics_result.value().birth_year);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST_F(UserDemographicsPrefsTest, ReadAndClearUserDemographicPreferences) {
  // Verify demographic prefs are not available when there is nothing set.
  ASSERT_FALSE(GetUserNoisedBirthYearAndGenderFromPrefs(
                   GetNowTime(), GetLocalState(), GetProfilePrefs())
                   .IsSuccess());

  // Set demographic prefs directly from the pref service interface because
  // demographic prefs will only be set on the server-side. The SyncPrefs
  // interface cannot set demographic prefs.
  SetDemographics(1983, UserDemographicsProto::GENDER_FEMALE);

  // Set birth year noise offset to not have it randomized.
  GetProfilePrefs()->SetInteger(kUserDemographicsBirthYearOffsetPrefName, 2);

  // Verify that demographics are provided.
  {
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
  }

  ClearDemographicsPrefs(GetProfilePrefs());

  // Verify that demographics are not provided and kSyncDemographics is cleared.
  // Note that we retain k*DemographicsBirthYearOffset. If the user resumes
  // syncing, causing these prefs to be recreated, we don't want them to start
  // reporting a different randomized birth year as this could narrow down or
  // even reveal their true birth year.
  EXPECT_FALSE(GetUserNoisedBirthYearAndGenderFromPrefs(
                   GetNowTime(), GetLocalState(), GetProfilePrefs())
                   .IsSuccess());
  EXPECT_FALSE(GetProfilePrefs()->HasPrefPath(kSyncDemographicsPrefName));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(GetProfilePrefs()->HasPrefPath(kSyncOsDemographicsPrefName));
#endif
  EXPECT_TRUE(
      GetLocalState()->HasPrefPath(kUserDemographicsBirthYearOffsetPrefName));
  // Deprecated offset is not created if it does not already exist.
  EXPECT_FALSE(GetProfilePrefs()->HasPrefPath(
      kDeprecatedDemographicsBirthYearOffsetPrefName));
}

TEST_F(UserDemographicsPrefsTest, ReadAndClearDeprecatedOffsetPref) {
  // Verify demographic prefs are not available when there is nothing set.
  ASSERT_FALSE(GetUserNoisedBirthYearAndGenderFromPrefs(
                   GetNowTime(), GetLocalState(), GetProfilePrefs())
                   .IsSuccess());

  // Set demographic prefs directly from the pref service interface because
  // demographic prefs will only be set on the server-side. The SyncPrefs
  // interface cannot set demographic prefs.
  SetDemographics(1983, UserDemographicsProto::GENDER_FEMALE);

  // Set deprecated birth year noise offset in the UserPrefs
  GetProfilePrefs()->SetInteger(kDeprecatedDemographicsBirthYearOffsetPrefName,
                                2);

  // Verify that demographics are provided.
  {
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
  }

  // Offset if migrated to new pref.
  EXPECT_EQ(
      2, GetLocalState()->GetInteger(kUserDemographicsBirthYearOffsetPrefName));
  // TODO(crbug.com/40240008): clear/remove deprecated pref after 2023/09
  EXPECT_TRUE(GetProfilePrefs()->HasPrefPath(
      kDeprecatedDemographicsBirthYearOffsetPrefName));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(UserDemographicsPrefsTest, ChromeOsAsh) {
  // Verify demographic prefs are not available when there is nothing set.
  ASSERT_FALSE(GetUserNoisedBirthYearAndGenderFromPrefs(
                   GetNowTime(), GetLocalState(), GetProfilePrefs())
                   .IsSuccess());

  // Set OS demographic prefs directly within the pref service interface.
  SetOsDemographics(1983, UserDemographicsProto::GENDER_FEMALE);

  // Set  birth year noise offset in the UserPrefs
  GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName, 2);

  // Verify that demographics are provided.
  {
    UserDemographicsResult demographics_result =
        GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                                 GetProfilePrefs());
    ASSERT_TRUE(demographics_result.IsSuccess());
  }

  EXPECT_FALSE(GetProfilePrefs()->HasPrefPath(kSyncDemographicsPrefName));
  EXPECT_TRUE(GetProfilePrefs()->HasPrefPath(kSyncOsDemographicsPrefName));
  EXPECT_TRUE(
      GetLocalState()->HasPrefPath(kUserDemographicsBirthYearOffsetPrefName));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

struct DemographicsTestParam {
  // Birth year of the user.
  int birth_year = kUserDemographicsBirthYearDefaultValue;

  // Non-random offset to apply to |birth_year| as noise.
  int birth_year_offset = kUserDemographicsBirthYearNoiseOffsetDefaultValue;

  // Gender of the user.
  UserDemographicsProto_Gender gender = kUserDemographicGenderDefaultEnumValue;

  // Status of the retrieval of demographics.
  UserDemographicsStatus status = UserDemographicsStatus::kMaxValue;
};

// Extend UserDemographicsPrefsTest fixture for parameterized tests on
// demographics.
class UserDemographicsPrefsTestWithParam
    : public UserDemographicsPrefsTest,
      public testing::WithParamInterface<DemographicsTestParam> {};

TEST_P(UserDemographicsPrefsTestWithParam, ReadDemographics_OffsetIsNotRandom) {
  DemographicsTestParam param = GetParam();

  // Set user demographic prefs.
  SetDemographics(param.birth_year, param.gender);

  // Set birth year noise offset to not have it randomized.
  GetLocalState()->SetInteger(kUserDemographicsBirthYearOffsetPrefName,
                              param.birth_year_offset);

  // Verify provided demographics for the different parameterized test cases.
  UserDemographicsResult demographics_result =
      GetUserNoisedBirthYearAndGenderFromPrefs(GetNowTime(), GetLocalState(),
                                               GetProfilePrefs());
  if (param.status == UserDemographicsStatus::kSuccess) {
    ASSERT_TRUE(demographics_result.IsSuccess());
    EXPECT_EQ(param.birth_year + param.birth_year_offset,
              demographics_result.value().birth_year);
    EXPECT_EQ(param.gender, demographics_result.value().gender);
  } else {
    ASSERT_FALSE(demographics_result.IsSuccess());
    EXPECT_EQ(param.status, demographics_result.status());
  }
}

// Test suite composed of different test cases of getting user demographics.
// The now time in each test case is "22 Jul 2019 00:00:00 UDT" which falls into
// the year bucket of 2018. Users need at most a |birth_year| +
// |birth_year_offset| of 1998 to be able to provide demographics.
INSTANTIATE_TEST_SUITE_P(
    All,
    UserDemographicsPrefsTestWithParam,
    ::testing::Values(
        // Test where birth year should not be provided because |birth_year| + 2
        // > 1998.
        DemographicsTestParam{
            /*birth_year=*/1997,
            /*birth_year_offset=*/2,
            /*gender=*/UserDemographicsProto::GENDER_FEMALE,
            /*status=*/UserDemographicsStatus::kIneligibleDemographicsData},
        // Test where birth year should not be provided because |birth_year| - 2
        // > 1998.
        DemographicsTestParam{
            /*birth_year=*/2001,
            /*birth_year_offset=*/-2,
            /*gender=*/UserDemographicsProto::GENDER_FEMALE,
            /*status=*/UserDemographicsStatus::kIneligibleDemographicsData},
        // Test where birth year should not be provided because age of user is
        // |kUserDemographicsMaxAge| + 1, which is over the max age.
        DemographicsTestParam{
            /*birth_year=*/1933,
            /*birth_year_offset=*/0,
            /*gender=*/UserDemographicsProto::GENDER_FEMALE,
            /*status=*/UserDemographicsStatus::kIneligibleDemographicsData},
        // Test where gender should not be provided because it has a low
        // population that can have their privacy compromised because of high
        // entropy.
        DemographicsTestParam{
            /*birth_year=*/1986,
            /*birth_year_offset=*/0,
            /*gender=*/UserDemographicsProto::GENDER_CUSTOM_OR_OTHER,
            /*status=*/UserDemographicsStatus::kIneligibleDemographicsData},
        // Test where birth year can be provided because |birth_year| + 2 ==
        // 1998.
        DemographicsTestParam{/*birth_year=*/1996,
                              /*birth_year_offset=*/2,
                              /*gender=*/UserDemographicsProto::GENDER_FEMALE,
                              /*status=*/UserDemographicsStatus::kSuccess},
        // Test where birth year can be provided because |birth_year| - 2 ==
        // 1998.
        DemographicsTestParam{/*birth_year=*/2000,
                              /*birth_year_offset=*/-2,
                              /*gender=*/UserDemographicsProto::GENDER_MALE,
                              /*status=*/UserDemographicsStatus::kSuccess},
        // Test where birth year can be provided because |birth_year| + 2 <
        // 1998.
        DemographicsTestParam{/*birth_year=*/1995,
                              /*birth_year_offset=*/2,
                              /*gender=*/UserDemographicsProto::GENDER_FEMALE,
                              /*status=*/UserDemographicsStatus::kSuccess},
        // Test where birth year can be provided because |birth_year| - 2 <
        // 1998.
        DemographicsTestParam{/*birth_year=*/1999,
                              /*birth_year_offset=*/-2,
                              /*gender=*/UserDemographicsProto::GENDER_MALE,
                              /*status=*/UserDemographicsStatus::kSuccess},
        // Test where gender can be provided because it is part of a large
        // population with a low entropy.
        DemographicsTestParam{/*birth_year=*/1986,
                              /*birth_year_offset=*/0,
                              /*gender=*/UserDemographicsProto::GENDER_FEMALE,
                              /*status=*/UserDemographicsStatus::kSuccess},
        // Test where gender can be provided because it is part of a large
        // population with a low entropy.
        DemographicsTestParam{/*birth_year=*/1986,
                              /*birth_year_offset=*/0,
                              /*gender=*/UserDemographicsProto::GENDER_MALE,
                              /*status=*/UserDemographicsStatus::kSuccess}));

}  // namespace metrics
