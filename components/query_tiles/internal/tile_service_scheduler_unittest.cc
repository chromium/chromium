// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_scheduler_impl.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/query_tiles/internal/black_hole_log_sink.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_store.h"
#include "components/query_tiles/switches.h"
#include "components/query_tiles/test/test_utils.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "net/base/backoff_entry_serializer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using ::testing::Invoke;

namespace query_tiles {
namespace {

constexpr net::BackoffEntry::Policy kTestPolicy = {
    0 /*num_errors_to_ignore*/,
    1000 /*init_delay_ms*/,
    2 /*multiply_factor*/,
    0 /*jitter_factor*/,
    4000 /*max_backoff_ms*/,
    -1 /*entry_lifetime_ms*/,
    false /*always_use_init_delay*/};

class MockBackgroundTaskScheduler
    : public background_task::BackgroundTaskScheduler {
 public:
  MockBackgroundTaskScheduler() = default;
  ~MockBackgroundTaskScheduler() override = default;
  MOCK_METHOD1(Schedule, bool(const background_task::TaskInfo& task_info));
  MOCK_METHOD1(Cancel, void(int));
};

class TileServiceSchedulerTest : public testing::Test {
 public:
  TileServiceSchedulerTest() = default;
  ~TileServiceSchedulerTest() override = default;

  void SetUp() override {
    base::Time fake_now;
    EXPECT_TRUE(base::Time::FromString("05/18/20 01:00:00 AM", &fake_now));
    clock_.SetNow(fake_now);
    query_tiles::RegisterPrefs(prefs()->registry());
    log_sink_ = std::make_unique<test::BlackHoleLogSink>();
    auto policy = std::make_unique<net::BackoffEntry::Policy>(kTestPolicy);
    tile_service_scheduler_ = std::make_unique<TileServiceSchedulerImpl>(
        &mocked_native_scheduler_, &prefs_, &clock_, &tick_clock_,
        std::move(policy), log_sink_.get());
    EXPECT_CALL(
        *native_scheduler(),
        Cancel(static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID)));
    tile_service_scheduler()->CancelTask();
  }

 protected:
  MockBackgroundTaskScheduler* native_scheduler() {
    return &mocked_native_scheduler_;
  }

  TileServiceScheduler* tile_service_scheduler() {
    return tile_service_scheduler_.get();
  }

  TestingPrefServiceSimple* prefs() { return &prefs_; }

  base::SimpleTestClock* clock() { return &clock_; }

  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }

  std::unique_ptr<net::BackoffEntry> GetBackoffPolicy() {
    const base::Value::List& value = prefs()->GetList(kBackoffEntryKey);
    return net::BackoffEntrySerializer::DeserializeFromList(
        value, &kTestPolicy, tick_clock(), clock()->Now());
  }

  void ResetTileServiceScheduler() {
    auto policy = std::make_unique<net::BackoffEntry::Policy>(kTestPolicy);
    tile_service_scheduler_ = std::make_unique<TileServiceSchedulerImpl>(
        &mocked_native_scheduler_, &prefs_, &clock_, &tick_clock_,
        std::move(policy), log_sink_.get());
  }

 private:
  base::test::TaskEnvironment task_environment_;

  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  TestingPrefServiceSimple prefs_;
  MockBackgroundTaskScheduler mocked_native_scheduler_;
  std::unique_ptr<LogSink> log_sink_;
  std::unique_ptr<TileServiceScheduler> tile_service_scheduler_;
};

MATCHER_P2(TaskInfoEq,
           min_window_start_time_ms,
           max_window_start_time_ms,
           "Verify window range in TaskInfo.") {
  EXPECT_TRUE(arg.one_off_info.has_value());
  EXPECT_GE(arg.one_off_info->window_start_time_ms, min_window_start_time_ms);
  EXPECT_LE(arg.one_off_info->window_start_time_ms, max_window_start_time_ms)
      << "Actual window start time in ms: "
      << arg.one_off_info->window_start_time_ms;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQueryTilesInstantBackgroundTask)) {
    EXPECT_EQ(arg.one_off_info->window_end_time_ms -
                  arg.one_off_info->window_start_time_ms,
              10 * 1000)
        << "Actual window end time in ms: "
        << arg.one_off_info->window_end_time_ms;
  } else {
    EXPECT_EQ(arg.one_off_info->window_end_time_ms -
                  arg.one_off_info->window_start_time_ms,
              TileConfig::GetOneoffTaskWindowInMs())
        << "Actual window end time in ms: "
        << arg.one_off_info->window_end_time_ms;
  }

  return true;
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuccess) {
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuccessInstantFetchOn) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      query_tiles::switches::kQueryTilesInstantBackgroundTask, "true");

  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedSuspend) {
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  tile_service_scheduler()->OnFetchCompleted(
      TileInfoRequestStatus::kShouldSuspend);
  auto backoff = GetBackoffPolicy();
  EXPECT_EQ(backoff->GetTimeUntilRelease().InMilliseconds(), 0);

  // Scheduler is in a suspended state, initializing the tile manager will not
  // schedule any tasks.
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);

  ResetTileServiceScheduler();
  // A task is rescheduled when scheduler is recreated.
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
}

// Verify the failure will add delay that using test backoff policy.
TEST_F(TileServiceSchedulerTest, OnFetchCompletedFailure) {
  for (int i = 0; i <= 2; i++) {
    EXPECT_CALL(*native_scheduler(),
                Schedule(TaskInfoEq(1000 * pow(2, i), 1000 * pow(2, i))));
    tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kFailure);
    auto backoff = GetBackoffPolicy();
    EXPECT_EQ(backoff->GetTimeUntilRelease().InMilliseconds(),
              1000 * pow(2, i));
  }
}

TEST_F(TileServiceSchedulerTest, OnFetchCompletedOtherStatus) {
  std::vector<TileInfoRequestStatus> other_status = {
      TileInfoRequestStatus::kInit};
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  for (const auto& status : other_status) {
    tile_service_scheduler()->OnFetchCompleted(status);
  }
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithNoTiles) {
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedInstantFetchOn) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      query_tiles::switches::kQueryTilesInstantBackgroundTask, "true");
  EXPECT_CALL(*native_scheduler(), Schedule(TaskInfoEq(10 * 1000, 10 * 1000)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kSuccess);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithFailure) {
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  tile_service_scheduler()->OnTileManagerInitialized(
      TileGroupStatus::kFailureDbOperation);

  // A task is rescheduled when scheduler is recreated.
  ResetTileServiceScheduler();
  auto expected_range_start = TileConfig::GetScheduleIntervalInMs();
  auto expected_range_end =
      expected_range_start + TileConfig::GetMaxRandomWindowInMs();
  EXPECT_CALL(*native_scheduler(),
              Schedule(TaskInfoEq(expected_range_start, expected_range_end)));
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
}

TEST_F(TileServiceSchedulerTest, OnTileGroupLoadedWithOtherStatus) {
  std::vector<TileGroupStatus> other_status = {TileGroupStatus::kUninitialized,
                                               TileGroupStatus ::kSuccess};
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  for (const auto status : other_status) {
    tile_service_scheduler()->OnTileManagerInitialized(status);
  }
}

// OnTileManagerInitialized(NoTiles) could be called many time before the first
// fetch task started. Ensure only the first one actually schedules the task,
// other calls should not override the existing task until it is executed and
// marked finished.
TEST_F(TileServiceSchedulerTest, FirstKickoffNotOverride) {
  // Verifying only schedule once also implying only the first schedule
  // call works.
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(1);
  auto now = clock()->Now();
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), now);
  auto two_hours_later = now + base::Hours(2);
  clock()->SetNow(two_hours_later);
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), now);

  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(1);
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), base::Time());
}

TEST_F(TileServiceSchedulerTest, FirstRunFinishedAfterInstantFetchComplete) {
  base::test::ScopedCommandLine scoped_command_line;
  auto now = clock()->Now();
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(1);
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), now);

  // Set instant-fetch flag to true after first-kickoff flow was marked and
  // scheduled, expecting the mark of first flow also being reset.
  scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
      query_tiles::switches::kQueryTilesInstantBackgroundTask, "true");
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(0);
  tile_service_scheduler()->OnFetchCompleted(TileInfoRequestStatus::kSuccess);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), base::Time());

  // Set instant-fetch flag to false after 2 hours. Chrome restarts with no
  // tiles, the scheduler should start a new first kickoff flow.
  scoped_command_line.GetProcessCommandLine()->RemoveSwitch(
      query_tiles::switches::kQueryTilesInstantBackgroundTask);
  auto two_hours_later = now + base::Hours(2);
  clock()->SetNow(two_hours_later);
  EXPECT_CALL(*native_scheduler(), Schedule(_)).Times(1);
  tile_service_scheduler()->OnTileManagerInitialized(TileGroupStatus::kNoTiles);
  EXPECT_EQ(prefs()->GetTime(kFirstScheduleTimeKey), two_hours_later);
}

}  // namespace
}  // namespace query_tiles
