// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys_chromeos.h"

#include <utility>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/test/scoped_path_override.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
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

// Verifies that initializing crash keys for a child process still populates
// in-memory crash keys on initial setup and subsequent trial activations,
// without writing .variations-list.txt to disk.
TEST_F(VariationsCrashKeysChromeOsTest, DoesNotWriteInChildProcess) {
  // Override the homedir so any unexpected write would land in `dir`.
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::ScopedPathOverride home_override(base::DIR_HOME, dir.GetPath());
  base::FilePath variations_list_path =
      dir.GetPath().Append(".variations-list.txt");

  // Start with 2 trials, one active before InitCrashKeys() and one activated
  // afterward.
  base::FieldTrialList::CreateFieldTrial("Trial1", "Group1")->Activate();
  base::FieldTrialList::CreateFieldTrial("Trial2", "Group2");
  InitCrashKeys(ProcessType::kChild);

  // Activate Trial2 after initialization to exercise the observer path too.
  EXPECT_EQ("Group2", base::FieldTrialList::FindFullName("Trial2"));
  base::RunLoop().RunUntilIdle();
  task_environment_.RunUntilIdle();

  // In-memory crash keys should include both trials, while
  // .variations-list.txt on disk must not be created.
  ExperimentListInfo info = GetExperimentListInfo();
  EXPECT_EQ(2, info.num_experiments);
  EXPECT_EQ("8e7abfb0-c16397b7,277f2a3d-d77354d0,", info.experiment_list);
  EXPECT_FALSE(base::PathExists(variations_list_path));
}

// Verifies that back-to-back calls to ReportVariationsToChromeOs() cancel any
// pending write tasks that have not started running yet, so only the latest
// variations snapshot is written to disk.
TEST(ReportVariationsToChromeOsTest, CancelsSupersededTasks) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir dir;
  ASSERT_TRUE(dir.CreateUniqueTempDir());
  base::ScopedPathOverride home_override(base::DIR_HOME, dir.GetPath());
  base::FilePath variations_list_path =
      dir.GetPath().Append(".variations-list.txt");

  base::CancelableTaskTracker tracker;
  // Use a manual task runner so the first write remains queued when the second
  // ReportVariationsToChromeOs() call cancels pending tasks.
  auto runner = base::MakeRefCounted<base::TestSimpleTaskRunner>();

  // Post two consecutive updates before `runner` executes either task.
  ReportVariationsToChromeOs(
      &tracker, runner,
      ExperimentListInfo{.num_experiments = 1, .experiment_list = "a-b,"});
  ReportVariationsToChromeOs(
      &tracker, runner,
      ExperimentListInfo{.num_experiments = 2, .experiment_list = "a-b,c-d,"});

  auto pending_tasks = runner->TakePendingTasks();
  ASSERT_EQ(2u, pending_tasks.size());

  // The first write task was superseded and canceled, so running its wrapper
  // callback should be a no-op and leave the file unwritten.
  std::move(pending_tasks[0].task).Run();
  EXPECT_FALSE(base::PathExists(variations_list_path));

  // Running the second task should write the latest experiment list. Pump
  // `task_environment` as well for CancelableTaskTracker's cleanup reply.
  std::move(pending_tasks[1].task).Run();
  task_environment.RunUntilIdle();

  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(variations_list_path, &contents));
  EXPECT_EQ(contents,
            "num-experiments=2\n"
            "variations=a-b,c-d,\n");
}

}  // namespace
}  // namespace variations
