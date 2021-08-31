// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <string>
#include <vector>

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

namespace metrics {
namespace {

using ::variations::SetUpExtendedSafeModeExperiment;
using ::variations::prefs::kVariationsCrashStreak;

const wchar_t kDummyWindowsRegistryKey[] = L"";

}  // namespace

class FakeTestingPrefStore : public TestingPrefStore {
 public:
  void CommitPendingWriteSynchronously() override {
    was_commit_pending_write_synchronously_called_ = true;
  }

  bool was_commit_pending_write_synchronously_called() {
    return was_commit_pending_write_synchronously_called_;
  }

 protected:
  ~FakeTestingPrefStore() override = default;

 private:
  bool was_commit_pending_write_synchronously_called_ = false;
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

class BeaconFileTest : public testing::WithParamInterface<std::string>,
                       public CleanExitBeaconTest {};

struct BeaconTestParams {
  const std::string test_name;
  bool beacon_file_exists;
  const std::string beacon_file_contents;
};

class BadBeaconFileTest : public testing::WithParamInterface<BeaconTestParams>,
                          public CleanExitBeaconTest {};

// Verify that the crash streak metric is 0 when default pref values are used.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithDefaultPrefs) {
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
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
  prefs_.SetInteger(kVariationsCrashStreak, 0);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 0,
                                       1);
}

// Verify that the crash streak metric is correctly recorded when there is a
// non-zero crash streak.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithSomeCrashes) {
  // The default value for kStabilityExitedCleanly is true, but defaults can
  // change, so we explicitly set it to true here.
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(kVariationsCrashStreak, 1);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly.
TEST_F(CleanExitBeaconTest, CrashIncrementsCrashStreak) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  prefs_.SetInteger(kVariationsCrashStreak, 1);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
  EXPECT_EQ(prefs_.GetInteger(kVariationsCrashStreak), 2);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 2,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly and the default crash streak value
// is used.
TEST_F(CleanExitBeaconTest,
       CrashIncrementsCrashStreakWithDefaultCrashStreakPref) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
  EXPECT_EQ(prefs_.GetInteger(kVariationsCrashStreak), 1);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that assigning a client to the default group DCHECKs. The experiment
// has four experiment groups of equal weight, so no clients should be in the
// default group.
TEST_F(CleanExitBeaconTest, DefaultGroupAssignmentChecks) {
  SetUpExtendedSafeModeExperiment(variations::kDefaultGroup);
  EXPECT_DCHECK_DEATH(CleanExitBeacon(kDummyWindowsRegistryKey,
                                      base::FilePath(), &prefs_,
                                      version_info::Channel::CANARY));
}

// Verify that (a) the client is excluded from the Extended Variations Safe Mode
// experiment and (b) no attempt is made to read the beacon file when no user
// data dir is provided.
TEST_F(CleanExitBeaconTest, CtorWithoutUserDataDir) {
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::CANARY);
  EXPECT_FALSE(
      base::FieldTrialList::IsTrialActive(variations::kExtendedSafeModeTrial));
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", 0);
}

INSTANTIATE_TEST_SUITE_P(
    CleanExitBeaconParameterizedTests,
    BeaconFileTest,
    ::testing::Values(
        variations::kControlGroup,
        variations::kSignalAndWriteSynchronouslyViaPrefServiceGroup,
        variations::kWriteSynchronouslyViaPrefServiceGroup));

// Verify that the beacon file is not read when the client is not in the
// SignalAndWriteViaFileUtil experiment group. It is possible for a client to
// have the file and to not be in the SignalAndWriteViaFileUtil group when the
// client was in the group in a previous session and then switched groups, e.g.
// via kResetVariationState.
TEST_P(BeaconFileTest, FileIgnoredBySomeExperimentGroups) {
  // Deliberately set the prefs so that we can later verify that their values
  // have not changed.
  int expected_crash_streak = 0;
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  prefs_.SetInteger(kVariationsCrashStreak, expected_crash_streak);

  // Prepare a well-formed beacon file, which we expect to be ignored. (If it
  // were used, then the prefs' values would change.)
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const std::string contents =
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\": false,\n"
      "  \"variations_crash_streak\": 2\n"
      "}";
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path, contents.data()));

  const std::string group_name = GetParam();
  SetUpExtendedSafeModeExperiment(group_name);
  ASSERT_EQ(group_name, base::FieldTrialList::FindFullName(
                            variations::kExtendedSafeModeTrial));
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, user_data_dir_path, &prefs_,
                         version_info::Channel::CANARY);

  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  EXPECT_EQ(prefs_.GetInteger(kVariationsCrashStreak), expected_crash_streak);
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", 0);
}

INSTANTIATE_TEST_SUITE_P(
    CleanExitBeaconParameterizedTests,
    BadBeaconFileTest,
    ::testing::Values(
        BeaconTestParams{.test_name = "NoBeaconFile",
                         .beacon_file_exists = false,
                         .beacon_file_contents = ""},
        BeaconTestParams{.test_name = "EmptyBeaconFile",
                         .beacon_file_exists = true,
                         .beacon_file_contents = ""},
        BeaconTestParams{.test_name = "NotDictionary",
                         .beacon_file_exists = true,
                         .beacon_file_contents = "{abc123"},
        BeaconTestParams{.test_name = "EmptyDictionary",
                         .beacon_file_exists = true,
                         .beacon_file_contents = "{}"},
        BeaconTestParams{
            .test_name = "MissingCrashStreak",
            .beacon_file_exists = true,
            .beacon_file_contents =
                "{\"user_experience_metrics.stability.exited_cleanly\": true}"},
        BeaconTestParams{
            .test_name = "MissingBeacon",
            .beacon_file_exists = true,
            .beacon_file_contents = "{\"variations_crash_streak\": 1}"}),
    [](const ::testing::TestParamInfo<BeaconTestParams>& params) {
      return params.param.test_name;
    });

// Verify that the inability to get the beacon file's contents for a plethora of
// reasons (a) doesn't crash and (b) correctly records the
// GotVariationsFileContents metric. (VariationsFile refers to the beacon file.)
// The file is read only for clients in the SignalAndWriteViaFileUtil group.
TEST_P(BadBeaconFileTest, CtorWithUnusableFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  BeaconTestParams params = GetParam();

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  if (params.beacon_file_exists) {
    const base::FilePath temp_beacon_file_path =
        user_data_dir_path.Append(variations::kVariationsFilename);
    ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                                 params.beacon_file_contents.data()));
  }

  CleanExitBeacon beacon(kDummyWindowsRegistryKey, user_data_dir_path, &prefs_,
                         version_info::Channel::CANARY);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", false, 1);
}

// Verify that successfully reading the Variations Safe Mode file's contents
// results in correctly (a) setting the |did_previous_session_exit_cleanly_|
// field and (b) recording metrics when the last session exited cleanly. The
// file is read only for clients in the SignalAndWriteViaFileUtil group.
TEST_F(CleanExitBeaconTest, CtorWithBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int num_crashes = 2;
  const std::string contents = base::StringPrintf(
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\": true,\n"
      "  \"variations_crash_streak\": %s\n"
      "}",
      base::NumberToString(num_crashes).data());
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path, contents.data()));

  CleanExitBeacon beacon(kDummyWindowsRegistryKey, user_data_dir_path, &prefs_,
                         version_info::Channel::CANARY);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_TRUE(beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       num_crashes, 1);
}

// Verify that successfully reading the Variations Safe Mode file's contents
// results in correctly (a) setting the |did_previous_session_exit_cleanly_|
// field and (b) recording metrics when the last session did not exit cleanly.
// The file is read only for clients in the SignalAndWriteViaFileUtil group.
TEST_F(CleanExitBeaconTest, CtorWithCrashAndBeaconFile) {
  SetUpExtendedSafeModeExperiment(variations::kSignalAndWriteViaFileUtilGroup);
  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  const base::FilePath temp_beacon_file_path =
      user_data_dir_path.Append(variations::kVariationsFilename);
  const int last_session_num_crashes = 2;
  const std::string contents = base::StringPrintf(
      "{\n"
      "  \"user_experience_metrics.stability.exited_cleanly\": false,\n"
      "  \"variations_crash_streak\": %s\n"
      "}",
      base::NumberToString(last_session_num_crashes).data());
  ASSERT_LT(0, base::WriteFile(temp_beacon_file_path, contents.data()));

  const int updated_num_crashes = last_session_num_crashes + 1;
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, user_data_dir_path, &prefs_,
                         version_info::Channel::CANARY);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_FALSE(beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       updated_num_crashes, 1);
}

// Verify that attempting to write synchronously is a no-op for clients that do
// not belong to specific Extended Variations Safe Mode experiment groups.
TEST_F(CleanExitBeaconTest, WriteBeaconValue_NoopSynchronousWrite) {
  SetUpExtendedSafeModeExperiment(variations::kControlGroup);
  ASSERT_EQ(variations::kControlGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  PrefServiceFactory factory;
  scoped_refptr<FakeTestingPrefStore> pref_store(new FakeTestingPrefStore);
  factory.set_user_prefs(pref_store);
  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple);
  std::unique_ptr<PrefService> prefs(factory.Create(registry.get()));
  CleanExitBeacon::RegisterPrefs(registry.get());
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, user_data_dir_.GetPath(),
                         prefs.get(), version_info::Channel::UNKNOWN);

  beacon.WriteBeaconValue(/*exited_cleanly=*/false,
                          /*write_synchronously=*/true);

  // Verify that CommitPendingWriteSynchronously() was not called and that
  // the WritePrefsTime metric was not emitted.
  EXPECT_FALSE(pref_store->was_commit_pending_write_synchronously_called());
  histogram_tester_.ExpectTotalCount(
      "Variations.ExtendedSafeMode.WritePrefsTime", 0);
}

// Verify that calling WriteBeaconValue() with update_beacon=false does not
// update the beacon. Also, verify that using update_beacon=true updates the
// beacon.
TEST_F(CleanExitBeaconTest, WriteBeaconValue_UpdateBeacon) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, true);
  CleanExitBeacon beacon(kDummyWindowsRegistryKey, base::FilePath(), &prefs_,
                         version_info::Channel::UNKNOWN);
  bool exited_cleanly = false;
  bool write_synchronously = false;

  ASSERT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  beacon.WriteBeaconValue(exited_cleanly, write_synchronously,
                          /*update_beacon=*/false);
  // Verify that the beacon is not changed when update_beacon is false.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));

  beacon.WriteBeaconValue(exited_cleanly, write_synchronously,
                          /*update_beacon=*/true);
  // Verify that the beacon is changed when update_beacon is true.
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
}

}  // namespace metrics
