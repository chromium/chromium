// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_IMPL_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/query_tiles/internal/log_source.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_service_scheduler.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "net/base/backoff_entry_serializer.h"

namespace background_task {
class BackgroundTaskScheduler;
}  // namespace background_task

class PrefService;

namespace query_tiles {

// An implementation of TileServiceScheduler interface and LogSource interface.
class TileServiceSchedulerImpl : public TileServiceScheduler, public LogSource {
 public:
  TileServiceSchedulerImpl(
      background_task::BackgroundTaskScheduler* scheduler,
      PrefService* prefs,
      base::Clock* clock,
      const base::TickClock* tick_clock,
      std::unique_ptr<net::BackoffEntry::Policy> backoff_policy,
      LogSink* log_sink);

  ~TileServiceSchedulerImpl() override;

 private:
  // TileServiceScheduler implementation.
  void CancelTask() override;
  void OnFetchStarted() override;
  void OnFetchCompleted(TileInfoRequestStatus status) override;
  void OnTileManagerInitialized(TileGroupStatus status) override;
  void OnDbPurged(TileGroupStatus status) override;
  void OnGroupDataSaved(TileGroupStatus status) override;
  void SetDelegate(Delegate* delegate) override;

  // LogSource implementation.
  TileInfoRequestStatus GetFetcherStatus() override;
  TileGroupStatus GetGroupStatus() override;
  TileGroup* GetTileGroup() override;

  void ScheduleTask(bool is_init_schedule);
  std::unique_ptr<net::BackoffEntry> GetBackoff();
  void AddBackoff();
  void ResetBackoff();
  int64_t GetDelaysFromBackoff();
  void GetInstantTaskWindow(int64_t* start_time_ms,
                            int64_t* end_time_ms,
                            bool is_init_schedule);
  void GetTaskWindow(int64_t* start_time_ms,
                     int64_t* end_time_ms,
                     bool is_init_schedule);
  void UpdateBackoff(net::BackoffEntry* backoff);
  void MarkFirstRunScheduled();
  void MarkFirstRunFinished();

  // Returns true if the initial task has been scheduled because no tiles in
  // db(kickoff condition), but still waiting to be completed at the designated
  // window. Returns false either the first task is not scheduled yet or it is
  // already finished.
  bool IsDuringFirstFlow();

  // Ping the log sink to update.
  void PingLogSink();

  // Native Background Scheduler instance.
  raw_ptr<background_task::BackgroundTaskScheduler> scheduler_;

  raw_ptr<PrefService> prefs_;

  // Clock object to get current time.
  raw_ptr<base::Clock> clock_;

  // TickClock used for building backoff entry.
  raw_ptr<const base::TickClock> tick_clock_;

  // Backoff policy used for reschdule.
  std::unique_ptr<net::BackoffEntry::Policy> backoff_policy_;

  // Flag to indicate if currently have a suspend status to avoid overwriting if
  // already scheduled a suspend task during this lifecycle.
  bool is_suspend_;

  // Delegate instance.
  raw_ptr<Delegate> delegate_;

  // Internal fetcher status.
  TileInfoRequestStatus fetcher_status_;

  // Internal group status.
  TileGroupStatus group_status_;

  // LogSink instance.
  raw_ptr<LogSink> log_sink_;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_IMPL_H_
