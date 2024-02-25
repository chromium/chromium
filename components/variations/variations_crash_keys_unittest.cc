// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/hashing.h"
#include "components/variations/synthetic_trial_registry.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {

namespace {

std::string GetVariationsCrashKey() {
  return crash_reporter::GetCrashKeyValue("variations");
}

std::string GetNumExperimentsCrashKey() {
  return crash_reporter::GetCrashKeyValue("num-experiments");
}

std::string GetVariationsSeedVersionCrashKey() {
  return crash_reporter::GetCrashKeyValue("variations-seed-version");
}

class VariationsCrashKeysTest : public ::testing::Test {
 public:
  VariationsCrashKeysTest() {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeysForTesting();
  }

  VariationsCrashKeysTest(const VariationsCrashKeysTest&) = delete;
  VariationsCrashKeysTest& operator=(const VariationsCrashKeysTest&) = delete;

  ~VariationsCrashKeysTest() override {
    SyntheticTrialsActiveGroupIdProvider::GetInstance()->ResetForTesting();
    ClearCrashKeysInstanceForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(VariationsCrashKeysTest, BasicFunctionality) {
  SyntheticTrialRegistry registry;
  registry.AddObserver(SyntheticTrialsActiveGroupIdProvider::GetInstance());

  // Start with 2 trials, one active and one not
  base::FieldTrialList::CreateFieldTrial("Trial1", "Group1")->Activate();
  base::FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  InitCrashKeys();

  EXPECT_EQ("1", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,", GetVariationsCrashKey());

  ExperimentListInfo info = GetExperimentListInfo();
  EXPECT_EQ(1, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,", info.experiment_list);

  // Now, active Trial2.
  EXPECT_EQ("Group2", base::FieldTrialList::FindFullName("Trial2"));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ("2", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,", GetVariationsCrashKey());
  info = GetExperimentListInfo();
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,", info.experiment_list);

  // Add two synthetic trials and confirm that they show up in the list.
  SyntheticTrialGroup synth_trial(
      "Trial3", "Group3", variations::SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(synth_trial);

  EXPECT_EQ("3", GetNumExperimentsCrashKey());
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,9f339c9d-746c2ad4,",
            GetVariationsCrashKey());
  info = GetExperimentListInfo();
  EXPECT_EQ(3, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,9f339c9d-746c2ad4,",
            info.experiment_list);

  // Add another regular trial.
  base::FieldTrialList::CreateFieldTrial("Trial4", "Group4")->Activate();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ("4", GetNumExperimentsCrashKey());
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-746c2ad4,",
      GetVariationsCrashKey());
  info = GetExperimentListInfo();
  EXPECT_EQ(4, info.num_experiments);
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-746c2ad4,",
      info.experiment_list);

  // Replace synthetic trial group and add one more.
  SyntheticTrialGroup synth_trial2(
      "Trial3", "Group3_A", variations::SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(synth_trial2);
  SyntheticTrialGroup synth_trial3(
      "Trial4", "Group4", variations::SyntheticTrialAnnotationMode::kNextLog);
  registry.RegisterSyntheticFieldTrial(synth_trial3);

  EXPECT_EQ("5", GetNumExperimentsCrashKey());
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-3250dddc,21710f4c-99b90b01,",
      GetVariationsCrashKey());
  info = GetExperimentListInfo();
  EXPECT_EQ(5, info.num_experiments);
  EXPECT_EQ(
      "8e7abfb0-c16397b7,277f2a3d-d77354d0,21710f4c-99b90b01,"
      "9f339c9d-3250dddc,21710f4c-99b90b01,",
      info.experiment_list);
}

TEST_F(VariationsCrashKeysTest, SeedVersionFromParsedSeed) {
  SetSeedVersion("version-123");
  InitCrashKeys();
  EXPECT_EQ("version-123", GetVariationsSeedVersionCrashKey());
}

TEST_F(VariationsCrashKeysTest, SeedVersionFromCommandLineSwitch) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      variations::switches::kVariationsSeedVersion, "version-456");
  InitCrashKeys();
  EXPECT_EQ("version-456", GetVariationsSeedVersionCrashKey());
}

TEST_F(VariationsCrashKeysTest, OverriddenFieldTrial) {
  base::FieldTrialList::CreateFieldTrial("Trial1", "Group1",
                                         /*is_low_anonymity=*/false,
                                         /*is_overridden=*/true)
      ->Activate();

  InitCrashKeys();

  // Because the trial is overridden, it has a different group variation ID.
  EXPECT_EQ("1", GetNumExperimentsCrashKey());
  EXPECT_EQ("2a140065", HashNameAsHexString("Group1_MANUALLY_FORCED"));
  EXPECT_EQ("8e7abfb0-2a140065,", GetVariationsCrashKey());
}

}  // namespace variations
