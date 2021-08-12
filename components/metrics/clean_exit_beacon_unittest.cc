// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/clean_exit_beacon.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_field_trial_list_resetter.h"
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
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {
namespace {

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

struct BeaconTestParams {
  const std::string test_name;
  bool user_data_dir_exists;
  bool variations_file_exists;
  const std::string beacon_file_contents;
};

class CleanExitBeaconParameterizedTest
    : public CleanExitBeaconTest,
      public testing::WithParamInterface<BeaconTestParams> {};

// Verify that the crash streak metric is 0 when default pref values are used.
TEST_F(CleanExitBeaconTest, CrashStreakMetricWithDefaultPrefs) {
  CleanExitBeacon::ResetStabilityExitedCleanlyForTesting(&prefs_);
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
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
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
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
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

// Verify that the crash streak is correctly incremented and recorded when the
// last Chrome session did not exit cleanly.
TEST_F(CleanExitBeaconTest, CrashIncrementsCrashStreak) {
  CleanExitBeacon::SetStabilityExitedCleanlyForTesting(&prefs_, false);
  prefs_.SetInteger(variations::prefs::kVariationsCrashStreak, 1);
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
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
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
  EXPECT_EQ(prefs_.GetInteger(variations::prefs::kVariationsCrashStreak), 1);
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes", 1,
                                       1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CleanExitBeaconParameterizedTest,
    ::testing::Values(
        BeaconTestParams{.test_name = "NoUserDataDir",
                         .user_data_dir_exists = false,
                         .variations_file_exists = false,
                         .beacon_file_contents = ""},
        BeaconTestParams{.test_name = "NoVariationsFile",
                         .user_data_dir_exists = true,
                         .variations_file_exists = false,
                         .beacon_file_contents = ""},
        BeaconTestParams{.test_name = "EmptyVariationsFile",
                         .user_data_dir_exists = true,
                         .variations_file_exists = true,
                         .beacon_file_contents = ""},
        BeaconTestParams{.test_name = "NotDictionary",
                         .user_data_dir_exists = true,
                         .variations_file_exists = true,
                         .beacon_file_contents = "{abc123"},
        BeaconTestParams{.test_name = "EmptyDictionary",
                         .user_data_dir_exists = true,
                         .variations_file_exists = true,
                         .beacon_file_contents = "{}"},
        BeaconTestParams{
            .test_name = "MissingCrashStreak",
            .user_data_dir_exists = true,
            .variations_file_exists = true,
            .beacon_file_contents =
                "{\"user_experience_metrics.stability.exited_cleanly\": true}"},
        BeaconTestParams{
            .test_name = "MissingBeacon",
            .user_data_dir_exists = true,
            .variations_file_exists = true,
            .beacon_file_contents = "{\"variations_crash_streak\": 1}"}),
    [](const ::testing::TestParamInfo<BeaconTestParams>& params) {
      return params.param.test_name;
    });

// Verify that the inability to get the Variations Safe Mode file's contents for
// a plethora of reasons (a) doesn't crash and (b) correctly records the
// GotVariationsFileContents metric.
TEST_P(CleanExitBeaconParameterizedTest, CtorWithUnusableVariationsFile) {
  BeaconTestParams params = GetParam();

  const base::FilePath user_data_dir_path = user_data_dir_.GetPath();
  if (params.variations_file_exists) {
    const base::FilePath temp_beacon_file_path =
        user_data_dir_path.Append(variations::kVariationsFilename);
    ASSERT_LT(0, base::WriteFile(temp_beacon_file_path,
                                 params.beacon_file_contents.data()));
  }

  CleanExitBeacon clean_exit_beacon(
      kDummyWindowsRegistryKey,
      params.user_data_dir_exists ? user_data_dir_path : base::FilePath(),
      &prefs_);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", false, 1);
}

// Verify that successfully reading the Variations Safe Mode file's contents
// results in correctly (a) setting the |did_previous_session_exit_cleanly_|
// field and (b) recording metrics when the last session exited cleanly.
TEST_F(CleanExitBeaconTest, CtorWithVariationsFile) {
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

  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey,
                                    user_data_dir_path, &prefs_);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_TRUE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       num_crashes, 1);
}

// Verify that successfully reading the Variations Safe Mode file's contents
// results in correctly (a) setting the |did_previous_session_exit_cleanly_|
// field and (b) recording metrics when the last session did not exit cleanly.
TEST_F(CleanExitBeaconTest, CtorWithCrashAndVariationsFile) {
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
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey,
                                    user_data_dir_path, &prefs_);
  histogram_tester_.ExpectUniqueSample(
      "Variations.ExtendedSafeMode.GotVariationsFileContents", true, 1);
  EXPECT_FALSE(clean_exit_beacon.exited_cleanly());
  histogram_tester_.ExpectUniqueSample("Variations.SafeMode.Streak.Crashes",
                                       updated_num_crashes, 1);
}

// Verify that attempting to write synchronously is a no-op for clients that do
// not belong to specific Extended Variations Safe Mode experiment groups.
TEST_F(CleanExitBeaconTest, WriteBeaconValue_NoopSynchronousWrite) {
  base::test::ScopedFieldTrialListResetter resetter;
  base::FieldTrialList field_trial_list(
      std::make_unique<base::MockEntropyProvider>(0.1));
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          variations::kExtendedSafeModeTrial, 100, variations::kDefaultGroup,
          base::FieldTrial::ONE_TIME_RANDOMIZED, nullptr));
  trial->AppendGroup(variations::kControlGroup, 100);
  trial->SetForced();
  ASSERT_EQ(variations::kControlGroup, base::FieldTrialList::FindFullName(
                                           variations::kExtendedSafeModeTrial));

  PrefServiceFactory factory;
  scoped_refptr<FakeTestingPrefStore> pref_store(new FakeTestingPrefStore);
  factory.set_user_prefs(pref_store);
  scoped_refptr<PrefRegistrySimple> registry(new PrefRegistrySimple);
  std::unique_ptr<PrefService> prefs(factory.Create(registry.get()));
  CleanExitBeacon::RegisterPrefs(registry.get());
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey,
                                    user_data_dir_.GetPath(), prefs.get());

  clean_exit_beacon.WriteBeaconValue(/*exited_cleanly=*/false,
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
  CleanExitBeacon clean_exit_beacon(kDummyWindowsRegistryKey, base::FilePath(),
                                    &prefs_);
  bool exited_cleanly = false;
  bool write_synchronously = false;

  ASSERT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
  clean_exit_beacon.WriteBeaconValue(exited_cleanly, write_synchronously,
                                     /*update_beacon=*/false);
  // Verify that the beacon is not changed when update_beacon is false.
  EXPECT_TRUE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));

  clean_exit_beacon.WriteBeaconValue(exited_cleanly, write_synchronously,
                                     /*update_beacon=*/true);
  // Verify that the beacon is changed when update_beacon is true.
  EXPECT_FALSE(prefs_.GetBoolean(prefs::kStabilityExitedCleanly));
}

}  // namespace metrics
