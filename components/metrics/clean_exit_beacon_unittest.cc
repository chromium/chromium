// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <string>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
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
#include "components/variations/service/variations_safe_mode_constants.h"
#include "components/variations/variations_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace metrics {
namespace {

using ::variations::SetUpExtendedSafeModeExperiment;

const wchar_t kDummyWindowsRegistryKey[] = L"";

// Creates and returns well-formed beacon file contents with the given values.
std::string CreateWellFormedBeaconFileContents(
    bool exited_cleanly,
    int crash_streak,
    absl::optional<BeaconMonitoringStage> stage = absl::nullopt) {
  const std::string exited_cleanly_str = exited_cleanly ? "true" : "false";
  if (stage) {
    const std::string stage_str =
        base::NumberToString(static_cast<int>(stage.value()));
    return base::StringPrintf(
        "{\n"
        "  \"monitoring_stage\": %s,\n"
        "  \"user_experience_metrics.stability.exited_cleanly\": %s,\n"
        "  \"variations_crash_streak\": %s\n"
        "}",
        stage_str.data(), exited_cleanly_str.data(),
        base::NumberToString(crash_streak).data());
  }
  // The monitoring stage was added to the beacon file in a later milestone,
  // so beacon files of clients running older Chrome versions may not always
  // have it.
  return base::StringPrintf(
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\": %s,\n"
      "  \"variations_crash_streak\": %s\n"
      "}",
      exited_cleanly_str.data(), base::NumberToString(crash_streak).data());
}

}  // namespace

class TestCleanExitBeacon : public CleanExitBeacon {
 public:
  explicit TestCleanExitBeacon(
      PrefService* local_state,
      const base::FilePath& user_data_dir = base::FilePath(),
      version_info::Channel channel = version_info::Channel::UNKNOWN)
      : CleanExitBeacon(kDummyWindowsRegistryKey,
                        user_data_dir,
                        local_state,
                        channel) {
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
  absl::optional<bool> beacon_file_beacon_value;
  absl::optional<bool> platform_specific_beacon_value;
  absl::optional<bool> local_state_beacon_value;
  // Result:
  CleanExitBeaconConsistency expected_consistency;
};

#if BUILDFLAG(IS_IOS)
// Used for testing the logic that emits to the UMA.CleanExitBeaconConsistency2
// histogram.
class PlatformBeaconAndLocalStateBeaconConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};

// Used for testing the logic that emits to the UMA.CleanExitBeaconConsistency3
// histogram.
class BeaconFileAndPlatformBeaconConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};
#endif  // BUILDFLAG(IS_IOS)

// Used for testing the logic that emits to the
// UMA.CleanExitBeacon.BeaconFileConsistency histogram.
class BeaconFileConsistencyTest
    : public testing::WithParamInterface<BeaconConsistencyTestParams>,
      public CleanExitBeaconTest {};

struct MonitoringStageTestParams {
  const std::string test_name;
  const std::string experiment_group;
  bool exited_cleanly;
  bool is_extended_safe_mode;
  absl::optional<BeaconMonitoringStage> stage;
};

class MonitoringStageMetricTest
    : public testing::WithParamInterface<MonitoringStageTestParams>,
      public CleanExitBeaconTest {};

class MonitoringStageWritingTest
    : public testing::WithParamInterface<MonitoringStageTestParams>,
      public CleanExitBeaconTest {};

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

// Verify that (a) the client is excluded from the Extended Variations Safe Mode
// experiment and (b) no attempt is made to read the beacon file when no user
// data dir is provided.
TEST_F(CleanExitBeaconTest, InitWithoutUserDataDir) {
  TestCleanExitBeacon beacon(&prefs_, base::FilePath());
  EXPECT_FALSE(
      base::FieldTrialList::IsTrialActive(variations::kExtendedSafeModeTrial));
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup", 0);
}

// Verify that the beacon file is not read when the client is not in the
// SignalAndWriteViaFileUtil experiment group. It is possible for a client to
// have the file and to not be in the SignalAndWriteViaFileUtil group when the
// client was in the group in a previous session and then switched groups, e.g.
// via kResetVariationState.
TEST_F(CleanExitBeaconTest, FileIgnoredByControlGroup) {
  // Deliberately set the prefs so that we can later verify that their values
  // have not changed.
  int expected_crash_streak = 0;
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak,
                    expected_crash_streak);
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);

  // Prepare a well-formed beacon file, which we expect to be ignored. (If it
  // were used, then the prefs' values would change.)
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/false, /*crash_streak=*/2)
                                   .data()));
  const std::string group_name = variations::kControlGroup;
  SetUpExtendedSafeModeExperiment(group_name);
  ASSERT_EQ(group_name, base::FieldTrialList::FindFullName(
                            variations::kExtendedSafeModeTrial));
  TestCleanExitBeacon beacon(&prefs_, user_data_dir_path);

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak),
            expected_crash_streak);
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
                "{\"user_experience_metrics.stability.exited_cleanly\": true}",
            .beacon_file_state = BeaconFileState::kMissingCrashStreak},
        BadBeaconTestParams{
            .test_name = "MissingBeacon",
            .beacon_file_exists = true,
            .beacon_file_contents = "{\"variations_crash_streak\": 1}",
            .beacon_file_state = BeaconFileState::kMissingBeacon}),
    [](const ::testing::TestParamInfo<BadBeaconTestParams>& params) {
      return params.param.test_name;
    });

// Verify that the inability to get the beacon file's contents for a plethora of
// reasons (a) doesn't crash and (b) correctly records the  BeaconFileState
// metric.
TEST_P(BadBeaconFileTest, InitWithUnusableBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  BadBeaconTestParams params = GetParam();

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  if (params.beacon_file_exists) {
    const base::FilePath temp_beacon_file_path =
        user_data_dir_path.Append(variations::kVariationsFilename);
    ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                                 params.beacon_file_contents.data()));
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
  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int num_crashes = 2;
  ASSERT_LT(0, base::WriteFile(
                   temp_beacon_file_path,
                   CreateWellFormedBeaconFileContents(
                       /*exited_cleanly=*/true, /*crash_streak=*/num_crashes)
                       .data()));

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
  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int last_session_num_crashes = 2;
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/false,
                                   /*crash_streak=*/last_session_num_crashes)
                                   .data()));

  const int updated_num_crashes = last_session_num_crashes + 1;
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.BeaconFileStateAtStartup",
      BeaconFileState::kReadable, 1);
  EXPECT_FALSE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       updated_num_crashes, 1);
}

// Verify that the logic for recording UMA.CleanExitBeacon.BeaconFileConsistency
// is correct for clients in the Extended Variations Safe Mode experiment's
// enabled group.
INSTANTIATE_TEST_SUITE_P(
    All,
    BeaconFileConsistencyTest,
    ::testing::Values(
        BeaconConsistencyTestParams{
            .test_name = "MissingMissing",
            .expected_consistency =
                CleanExitBeaconConsistency::kMissingMissing},
        BeaconConsistencyTestParams{
            .test_name = "MissingClean",
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kMissingClean},
        BeaconConsistencyTestParams{
            .test_name = "MissingDirty",
            .local_state_beacon_value = false,
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
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanClean},
        BeaconConsistencyTestParams{
            .test_name = "CleanDirty",
            .beacon_file_beacon_value = true,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kCleanDirty},
        BeaconConsistencyTestParams{
            .test_name = "DirtyClean",
            .beacon_file_beacon_value = false,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyClean},
        BeaconConsistencyTestParams{
            .test_name = "DirtyDirty",
            .beacon_file_beacon_value = false,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyDirty}),
    [](const ::testing::TestParamInfo<BeaconConsistencyTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(BeaconFileConsistencyTest, BeaconConsistency) {
  // Verify that the beacon file is not present. Unless set below, this beacon
  // is considered missing.
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_FALSE(base::PathExists(temp_beacon_file_path));
  // Clear the Local State beacon. Unless set below, it is also considered
  // missing.
  prefs_.ClearPref(prefs::kStabilityExitedCleanly);

  BeaconConsistencyTestParams params = GetParam();
  if (params.beacon_file_beacon_value) {
    ASSERT_LT(
        0, base::WriteFile(
               temp_beacon_file_path,
               CreateWellFormedBeaconFileContents(
                   /*exited_cleanly=*/params.beacon_file_beacon_value.value(),
                   /*crash_streak=*/0)
                   .data()));
  }
  if (params.local_state_beacon_value) {
    prefs_.SetBoolean(prefs::kStabilityExitedCleanly,
                      params.local_state_beacon_value.value());
  }

  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  ASSERT_EQ(variations::kEnabledGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample(
      "UMA.CleanExitBeacon.BeaconFileConsistency", params.expected_consistency,
      1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MonitoringStageMetricTest,
    ::testing::Values(
        // Verify that UMA.CleanExitBeacon.MonitoringStage is not emitted when
        // Chrome exited cleanly.
        MonitoringStageTestParams{.test_name = "ControlGroup_CleanExit",
                                  .experiment_group = variations::kControlGroup,
                                  .exited_cleanly = true,
                                  .stage = absl::nullopt},
        MonitoringStageTestParams{.test_name = "ExperimentGroup_CleanExit",
                                  .experiment_group = variations::kEnabledGroup,
                                  .exited_cleanly = true,
                                  .stage = absl::nullopt},
        // Verify that BeaconMonitoringStage::kMissing is emitted when the
        // beacon file does not have a monitoring stage. This can happen because
        // the monitoring stage was added in a later milestone.
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_DirtyExit_Missing",
            .experiment_group = variations::kEnabledGroup,
            .exited_cleanly = false,
            .stage = BeaconMonitoringStage::kMissing},
        // Verify that BeaconMonitoringStage::kExtended is emitted when the
        // beacon file's monitoring stage indicates that the unclean exit was
        // detected due to the Extended Variations Safe Mode experiment.
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_DirtyExit_Extended",
            .experiment_group = variations::kEnabledGroup,
            .exited_cleanly = false,
            .stage = BeaconMonitoringStage::kExtended},
        // Verify that BeaconMonitoringStage::kStatusQuo is emitted when the
        // unclean exit was detected as a result of the status quo monitoring
        // code.
        MonitoringStageTestParams{
            .test_name = "ControlGroup_DirtyExit_StatusQuo",
            .experiment_group = variations::kControlGroup,
            .exited_cleanly = false,
            .stage = BeaconMonitoringStage::kStatusQuo},
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_DirtyExit_StatusQuo",
            .experiment_group = variations::kControlGroup,
            .exited_cleanly = false,
            .stage = BeaconMonitoringStage::kStatusQuo}),
    [](const ::testing::TestParamInfo<MonitoringStageTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(MonitoringStageMetricTest, CheckMonitoringStageMetric) {
  MonitoringStageTestParams params = GetParam();
  SetUpExtendedSafeModeExperiment(params.experiment_group);

  // |crash_streak|'s value is arbitrary and not important. We specify it since
  // well-formed beacon files include the streak and set it in Local State to be
  // consistent.
  const int crash_streak = 1;
  // Set up Local State prefs. If the control group behavior is under test, then
  // Local State is used and the beacon file is ignored.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_,
                                                       params.exited_cleanly);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, crash_streak);
  // Set up the beacon file. If the experiment group behavior is under test,
  // then the beacon file is used and Local State is ignored.
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                               CreateWellFormedBeaconFileContents(
                                   /*exited_cleanly=*/params.exited_cleanly,
                                   /*crash_streak=*/crash_streak,
                                   /*stage=*/params.stage)
                                   .data()));

  // Create and initialize the CleanExitBeacon.
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);

  if (params.exited_cleanly) {
    ASSERT_TRUE(clean_exit_beacon.exited_cleanly());
    // Verify that the metric is not emitted when Chrome exited cleanly.
    histogram_tester_.ExpectTotalCount("UMA.CleanExitBeacon.MonitoringStage",
                                       0);
  } else {
    ASSERT_FALSE(clean_exit_beacon.exited_cleanly());
    // Verify that the expected BeaconMonitoringStage is emitted.
    histogram_tester_.ExpectUniqueSample("UMA.CleanExitBeacon.MonitoringStage",
                                         params.stage.value(), 1);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MonitoringStageWritingTest,
    ::testing::Values(
        // Verify that the beacon file is not written for control group clients.
        MonitoringStageTestParams{.test_name = "ControlGroup_CleanExit",
                                  .experiment_group = variations::kControlGroup,
                                  .exited_cleanly = true,
                                  .is_extended_safe_mode = false},
        MonitoringStageTestParams{.test_name = "ControlGroup_DirtyExit",
                                  .experiment_group = variations::kControlGroup,
                                  .exited_cleanly = false,
                                  .is_extended_safe_mode = false},
        // Verify that signaling that Chrome should stop watching for crashes
        // for experiment group clients results in a beacon file with the
        // kNotMonitoring stage.
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_CleanExit_AsynchronousWrite",
            .experiment_group = variations::kEnabledGroup,
            .exited_cleanly = true,
            .is_extended_safe_mode = false,
            .stage = BeaconMonitoringStage::kNotMonitoring},
        // Verify that signaling that Chrome should watch for crashes with
        // |is_extended_safe_mode| set to true for experiment group clients
        // results in a beacon file with the kExtended stage.
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_DirtyExit_SynchronousWrite",
            .experiment_group = variations::kEnabledGroup,
            .exited_cleanly = false,
            .is_extended_safe_mode = true,
            .stage = BeaconMonitoringStage::kExtended},
        // Verify that signaling that Chrome should watch for crashes with
        // |is_extended_safe_mode| set to false for experiment group clients
        // results in a beacon file with the kStatusQuo stage.
        MonitoringStageTestParams{
            .test_name = "ExperimentGroup_DirtyExit_AsynchronousWrite",
            .experiment_group = variations::kEnabledGroup,
            .exited_cleanly = false,
            .is_extended_safe_mode = false,
            .stage = BeaconMonitoringStage::kStatusQuo}),
    [](const ::testing::TestParamInfo<MonitoringStageTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(MonitoringStageWritingTest, CheckMonitoringStage) {
  MonitoringStageTestParams params = GetParam();
  const std::string group = params.experiment_group;
  SetUpExtendedSafeModeExperiment(group);

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath expected_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_FALSE(base::PathExists(expected_beacon_file_path));

  // Create and initialize the CleanExitBeacon.
  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);

  clean_exit_beacon.WriteBeaconValue(params.exited_cleanly,
                                     params.is_extended_safe_mode);

  // Check that experiment group clients have a beacon file and that control
  // group clients do not.
  EXPECT_EQ(group == variations::kEnabledGroup,
            base::PathExists(expected_beacon_file_path));

  if (group == variations::kEnabledGroup) {
    // For experiment group clients, check the beacon file contents.
    std::string beacon_file_contents;
    ASSERT_TRUE(base::ReadFileToString(expected_beacon_file_path,
                                       &beacon_file_contents));

    const std::string expected_stage =
        "monitoring_stage\":" +
        base::NumberToString(static_cast<int>(params.stage.value()));
    const std::string exited_cleanly = params.exited_cleanly ? "true" : "false";
    const std::string expected_beacon_value =
        "exited_cleanly\":" + exited_cleanly;
    EXPECT_TRUE(base::Contains(beacon_file_contents, expected_stage));
    EXPECT_TRUE(base::Contains(beacon_file_contents, expected_beacon_value));
  }
}

// Verify that attempting to write synchronously DCHECKs for clients that do not
// belong to the SignalAndWriteViaFileUtil experiment group.
TEST_F(CleanExitBeaconTest,
       WriteBeaconValue_SynchronousWriteDcheck_ControlGroup) {
  SetUpExtendedSafeModeExperiment(variations::kControlGroup);
  ASSERT_EQ(variations::kControlGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_.GetPath());
  EXPECT_DCHECK_DEATH(
      clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                         /*is_extended_safe_mode=*/true));

  // Verify metrics.
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 0);
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.BeaconFileWrite", 0);
}

// Verify that there's a DCHECK when an Extended Variations Safe Mode client
// attempts to write a clean beacon with |is_extended_safe_mode| set to true.
// |is_extended_safe_mode| should only be set to true in one call site:
// VariationsFieldTrialCreator::MaybeExtendVariationsSafeMode().
TEST_F(CleanExitBeaconTest,
       WriteBeaconValue_SynchronousWriteDcheck_ExperimentGroup) {
  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  ASSERT_EQ(variations::kEnabledGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_.GetPath());
  EXPECT_DCHECK_DEATH(
      clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/true,
                                         /*is_extended_safe_mode=*/true));
}

#if BUILDFLAG(IS_IOS)
// Verify that the logic for recording UMA.CleanExitBeaconConsistency2 is
// correct.
INSTANTIATE_TEST_SUITE_P(
    All,
    PlatformBeaconAndLocalStateBeaconConsistencyTest,
    ::testing::Values(
        BeaconConsistencyTestParams{
            .test_name = "MissingMissing",
            .expected_consistency =
                CleanExitBeaconConsistency::kMissingMissing},
        BeaconConsistencyTestParams{
            .test_name = "MissingClean",
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kMissingClean},
        BeaconConsistencyTestParams{
            .test_name = "MissingDirty",
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kMissingDirty},
        BeaconConsistencyTestParams{
            .test_name = "CleanMissing",
            .platform_specific_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanMissing},
        BeaconConsistencyTestParams{
            .test_name = "DirtyMissing",
            .platform_specific_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyMissing},
        BeaconConsistencyTestParams{
            .test_name = "CleanClean",
            .platform_specific_beacon_value = true,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kCleanClean},
        BeaconConsistencyTestParams{
            .test_name = "CleanDirty",
            .platform_specific_beacon_value = true,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kCleanDirty},
        BeaconConsistencyTestParams{
            .test_name = "DirtyClean",
            .platform_specific_beacon_value = false,
            .local_state_beacon_value = true,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyClean},
        BeaconConsistencyTestParams{
            .test_name = "DirtyDirty",
            .platform_specific_beacon_value = false,
            .local_state_beacon_value = false,
            .expected_consistency = CleanExitBeaconConsistency::kDirtyDirty}),
    [](const ::testing::TestParamInfo<BeaconConsistencyTestParams>& params) {
      return params.param.test_name;
    });

TEST_P(PlatformBeaconAndLocalStateBeaconConsistencyTest, BeaconConsistency) {
  // Clear the platform-specific and Local State beacons. Unless set below, the
  // beacons are considered missing.
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);

  BeaconConsistencyTestParams params = GetParam();
  if (params.platform_specific_beacon_value) {
    CleanExitBeacon::SetUserDefaultsBeacon(
        /*exited_cleanly=*/params.platform_specific_beacon_value.value());
  }
  if (params.local_state_beacon_value) {
    prefs_.SetBoolean(prefs::kStabilityExitedCleanly,
                      params.local_state_beacon_value.value());
  }

  TestCleanExitBeacon clean_exit_beacon(&prefs_);
  histogram_tester_.ExpectUniqueSample("UMA.CleanExitBeaconConsistency2",
                                       params.expected_consistency, 1);
}

// Verify that the logic for recording UMA.CleanExitBeaconConsistency3 is
// correct for clients in the Extended Variations Safe Mode experiment's enabled
// group.
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
      user_data_dir_path.Append(variations::kVariationsFilename);
  ASSERT_FALSE(base::PathExists(temp_beacon_file_path));
  // Clear the platform-specific beacon. Unless set below, this beacon is also
  // considered missing.
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);

  BeaconConsistencyTestParams params = GetParam();
  if (params.beacon_file_beacon_value) {
    ASSERT_LT(
        0, base::WriteFile(
               temp_beacon_file_path,
               CreateWellFormedBeaconFileContents(
                   /*exited_cleanly=*/params.beacon_file_beacon_value.value(),
                   /*crash_streak=*/0)
                   .data()));
  }
  if (params.platform_specific_beacon_value) {
    CleanExitBeacon::SetUserDefaultsBeacon(
        /*exited_cleanly=*/params.platform_specific_beacon_value.value());
  }

  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);
  ASSERT_EQ(variations::kEnabledGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  TestCleanExitBeacon clean_exit_beacon(&prefs_, user_data_dir_path);
  histogram_tester_.ExpectUniqueSample("UMA.CleanExitBeaconConsistency3",
                                       params.expected_consistency, 1);
}
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
TEST_F(CleanExitBeaconTest, EnabledGroupEmitsStageDurationMetric) {
  // Force the client into the Extended Variations Safe Mode experiment's
  // enabled group.
  SetUpExtendedSafeModeExperiment(variations::kEnabledGroup);

  // Create and initialize the CleanExitBeacon.
  TestCleanExitBeacon clean_exit_beacon(&prefs_);

  // Simulate Chrome starting to watch for browser crashes for enabled-group
  // clients.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/true);
  // Verify that the metric has not yet been emitted.
  histogram_tester_.ExpectTotalCount(
      "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration", 0);

  // Simulate Chrome continuing to watch for crashes once the app enters the
  // foreground.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/false);
  // Verify that the metric was emitted.
  histogram_tester_.ExpectTotalCount(
      "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration", 1);

  // Make the same call. Note that these two identical, consecutive calls to
  // WriteBeaconValue() shouldn't actually happen, but this is done for the
  // purpose of the test.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/false);
  // Verify that the metric was not emitted again.
  histogram_tester_.ExpectTotalCount(
      "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration", 1);
}

TEST_F(CleanExitBeaconTest, ControlGroupDoesNotEmitStageDurationMetric) {
  // Force the client into the Extended Variations Safe Mode experiment's
  // control group.
  SetUpExtendedSafeModeExperiment(variations::kControlGroup);

  // Create and initialize the CleanExitBeacon.
  TestCleanExitBeacon clean_exit_beacon(&prefs_);

  // Simulate Chrome starting to watch for browser crashes for control-group
  // clients once the app enters the foreground.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/false);
  // Verify that the metric was not emitted.
  histogram_tester_.ExpectTotalCount(
      "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration", 0);

  // Make the same call. Note that these two identical, consecutive calls to
  // WriteBeaconValue() shouldn't actually happen, but this is done for the
  // purpose of the test.
  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                                     /*is_extended_safe_mode=*/false);
  // Verify that the metric was not emitted.
  histogram_tester_.ExpectTotalCount(
      "UMA.CleanExitBeacon.ExtendedMonitoringStageDuration", 0);
}

#endif  //  BUILDFLAG(IS_ANDROID)

}  // namespace metrics
