// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment_storage.h"

#include "base/strings/string_piece.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/install_static/install_util.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/experiment.h"
#include "chrome/installer/util/experiment_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

// A test fixture that can tests saving experiment data in windows registry.
// Individual tests provide a parameter, which is true if Chrome is installed
// in system level.
class ExperimentStorageTest : public ::testing::TestWithParam<bool> {
 public:
  ExperimentStorageTest(const ExperimentStorageTest&) = delete;
  ExperimentStorageTest& operator=(const ExperimentStorageTest&) = delete;

 protected:
  ExperimentStorageTest()
      : system_level_install_(GetParam()),
        scoped_install_details_(system_level_install_, 0) {}

  HKEY root() {
    return system_level_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  }

  void SetUp() override {
    ::testing::TestWithParam<bool>::SetUp();
    ASSERT_NO_FATAL_FAILURE(override_manager_.OverrideRegistry(root()));

    // Create an empty participation key since participation registry is assumed
    // to be present for chrome build.
    base::win::RegKey key;
    ASSERT_EQ(
        ERROR_SUCCESS,
        key.Create(root(), install_static::GetClientStateKeyPath().c_str(),
                   KEY_WOW64_32KEY | KEY_QUERY_VALUE));
  }

  bool system_level_install_;

 private:
  install_static::ScopedInstallDetails scoped_install_details_;
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_P(ExperimentStorageTest, TestEncodeDecodeMetrics) {
  ExperimentMetrics metrics;
  metrics.state = ExperimentMetrics::kGroupAssigned;
  metrics.toast_location = ExperimentMetrics::kOverTaskbarPin;
  metrics.toast_count = 1;
  metrics.first_toast_offset_days = 30;
  metrics.toast_hour = 3;
  metrics.last_used_bucket = 2;
  metrics.action_delay_bucket = 11;
  metrics.session_length_bucket = 36;
  std::wstring encoded_metrics(ExperimentStorage::EncodeMetrics(metrics));
  EXPECT_EQ(L"5BIMD2IA", encoded_metrics);
  ExperimentMetrics decoded_metrics;
  ASSERT_TRUE(
      ExperimentStorage::DecodeMetrics(encoded_metrics, &decoded_metrics));
  EXPECT_EQ(metrics, decoded_metrics);
}

TEST_P(ExperimentStorageTest, TestEncodeDecodeForMax) {
  Experiment experiment;
  experiment.AssignGroup(ExperimentMetrics::kNumGroups - 1);
  experiment.SetToastLocation(ExperimentMetrics::kOverNotificationArea);
  experiment.SetInactiveDays(ExperimentMetrics::kMaxLastUsed);
  experiment.SetToastCount(ExperimentMetrics::kMaxToastCount);
  experiment.SetUserSessionUptime(
      base::Minutes(ExperimentMetrics::kMaxSessionLength));
  experiment.SetActionDelay(base::Seconds(ExperimentMetrics::kMaxActionDelay));
  experiment.SetDisplayTime(
      base::Time::UnixEpoch() +
      base::Seconds(ExperimentMetrics::kExperimentStartSeconds) +
      base::Days(ExperimentMetrics::kMaxFirstToastOffsetDays));
  experiment.SetState(ExperimentMetrics::kUserLogOff);  // Max state.
  ExperimentMetrics metrics = experiment.metrics();
  // toast_hour uses LocalMidnight whose value depend on local time. So, reset
  // it to its maximum value.
  metrics.toast_hour = 24;
  std::wstring encoded_metrics(ExperimentStorage::EncodeMetrics(metrics));
  EXPECT_EQ(L"///j//9B", encoded_metrics);
  ExperimentMetrics decoded_metrics;
  ASSERT_TRUE(
      ExperimentStorage::DecodeMetrics(encoded_metrics, &decoded_metrics));
  EXPECT_EQ(decoded_metrics.state, ExperimentMetrics::kUserLogOff);
  EXPECT_EQ(decoded_metrics.group, ExperimentMetrics::kNumGroups - 1);
  EXPECT_EQ(decoded_metrics.toast_location,
            ExperimentMetrics::kOverNotificationArea);
  EXPECT_EQ(decoded_metrics.toast_count, ExperimentMetrics::kMaxToastCount);
  EXPECT_EQ(decoded_metrics.first_toast_offset_days,
            ExperimentMetrics::kMaxFirstToastOffsetDays);
  EXPECT_EQ(decoded_metrics.toast_hour, 24);
  // Following are exponential buckets. So, there max value will be
  // 2^bits - 1
  EXPECT_EQ(decoded_metrics.last_used_bucket,
            (1 << ExperimentMetrics::kLastUsedBucketBits) - 1);
  EXPECT_EQ(decoded_metrics.action_delay_bucket,
            (1 << ExperimentMetrics::kActionDelayBucketBits) - 1);
  EXPECT_EQ(decoded_metrics.session_length_bucket,
            (1 << ExperimentMetrics::kSessionLengthBucketBits) - 1);
}

TEST_P(ExperimentStorageTest, TestEncodeDecodeForMin) {
  ExperimentMetrics metrics;
  metrics.state = ExperimentMetrics::kRelaunchFailed;
  std::wstring encoded_metrics(ExperimentStorage::EncodeMetrics(metrics));
  EXPECT_EQ(L"AAAAAAAA", encoded_metrics);
  ExperimentMetrics decoded_metrics;
  ASSERT_TRUE(
      ExperimentStorage::DecodeMetrics(encoded_metrics, &decoded_metrics));
  EXPECT_EQ(metrics, decoded_metrics);
}

TEST_P(ExperimentStorageTest, TestReadWriteParticipation) {
  ExperimentStorage storage;
  ExperimentStorage::Study expected = ExperimentStorage::kStudyOne;
  ASSERT_TRUE(storage.AcquireLock()->WriteParticipation(expected));
  ExperimentStorage::Study p;
  ASSERT_TRUE(storage.AcquireLock()->ReadParticipation(&p));
  EXPECT_EQ(expected, p);
}

TEST_P(ExperimentStorageTest, TestLoadStoreExperiment) {
  Experiment experiment;
  experiment.AssignGroup(5);
  ExperimentStorage storage;
  ASSERT_TRUE(storage.AcquireLock()->StoreExperiment(experiment));
  Experiment stored_experiment;
  ASSERT_TRUE(storage.AcquireLock()->LoadExperiment(&stored_experiment));
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned, stored_experiment.state());
  EXPECT_EQ(ExperimentMetrics::kGroupAssigned,
            stored_experiment.metrics().state);
  EXPECT_EQ(5, stored_experiment.group());
  // Verify that expeirment state is stored in correct location in registry.
  base::win::RegKey key;
  std::wstring client_state_path(
      system_level_install_ ? install_static::GetClientStateMediumKeyPath()
                            : install_static::GetClientStateKeyPath());
  client_state_path.append(L"\\Retention");
  EXPECT_EQ(ERROR_SUCCESS, key.Open(root(), client_state_path.c_str(),
                                    KEY_QUERY_VALUE | KEY_WOW64_32KEY));
}

TEST_P(ExperimentStorageTest, TestLoadStoreMetrics) {
  ExperimentStorage storage;
  ExperimentMetrics metrics;
  metrics.state = ExperimentMetrics::kGroupAssigned;
  metrics.toast_location = ExperimentMetrics::kOverTaskbarPin;
  metrics.toast_count = 1;
  metrics.first_toast_offset_days = 30;
  metrics.toast_hour = 3;
  metrics.last_used_bucket = 2;
  metrics.action_delay_bucket = 11;
  metrics.session_length_bucket = 36;
  ASSERT_TRUE(storage.AcquireLock()->StoreMetrics(metrics));
  ExperimentMetrics stored_metrics;
  ASSERT_TRUE(storage.AcquireLock()->LoadMetrics(&stored_metrics));
  EXPECT_EQ(L"5BIMD2IA", ExperimentStorage::EncodeMetrics(stored_metrics));
  // Verify that expeirment labels are stored in registry.
  EXPECT_EQ(metrics, stored_metrics);
}

INSTANTIATE_TEST_SUITE_P(UserLevel,
                         ExperimentStorageTest,
                         ::testing::Values(false));

INSTANTIATE_TEST_SUITE_P(SystemLevel,
                         ExperimentStorageTest,
                         ::testing::Values(true));

}  // namespace installer
