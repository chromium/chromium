// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys_chromeos.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/synthetic_trials_active_group_id_provider.h"
#include "components/variations/variations_crash_keys.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace variations {
namespace {

class VariationsCrashKeysChromeOsTest : public ::testing::Test {
 public:
  VariationsCrashKeysChromeOsTest() {
    crash_reporter::ResetCrashKeysForTesting();
    crash_reporter::InitializeCrashKeysForTesting();
  }

  VariationsCrashKeysChromeOsTest(const VariationsCrashKeysChromeOsTest&) =
      delete;
  VariationsCrashKeysChromeOsTest& operator=(
      const VariationsCrashKeysChromeOsTest&) = delete;

  ~VariationsCrashKeysChromeOsTest() override {
    SyntheticTrialsActiveGroupIdProvider::GetInstance()->ResetForTesting();
    ClearCrashKeysInstanceForTesting();
    crash_reporter::ResetCrashKeysForTesting();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(VariationsCrashKeysChromeOsTest, WritesVariationsList) {
  // Override the homedir so that the class writes to a known location we can
  // check.
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  const base::FilePath& home_path = dir.GetPath();
  base::ScopedPathOverride home_override(base::DIR_HOME, home_path);
  base::FilePath variations_list_path =
      home_path.Append(".variations-list.txt");

  // Start with 2 trials, one active and one not
  base::FieldTrialList::CreateFieldTrial("Trial1", "Group1")->Activate();
  base::FieldTrialList::CreateFieldTrial("Trial2", "Group2");

  InitCrashKeys();

  base::RunLoop().RunUntilIdle();
  task_environment_.RunUntilIdle();

  ExperimentListInfo info = GetExperimentListInfo();
  EXPECT_EQ(1, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,", info.experiment_list);

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(variations_list_path, &contents));
  EXPECT_EQ(contents,
            "num-experiments=1\n"
            "variations=8e7abfb0-c16397b7,\n");

  // Now, activate Trial2.
  EXPECT_EQ("Group2", base::FieldTrialList::FindFullName("Trial2"));
  base::RunLoop().RunUntilIdle();
  task_environment_.RunUntilIdle();

  info = GetExperimentListInfo();
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,", info.experiment_list);

  ASSERT_TRUE(base::ReadFileToString(variations_list_path, &contents));
  EXPECT_EQ(contents,
            "num-experiments=2\n"
            "variations=8e7abfb0-c16397b7,277f2a3d-d77354d0,\n");
}

}  // namespace
}  // namespace variations
