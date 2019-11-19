// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys.h"

#include <string>

#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

std::string GetVariationsCrashKey() {
  return crash_reporter::GetCrashKeyValue("variations");
}

std::string GetNumExperimentsCrashKey() {
  return crash_reporter::GetCrashKeyValue("num-experiments");
}

class VariationsCrashKeysTest : public ::testing::Test {
 public:
  VariationsCrashKeysTest() {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeysForTesting();
  }

  ~VariationsCrashKeysTest() override {
    SyntheticTrialsActiveGroupIdProvider::GetInstance()->ResetForTesting();
    ClearCrashKeysInstanceForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(VariationsCrashKeysTest);
};

}  // namespace

TEST_F(VariationsCrashKeysTest, BasicFunctionality) {
  SyntheticTrialRegistry registry;
  registry.AddSyntheticTrialObserver(
      SyntheticTrialsActiveGroupIdProvider::GetInstance());

  // Start with 2 trials, one active and one not
  base::FieldTrialList::CreateFieldTrial("Trial1", "Group1")->group();
  base::FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  InitCrashKeys();

  EXPECT_EQ("1", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,", GetVariationsCrashKey());

  // Now, active Trial2.
  EXPECT_EQ("Group2", base::FieldTrialList::FindFullName("Trial2"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("2", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,", GetVariationsCrashKey());

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup synth_trial(HashName("Trial3"), HashName("Group3"));
  registry.RegisterSyntheticFieldTrial(synth_trial);

  EXPECT_EQ("3", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,9f339c9d-746c2ad4,",
            GetVariationsCrashKey());

  // Add another regular trial.
  base::FieldTrialList::CreateFieldTrial("Trial4", "Group4")->group();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("4", GetNumExperimentsCrashKey());
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-746c2ad4,",
      GetVariationsCrashKey());

  // Replace synthetic trial group and add one more.
  SyntheticTrialGroup synth_trial2(HashName("Trial3"), HashName("Group3_A"));
  registry.RegisterSyntheticFieldTrial(synth_trial2);
  SyntheticTrialGroup synth_trial3(HashName("Trial4"), HashName("Group4"));
  registry.RegisterSyntheticFieldTrial(synth_trial3);

  EXPECT_EQ("5", GetNumExperimentsCrashKey());
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-3250dddc,21710f4c-99b90b01,",
      GetVariationsCrashKey());
}

}  // namespace variations
