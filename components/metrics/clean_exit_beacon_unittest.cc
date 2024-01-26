// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "components/prefs/testing_pref_store.h"
#include "components/variations/pref_names.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

const wchar_t kDummyWindowsRegistryKey[] = L"";

}  // namespace

class TestCleanExitBeacon : public CleanExitBeacon {
 public:
  explicit TestCleanExitBeacon(
      PrefService* local_state,
      const base::FilePath& user_data_dir = base::FilePath())
      : CleanExitBeacon(kDummyWindowsRegistryKey, user_data_dir, local_state) {
    Initialize();
  }

  ~TestCleanExitBeacon() override = default;
};

class CleanExitBeaconTest : public ::testing::Test {
 public:
  void SetUp() override {
    metrics::CleanExitBeacon::RegisterPrefs(prefs_.registry());
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  }

 protected:
  base::HistogramTester histogram_tester_;
  TestingPrefServiceSimple prefs_;
  base::ScopedTempDir user_data_dir_;

 private:
  base::test::TaskEnvironment task_environment_;
};

struct BadBeaconTestParams {
  const std::string test_name;
  bool beacon_file_exists;
  const std::string beacon_file_contents;
  BeaconFileState beacon_file_state;
};

// Used for testing beacon files that are not well-formed, do not exist, etc.
class BadBeaconFileTest
    : public testing::WithParamInterface<BadBeaconTestParams>,
      public CleanExitBeaconTest {};

struct BeaconConsistencyTestParams {
  // Inputs:
  const std::string test_name;
  std::optional<bool> beacon_file_beacon_value;
  std::optional<bool> platform_specific_beacon_value;
  std::optional<bool> local_state_beacon_value;
  // Result:
  CleanExitBeaconConsistency expected_consistency;
};

#if BUILDFLAG(IS_IOS)
// Used for testing the logic that emits to the UMA.CleanExitBeaconConsistency3
// histogram.
class BeaconFileAndPlatformBeaconConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};
#endif  // BUILDFLAG(IS_IOS)

// Verify that the crash streak metric is 0 when default pref values are used.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithDefaultPrefs) {
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                       1);
}

// Verify that the crash streak metric is 0 when prefs are explicitly set to
// their defaults.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithNoCrashes) {
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here. Similarly, we explicitly set
  // kVariationsCrashStreak to 0.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 0);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                       1);
}

// Verify that the crash streak metric is correctly recorded when there is a
// non-zero crash streak.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithSomeCrashes) {
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly.
TEST_F(CleanExitBeaconTest, CrashIncrementsCrashStreak) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak), 2);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly and the default crash streak value
// is used.
TEST_F(CleanExitBeaconTest,
       CrashIncrementsCrashStreakWithDefaultCrashStreakPref) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak), 1);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that no attempt is made to read the beacon file when no user
// data dir is provided.
TEST_F(CleanExitBeaconTest, InitWithoutUserDataDir) {
  TestCleanExitBeacon beacon(&prefs_, base::FilePath());
  EXPECT_TRUE(beacon.GetUserDataDirForTesting().empty());
  EXPECT_TRUE(beacon.GetBeaconFilePathForTesting().empty());
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup", 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    BadBeaconFileTest,
    ::testing::Values(
        BadBeaconTestParams{
            .test_name = "NoVariationsFile",
            .beacon_file_exists = false,
            .beacon_file_contents = "",
            .beacon_file_state = BeaconFileState::kNotDeserializable},
        BadBeaconTestParams{
            .test_name = "EmptyVariationsFile",
            .beacon_file_exists = true,
            .beacon_file_contents = "",
            .beacon_file_state = BeaconFileState::kNotDeserializable},
        BadBeaconTestParams{
            .test_name = "NotDictionary",
            .beacon_file_exists = true,
            .beacon_file_contents = "{abc123",
            .beacon_file_state = BeaconFileState::kNotDeserializable},
        BadBeaconTestParams{
            .test_name = "EmptyDictionary",
            .beacon_file_exists = true,
            .beacon_file_contents = "{}",
            .beacon_file_state = BeaconFileState::kMissingDictionary},
        BadBeaconTestParams{
            .test_name = "MissingCrashStreak",
            .beacon_file_exists = true,
            .beacon_file_contents =
                "{\"user_experience_metrics.stability.exited_cleanly\":true}",
            .beacon_file_state = BeaconFileState::kMissingCrashStreak},
        BadBeaconTestParams{
            .test_name = "MissingBeacon",
            .beacon_file_exists = true,
            .beacon_file_contents = "{\"variations_crash_streak\":1}",
            .beacon_file_state = BeaconFileState::kMissingBeacon}),
    [](const ::testing::TestParamInfo<BadBeaconTestParams>& params) {
      return params.param.test_name;
    });

// Verify that the inability to get the beacon file's contents for a plethora of
// reasons (a) doesn't crash and (b) correctly records the  BeaconFileState
// metric.
TEST_P(BadBeaconFileTest, InitWithUnusableBeaconFile) {
  BadBeaconTestParams params = GetParam();

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  if (params.beacon_file_exists) {
    const base::FilePath temp_beacon_file_path =
        user_data_dir_path.Append(kCleanExitBeaconFilename);
    ASSERT_TRUE(
        base::WriteFile(temp_beacon_file_path, params.beacon_file_contents));
  }

  TestCleanExitBeacon beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup",
      params.beacon_file_state, 1);
}

// Verify that successfully reading the beacon file's contents results in
// correctly (a) setting the |did_previous_session_exit_cleanly_| field and (b)
// recording metrics when the last session exited cleanly.
TEST_F(CleanExitBeaconTest, InitWithBeaconFile) {
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(kCleanExitBeaconFilename);
  const int num_crashes = 2;
  ASSERT_TRUE(base::WriteFile(
      temp_beacon_file_path,
      CleanExitBeacon::CreateBeaconFileContentsForTesting(
          /*exited_cleanly=*/true, /*crash_streak=*/num_crashes)));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup",
      BeaconFileState::kReadable, 1);
  EXPECT_TRUE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       num_crashes, 1);
}

// Verify that successfully reading the beacon file's contents results in
// correctly (a) setting the |did_previous_session_exit_cleanly_| field and (b)
// recording metrics when the last session did not exit cleanly.
TEST_F(CleanExitBeaconTest, InitWithCrashAndBeaconFile) {
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(kCleanExitBeaconFilename);
  const int last_session_num_crashes = 2;
  ASSERT_TRUE(
      base::WriteFile(temp_beacon_file_path,
                      CleanExitBeacon::CreateBeaconFileContentsForTesting(
                          /*exited_cleanly=*/false,
                          /*crash_streak=*/last_session_num_crashes)));

  const int updated_num_crashes = last_session_num_crashes + 1;
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup",
      BeaconFileState::kReadable, 1);
  EXPECT_FALSE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       updated_num_crashes, 1);
}

TEST_F(CleanExitBeaconTest, WriteBeaconValueWhenNotExitingCleanly) {
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath beacon_file_path =
      user_data_dir_path.Append(kCleanExitBeaconFilename);
  ASSERT_FALSE(base::PathExists(beacon_file_path));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/true);

  // Verify that the beacon file exists and has well-formed contents after
  // updating the beacon value.
  EXPECT_TRUE(base::PathExists(beacon_file_path));
  std::string beacon_file_contents1;
  ASSERT_TRUE(base::ReadFileToString(beacon_file_path, &beacon_file_contents1));
  EXPECT_EQ(beacon_file_contents1,
            "{\"user_experience_metrics.stability.exited_cleanly\":false,"
            "\"variations_crash_streak\":0}");
  // Verify that the BeaconFileWrite metric was emitted.
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);

  // Write the beacon value again. This is done because it is possible for
  // WriteBeaconValue() to be called twice during startup or shutdown with the
  // same value for |exited_cleanly|.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly*/ false,
                                     /*is_extended_safe_mode=*/false);

  // Verify that the beacon file exists and has well-formed contents after
  // updating the beacon value.
  EXPECT_TRUE(base::PathExists(beacon_file_path));
  std::string beacon_file_contents2;
  ASSERT_TRUE(base::ReadFileToString(beacon_file_path, &beacon_file_contents2));
  EXPECT_EQ(beacon_file_contents2,
            "{\"user_experience_metrics.stability.exited_cleanly\":false,"
            "\"variations_crash_streak\":0}");
  // Verify that the BeaconFileWrite metric was not emitted a second time. The
  // beacon file should not have been written again since the beacon value did
  // not change.
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);
}

TEST_F(CleanExitBeaconTest, WriteBeaconValueWhenExitingCleanly) {
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath beacon_file_path =
      user_data_dir_path.Append(kCleanExitBeaconFilename);
  ASSERT_FALSE(base::PathExists(beacon_file_path));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/true,
                                     /*is_extended_safe_mode=*/false);

  // Verify that the beacon file exists and has well-formed contents after
  // updating the beacon value.
  EXPECT_TRUE(base::PathExists(beacon_file_path));
  std::string beacon_file_contents1;
  ASSERT_TRUE(base::ReadFileToString(beacon_file_path, &beacon_file_contents1));
  EXPECT_EQ(beacon_file_contents1,
            "{\"user_experience_metrics.stability.exited_cleanly\":true,"
            "\"variations_crash_streak\":0}");
  // Verify that the BeaconFileWrite metric was emitted.
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);

  // Write the beacon value again. This is done because it is possible for
  // WriteBeaconValue() to be called twice during startup or shutdown with the
  // same value for |exited_cleanly|.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly*/ true,
                                     /*is_extended_safe_mode=*/false);

  // Verify that the beacon file exists and has well-formed contents after
  // updating the beacon value.
  EXPECT_TRUE(base::PathExists(beacon_file_path));
  std::string beacon_file_contents2;
  ASSERT_TRUE(base::ReadFileToString(beacon_file_path, &beacon_file_contents2));
  EXPECT_EQ(beacon_file_contents2,
            "{\"user_experience_metrics.stability.exited_cleanly\":true,"
            "\"variations_crash_streak\":0}");
  // Verify that the BeaconFileWrite metric was not emitted a second time. The
  // beacon file should not have been written again since the beacon value did
  // not change.
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 1, 1);
}

// Verify that there's a DCHECK when attempting to write a clean beacon with
// |is_extended_safe_mode| set to true. When |is_extended_safe_mode| is true,
// the only valid value for |exited_cleanly| is false.
TEST_F(CleanExitBeaconTest, InvalidWriteBeaconValueArgsTriggerDcheck) {
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_.GetPath());
  EXPECT_DCHECK_DEATH(
      clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/true,
                                         /*is_extended_safe_mode=*/true));
}

#if BUILDFLAG(IS_IOS)
// Verify the logic for recording UMA.CleanExitBeaconConsistency3.
INSTANTIATE_TEST_SUITE_P(
    All,
    BeaconFileAndPlatformBeaconConsistencyTest,
    ::testing::Values(
        BeaconConsistencyTestParams{
            .test_name = "MissingMissing",
            .expected_consistency =
                CleanExitBeaconConsistency::kMissingMissing},
        BeaconConsistencyTestParams{
            .test_name = "MissingClean",
            .platform_specific_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kMissingClean},
        BeaconConsistencyTestParams{
            .test_name = "MissingDirty",
            .platform_specific_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kMissingDirty},
        BeaconConsistencyTestParams{
            .test_name = "CleanMissing",
            .beacon_file_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanMissing},
        BeaconConsistencyTestParams{
            .test_name = "DirtyMissing",
            .beacon_file_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyMissing},
        BeaconConsistencyTestParams{
            .test_name = "CleanClean",
            .beacon_file_beacon_value = true,
            .platform_specific_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanClean},
        BeaconConsistencyTestParams{
            .test_name = "CleanDirty",
            .beacon_file_beacon_value = true,
            .platform_specific_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kCleanDirty},
        BeaconConsistencyTestParams{
            .test_name = "DirtyClean",
            .beacon_file_beacon_value = false,
            .platform_specific_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyClean},
        BeaconConsistencyTestParams{
            .test_name = "DirtyDirty",
            .beacon_file_beacon_value = false,
            .platform_specific_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyDirty}),
    [](const ::testing::TestParamInfo<BeaconConsistencyTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(BeaconFileAndPlatformBeaconConsistencyTest, BeaconConsistency) {
  // Verify that the beacon file is not present. Unless set below, this beacon
  // is considered missing.
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(kCleanExitBeaconFilename);
  ASSERT_FALSE(base::PathExists(temp_beacon_file_path));
  // Clear the platform-specific beacon. Unless set below, this beacon is also
  // considered missing.
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);

  BeaconConsistencyTestParams params = GetParam();
  if (params.beacon_file_beacon_value) {
    ASSERT_TRUE(base::WriteFile(
        temp_beacon_file_path,
        CleanExitBeacon::CreateBeaconFileContentsForTesting(
            /*exited_cleanly=*/params.beacon_file_beacon_value.value(),
            /*crash_streak=*/0)));
  }
  if (params.platform_specific_beacon_value) {
    CleanExitBeacon::SetUserDefaultsBeacon(
        /*exited_cleanly=*/params.platform_specific_beacon_value.value());
  }

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample("UMA.CleanExitBeaconConsistency3",
                                       params.expected_consistency, 1);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace metrics
