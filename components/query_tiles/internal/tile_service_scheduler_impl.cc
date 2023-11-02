// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_scheduler_impl.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/rand_util.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "components/query_tiles/internal/stats.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/switches.h"
#include "net/base/backoff_entry_serializer.h"

namespace query_tiles {
namespace {

// Schedule window params in instant schedule mode.
const int kInstantScheduleWindowStartMs = 10 * 1000;  // 10 seconds
const int kInstantScheduleWindowEndMs = 20 * 1000;    // 20 seconds

bool IsInstantFetchMode() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kQueryTilesInstantBackgroundTask);
}

}  // namespace

TileServiceSchedulerImpl::TileServiceSchedulerImpl(
    background_task::BackgroundTaskScheduler* scheduler,
    PrefService* prefs,
    base::Clock* clock,
    const base::TickClock* tick_clock,
    std::unique_ptr<net::BackoffEntry::Policy> backoff_policy,
    LogSink* log_sink)
    : scheduler_(scheduler),
      prefs_(prefs),
      clock_(clock),
      tick_clock_(tick_clock),
      backoff_policy_(std::move(backoff_policy)),
      is_suspend_(false),
      delegate_(nullptr),
      fetcher_status_(TileInfoRequestStatus::kInit),
      group_status_(TileGroupStatus::kUninitialized),
      log_sink_(log_sink) {}

TileServiceSchedulerImpl::~TileServiceSchedulerImpl() = default;

void TileServiceSchedulerImpl::CancelTask() {
  scheduler_->Cancel(
      static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID));
  ResetBackoff();
  MarkFirstRunFinished();
}

void TileServiceSchedulerImpl::OnFetchStarted() {
  fetcher_status_ = TileInfoRequestStatus::kInit;
  if (!IsInstantFetchMode()) {
    base::Time::Exploded local_explode;
    base::Time::Now().LocalExplode(&local_explode);
    stats::RecordExplodeOnFetchStarted(local_explode.hour);
  }
}

void TileServiceSchedulerImpl::OnFetchCompleted(TileInfoRequestStatus status) {
  auto first_schedule_time = prefs_->GetTime(kFirstScheduleTimeKey);
  MarkFirstRunFinished();
  fetcher_status_ = status;

  if (IsInstantFetchMode())
    return;

  // If this task was marked at first attempting flow, record the duration, and
  // mark the flow is finished now.
  if (first_schedule_time != base::Time()) {
    auto hours_past = (clock_->Now() - first_schedule_time).InHours();
    if (hours_past >= 0) {
      stats::RecordFirstFetchFlowDuration(hours_past);
    }
  }

  if (status == TileInfoRequestStatus::kShouldSuspend) {
    ResetBackoff();
    is_suspend_ = true;
  } else if (status == TileInfoRequestStatus::kFailure) {
    AddBackoff();
    ScheduleTask(false);
  } else if (status == TileInfoRequestStatus::kSuccess && !is_suspend_) {
    ResetBackoff();
    ScheduleTask(true);
  }
  stats::RecordTileRequestStatus(status);
  PingLogSink();
}

void TileServiceSchedulerImpl::OnDbPurged(TileGroupStatus status) {
  CancelTask();
  group_status_ = status;
  PingLogSink();
}

void TileServiceSchedulerImpl::OnGroupDataSaved(TileGroupStatus status) {
  group_status_ = status;
  PingLogSink();
}

void TileServiceSchedulerImpl::OnTileManagerInitialized(
    TileGroupStatus status) {
  group_status_ = status;

  if (IsInstantFetchMode()) {
    ResetBackoff();
    ScheduleTask(true);
    return;
  }

  if (status == TileGroupStatus::kNoTiles && !is_suspend_ &&
      !IsDuringFirstFlow()) {
    ResetBackoff();
    ScheduleTask(true);
    MarkFirstRunScheduled();
  } else if (status == TileGroupStatus::kFailureDbOperation) {
    ResetBackoff();
    is_suspend_ = true;
  }
  stats::RecordTileGroupStatus(status);
  PingLogSink();
}

TileInfoRequestStatus TileServiceSchedulerImpl::GetFetcherStatus() {
  return fetcher_status_;
}

TileGroupStatus TileServiceSchedulerImpl::GetGroupStatus() {
  return group_status_;
}

TileGroup* TileServiceSchedulerImpl::GetTileGroup() {
  return delegate_ ? delegate_->GetTileGroup() : nullptr;
}

std::unique_ptr<net::BackoffEntry> TileServiceSchedulerImpl::GetBackoff() {
  const base::Value::List& value = prefs_->GetList(kBackoffEntryKey);
  std::unique_ptr<net::BackoffEntry> result =
      net::BackoffEntrySerializer::DeserializeFromList(
          value, backoff_policy_.get(), tick_clock_, clock_->Now());
  if (!result) {
    return std::make_unique<net::BackoffEntry>(backoff_policy_.get(),
                                               tick_clock_);
  }
  return result;
}

void TileServiceSchedulerImpl::ScheduleTask(bool is_init_schedule) {
  background_task::OneOffInfo one_off_task_info;
  if (IsInstantFetchMode()) {
    GetInstantTaskWindow(&one_off_task_info.window_start_time_ms,
                         &one_off_task_info.window_end_time_ms,
                         is_init_schedule);
  } else {
    GetTaskWindow(&one_off_task_info.window_start_time_ms,
                  &one_off_task_info.window_end_time_ms, is_init_schedule);
  }
  background_task::TaskInfo task_info(
      static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID),
      one_off_task_info);
  task_info.is_persisted = true;
  task_info.update_current = true;
  task_info.network_type =
      TileConfig::GetIsUnMeteredNetworkRequired()
          ? background_task::TaskInfo::NetworkType::UNMETERED
          : background_task::TaskInfo::NetworkType::ANY;
  scheduler_->Schedule(task_info);
}

void TileServiceSchedulerImpl::AddBackoff() {
  std::unique_ptr<net::BackoffEntry> current = GetBackoff();
  current->InformOfRequest(false);
  UpdateBackoff(current.get());
}

void TileServiceSchedulerImpl::ResetBackoff() {
  std::unique_ptr<net::BackoffEntry> current = GetBackoff();
  current->Reset();
  UpdateBackoff(current.get());
}

int64_t TileServiceSchedulerImpl::GetDelaysFromBackoff() {
  return GetBackoff()->GetTimeUntilRelease().InMilliseconds();
}

// Schedule next task in next 10 to 20 seconds in debug mode.
void TileServiceSchedulerImpl::GetInstantTaskWindow(int64_t* start_time_ms,
                                                    int64_t* end_time_ms,
                                                    bool is_init_schedule) {
  if (is_init_schedule) {
    *start_time_ms = kInstantScheduleWindowStartMs;
  } else {
    *start_time_ms = GetDelaysFromBackoff();
  }
  *end_time_ms = kInstantScheduleWindowEndMs;
}

void TileServiceSchedulerImpl::GetTaskWindow(int64_t* start_time_ms,
                                             int64_t* end_time_ms,
                                             bool is_init_schedule) {
  if (is_init_schedule) {
    *start_time_ms = TileConfig::GetScheduleIntervalInMs() +
                     base::RandGenerator(TileConfig::GetMaxRandomWindowInMs());
  } else {
    *start_time_ms = GetDelaysFromBackoff();
  }
  *end_time_ms = *start_time_ms + TileConfig::GetOneoffTaskWindowInMs();
}

void TileServiceSchedulerImpl::UpdateBackoff(net::BackoffEntry* backoff) {
  base::Value::List serialized =
      net::BackoffEntrySerializer::SerializeToList(*backoff, clock_->Now());
  prefs_->SetList(kBackoffEntryKey, std::move(serialized));
}

void TileServiceSchedulerImpl::MarkFirstRunScheduled() {
  prefs_->SetTime(kFirstScheduleTimeKey, clock_->Now());
}

void TileServiceSchedulerImpl::MarkFirstRunFinished() {
  prefs_->SetTime(kFirstScheduleTimeKey, base::Time());
}

bool TileServiceSchedulerImpl::IsDuringFirstFlow() {
  return prefs_->GetTime(kFirstScheduleTimeKey) != base::Time();
}

void TileServiceSchedulerImpl::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void TileServiceSchedulerImpl::PingLogSink() {
  log_sink_->OnServiceStatusChanged();
  log_sink_->OnTileDataAvailable();
}

}  // namespace query_tiles
