// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/field_trial_internals_utils.h"

#include <string_view>

#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/variations/hashing.h"
#include "components/variations/pref_names.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {
using testing::ElementsAre;
using testing::Pair;
using variations::HashNameAsHexString;
using ::variations::prefs::kVariationsForcedFieldTrials;
using ::variations::prefs::kVariationsForcedTrialExpiration;
using ::variations::prefs::kVariationsForcedTrialStarts;

variations::StudyGroupNames MakeStudy(std::string_view name) {
  variations::StudyGroupNames study;
  study.name = name;
  study.groups.push_back("On");
  study.groups.push_back("Off");
  return study;
}

class FieldTrialInternalsUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    RegisterFieldTrialInternalsPrefs(*pref_service_.registry());
    feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }

  PrefService& prefs() { return pref_service_; }

  std::string TrialState() const {
    std::string trials;
    base::FieldTrialList::AllStatesToString(&trials);
    return trials;
  }
  void ClearFieldTrials() {
    feature_list_.Reset();
    feature_list_.InitWithEmptyFeatureAndFieldTrialLists();
  }
  void SimulateRestart() {
    ClearFieldTrials();
    ForceTrialsAtStartup(prefs());
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FieldTrialInternalsUtilsTest, ForceTrialsAtStartupHasTrialOverride) {
  prefs().SetString(kVariationsForcedFieldTrials, "Blue/On");
  prefs().SetTime(kVariationsForcedTrialExpiration,
                  base::Time::Now() + base::Minutes(1));
  prefs().SetInteger(kVariationsForcedTrialStarts, 0);
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "Blue/On");
  base::FieldTrial* trial = base::FieldTrialList::Find("Blue");
  ASSERT_TRUE(trial);
  EXPECT_TRUE(trial->IsOverridden());
}

TEST_F(FieldTrialInternalsUtilsTest, ForceTrialsAtStartupExpiredByTime) {
  prefs().SetString(kVariationsForcedFieldTrials, "Blue/On");
  prefs().SetTime(kVariationsForcedTrialExpiration,
                  base::Time::Now() - base::Minutes(1));
  prefs().SetInteger(kVariationsForcedTrialStarts, 0);
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "");
}

TEST_F(FieldTrialInternalsUtilsTest,
       ForceTrialsAtStartupExpiredByInvalidExpiryTime) {
  prefs().SetString(kVariationsForcedFieldTrials, "Blue/On");
  prefs().SetTime(
      kVariationsForcedTrialExpiration,
      base::Time::Now() + base::Minutes(1) + kManualForceFieldTrialDuration);
  prefs().SetInteger(kVariationsForcedTrialStarts, 0);
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "");
}

TEST_F(FieldTrialInternalsUtilsTest,
       ForceTrialsAtStartupExpiredByRestartCount) {
  prefs().SetString(kVariationsForcedFieldTrials, "Blue/On");
  prefs().SetTime(kVariationsForcedTrialExpiration,
                  base::Time::Now() + base::Minutes(1));
  prefs().SetInteger(kVariationsForcedTrialStarts, 4);
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "");
  EXPECT_EQ(prefs().GetTime(kVariationsForcedTrialExpiration), base::Time());
}

TEST_F(FieldTrialInternalsUtilsTest, ForceTrialsAtStartupNoPrefsStored) {
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "");
}

TEST_F(FieldTrialInternalsUtilsTest, SetTemporaryTrialOverridesOneGroup) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);

  EXPECT_EQ(prefs().GetString(kVariationsForcedFieldTrials), "Blue/On");
  EXPECT_EQ(prefs().GetTime(kVariationsForcedTrialExpiration),
            base::Time::Now() + kManualForceFieldTrialDuration);
  EXPECT_EQ(prefs().GetInteger(kVariationsForcedTrialStarts), 0);
  // Trials aren't updated until after restart.
  EXPECT_EQ(TrialState(), "");
}

TEST_F(FieldTrialInternalsUtilsTest, SetTemporaryTrialOverridesTwoGroups) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {"Blue", "On"},
      {"Red", "Off"},
  };
  SetTemporaryTrialOverrides(prefs(), groups);

  EXPECT_EQ(prefs().GetString(kVariationsForcedFieldTrials), "Blue/On/Red/Off");
}

TEST_F(FieldTrialInternalsUtilsTest,
       SetTemporaryTrialOverridesThenForceTrialsAtStartup) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);
  ForceTrialsAtStartup(prefs());

  EXPECT_EQ(TrialState(), "Blue/On");
}

TEST_F(FieldTrialInternalsUtilsTest, EmulateMultipleStarts) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);

  SimulateRestart();
  EXPECT_EQ(TrialState(), "Blue/On");
  EXPECT_EQ(prefs().GetString(kVariationsForcedFieldTrials), "Blue/On");
  EXPECT_EQ(prefs().GetInteger(kVariationsForcedTrialStarts), 1);

  SimulateRestart();
  EXPECT_EQ(TrialState(), "Blue/On");
  EXPECT_EQ(prefs().GetInteger(kVariationsForcedTrialStarts), 2);

  SimulateRestart();
  EXPECT_EQ(TrialState(), "Blue/On");
  EXPECT_EQ(prefs().GetInteger(kVariationsForcedTrialStarts), 3);

  // Note that kChromeStartCountBeforeResetForcedFieldTrials = 3
  SimulateRestart();
  EXPECT_EQ(TrialState(), "");
  EXPECT_EQ(prefs().GetTime(kVariationsForcedTrialExpiration), base::Time());
}

TEST_F(FieldTrialInternalsUtilsTest,
       RefreshAndGetFieldTrialOverridesWithEmptyState) {
  bool requires_restart = false;
  base::flat_map<std::string, std::string> overrides =
      RefreshAndGetFieldTrialOverrides({MakeStudy("Blue")}, prefs(),
                                       requires_restart);

  EXPECT_THAT(overrides, testing::IsEmpty());
  EXPECT_FALSE(requires_restart);
}

TEST_F(FieldTrialInternalsUtilsTest,
       RefreshAndGetFieldTrialOverridesWithActiveOverride) {
  // Set an override, and simulate a couple restarts.
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);

  SimulateRestart();
  SimulateRestart();

  bool requires_restart = false;
  base::flat_map<std::string, std::string> overrides =
      RefreshAndGetFieldTrialOverrides({MakeStudy("Blue")}, prefs(),
                                       requires_restart);

  EXPECT_THAT(overrides, testing::ElementsAre(Pair(HashNameAsHexString("Blue"),
                                                   HashNameAsHexString("On"))));
  EXPECT_FALSE(requires_restart);
  EXPECT_EQ(prefs().GetInteger(kVariationsForcedTrialStarts), 1);
}

TEST_F(FieldTrialInternalsUtilsTest,
       RefreshAndGetFieldTrialOverridesBeforeRestart) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  ASSERT_TRUE(SetTemporaryTrialOverrides(prefs(), groups));
  bool requires_restart = false;
  base::flat_map<std::string, std::string> overrides =
      RefreshAndGetFieldTrialOverrides({MakeStudy("Blue")}, prefs(),
                                       requires_restart);
  EXPECT_TRUE(requires_restart);
}

TEST_F(FieldTrialInternalsUtilsTest,
       RefreshAndGetFieldTrialOverridesStudyNoLongerExists) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);
  SimulateRestart();

  bool requires_restart = false;
  base::flat_map<std::string, std::string> overrides =
      RefreshAndGetFieldTrialOverrides({MakeStudy("Red")}, prefs(),
                                       requires_restart);

  EXPECT_THAT(overrides, testing::IsEmpty());
  EXPECT_TRUE(requires_restart);
  EXPECT_EQ(prefs().GetString(kVariationsForcedFieldTrials), "");
}

TEST_F(FieldTrialInternalsUtilsTest,
       RefreshAndGetFieldTrialOverridesDoesNotRestoreExpiredOverrides) {
  std::vector<std::pair<std::string, std::string>> groups = {
      {{"Blue", "On"}},
  };
  SetTemporaryTrialOverrides(prefs(), groups);
  SimulateRestart();
  SimulateRestart();
  SimulateRestart();
  ClearFieldTrials();

  bool requires_restart = false;
  base::flat_map<std::string, std::string> overrides =
      RefreshAndGetFieldTrialOverrides({MakeStudy("Blue")}, prefs(),
                                       requires_restart);

  EXPECT_THAT(overrides, testing::IsEmpty());
  EXPECT_FALSE(requires_restart);
}

}  // namespace
}  // namespace variations
