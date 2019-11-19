// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_mock_clock_override.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

constexpr int kSecondsPerDay = 86400;
// Condition values start from this number for tests.
constexpr int kConditionValue = 11;

}  // namespace

// Test functions of CrOSActionRecorder.
class CrOSActionRecorderTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    profile_path_ = temp_dir_.GetPath();
    actions_ = {"Action0", "Action1", "Action2"};
    conditions_ = {"Condition0", "Condition1", "Condition2"};

    save_internal_secs_ = CrOSActionRecorder::kSaveInternal.InSeconds();

    // Set time_ to be base::Time::UnixEpoch().
    time_.Advance(base::TimeDelta::FromSeconds(11612937600));

    // Reset the CrOSActionRecorder to be default.
    CrOSActionRecorder::GetCrosActionRecorder()->actions_.Clear();
    CrOSActionRecorder::GetCrosActionRecorder()->last_save_timestamp_ =
        base::Time::UnixEpoch();
    CrOSActionRecorder::GetCrosActionRecorder()->should_log_ = false;
    CrOSActionRecorder::GetCrosActionRecorder()->should_hash_ = true;
    CrOSActionRecorder::GetCrosActionRecorder()->profile_path_ = profile_path_;
  }

  CrOSActionHistoryProto GetCrOSActionHistory() {
    return CrOSActionRecorder::GetCrosActionRecorder()->actions_;
  }

  void SetLogWithHash() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kEnableCrOSActionRecorder,
        ash::switches::kCrOSActionRecorderWithHash);

    CrOSActionRecorder::GetCrosActionRecorder()->SetCrOSActionRecorderType();
  }

  void SetLogWithoutHash() {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kEnableCrOSActionRecorder,
        ash::switches::kCrOSActionRecorderWithoutHash);

    CrOSActionRecorder::GetCrosActionRecorder()->SetCrOSActionRecorderType();
  }

  // Read and Parse log from |day|-th file.
  CrOSActionHistoryProto ReadLog(const int day) {
    const base::FilePath action_file_path =
        profile_path_.Append(CrOSActionRecorder::kActionHistoryDir)
            .Append(base::NumberToString(day));
    std::string proto_str;
    CHECK(base::ReadFileToString(action_file_path, &proto_str));
    CrOSActionHistoryProto actions_history;
    CHECK(actions_history.ParseFromString(proto_str));
    return actions_history;
  }

  // Expects |action| to be actions_[i], with certain features.
  void ExpectCrOSAction(const CrOSActionProto& action,
                        const int i,
                        const int64_t secs_since_epoch,
                        const bool should_hash = true) {
    EXPECT_EQ(action.action_name(),
              CrOSActionRecorder::MaybeHashed(actions_[i], should_hash));
    EXPECT_EQ(action.secs_since_epoch(), secs_since_epoch);
    EXPECT_EQ(action.conditions_size(), 1);
    const CrOSActionProto::CrOSActionConditionProto& condition =
        action.conditions(0);
    EXPECT_EQ(condition.name(),
              CrOSActionRecorder::MaybeHashed(conditions_[i], should_hash));
    EXPECT_EQ(condition.value(), i + kConditionValue);
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  std::vector<std::string> actions_;
  std::vector<std::string> conditions_;
  base::ScopedTempDir temp_dir_;
  base::FilePath profile_path_;
  int64_t save_internal_secs_ = 0;
  base::ScopedMockClockOverride time_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

// Log is disabled by default.
TEST_F(CrOSActionRecorderTest, NoLogAsDefault) {
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction({actions_[0]});
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
}

// Log is hashed if CrOSActionRecorderType == 1.
TEST_F(CrOSActionRecorderTest, HashActionNameAndConditionName) {
  SetLogWithHash();
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[0]}, {{conditions_[0], kConditionValue}});
  const CrOSActionHistoryProto& action_history = GetCrOSActionHistory();
  EXPECT_EQ(action_history.actions_size(), 1);

  ExpectCrOSAction(action_history.actions(0), 0, 0);
}

// Log action name and condition name explicitly if
// CrOSActionRecorderType == 2.
TEST_F(CrOSActionRecorderTest, DisableHashToLogExplicitly) {
  SetLogWithoutHash();
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[0]}, {{conditions_[0], kConditionValue}});
  const CrOSActionHistoryProto& action_history = GetCrOSActionHistory();
  EXPECT_EQ(action_history.actions_size(), 1);
  ExpectCrOSAction(action_history.actions(0), 0, 0, false);
}

// Check a new file is written every day for expected values.
TEST_F(CrOSActionRecorderTest, WriteToNewFileEveryDay) {
  SetLogWithHash();
  time_.Advance(base::TimeDelta::FromSeconds(save_internal_secs_));
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[0]}, {{conditions_[0], kConditionValue}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values.
  const CrOSActionHistoryProto action_history_0 = ReadLog(0);
  EXPECT_EQ(action_history_0.actions_size(), 1);
  ExpectCrOSAction(action_history_0.actions(0), 0, save_internal_secs_);

  // Advance for 1 day.
  time_.Advance(base::TimeDelta::FromSeconds(kSecondsPerDay));
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[1]}, {{conditions_[1], kConditionValue + 1}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the new log file to have correct values.
  const CrOSActionHistoryProto action_history_1 = ReadLog(1);
  EXPECT_EQ(action_history_1.actions_size(), 1);
  ExpectCrOSAction(action_history_1.actions(0), 1,
                   save_internal_secs_ + kSecondsPerDay);
}

// Check that the result is appended to previous log within a day.
TEST_F(CrOSActionRecorderTest, AppendToFileEverySaveInAday) {
  SetLogWithHash();
  time_.Advance(base::TimeDelta::FromSeconds(save_internal_secs_));
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[0]}, {{conditions_[0], kConditionValue}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log has correct values.
  const CrOSActionHistoryProto action_history_0 = ReadLog(0);
  EXPECT_EQ(action_history_0.actions_size(), 1);
  ExpectCrOSAction(action_history_0.actions(0), 0, save_internal_secs_);

  // Advance for 1 kSaveInternal.
  time_.Advance(base::TimeDelta::FromSeconds(save_internal_secs_));
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[1]}, {{conditions_[1], kConditionValue + 1}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values (two actions).
  const CrOSActionHistoryProto action_history_1 = ReadLog(0);
  EXPECT_EQ(action_history_1.actions_size(), 2);
  ExpectCrOSAction(action_history_1.actions(0), 0, save_internal_secs_);
  ExpectCrOSAction(action_history_1.actions(1), 1, save_internal_secs_ * 2);

  // Advance for 3 kSaveInternal.
  time_.Advance(base::TimeDelta::FromSeconds(save_internal_secs_ * 3));
  CrOSActionRecorder::GetCrosActionRecorder()->RecordAction(
      {actions_[2]}, {{conditions_[2], kConditionValue + 2}});
  Wait();

  // Expect the GetCrOSActionHistory() is already cleared.
  EXPECT_TRUE(GetCrOSActionHistory().actions().empty());
  // Expect the log to have correct values (three actions).
  const CrOSActionHistoryProto action_history_2 = ReadLog(0);
  EXPECT_EQ(action_history_2.actions_size(), 3);
  ExpectCrOSAction(action_history_1.actions(0), 0, save_internal_secs_);
  ExpectCrOSAction(action_history_1.actions(1), 1, save_internal_secs_ * 2);
  ExpectCrOSAction(action_history_2.actions(2), 2, save_internal_secs_ * 5);
}

}  // namespace app_list
