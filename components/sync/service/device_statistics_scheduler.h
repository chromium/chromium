// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_
#define COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/wall_clock_timer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

struct CoreAccountInfo;
class PrefRegistrySimple;
class PrefService;

namespace syncer {

class DeviceStatisticsRequest;
class DeviceStatisticsTracker;

// Responsible for scheduling the recording of multi-account device metrics
// (from DeviceStatisticsTracker) once per day.
class DeviceStatisticsScheduler : public signin::IdentityManager::Observer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

    virtual bool IsDeviceStatisticsMetricReportingEnabled() = 0;

    virtual std::unique_ptr<DeviceStatisticsRequest>
    CreateDeviceStatisticsRequest(const CoreAccountInfo&, const GURL&) = 0;

    virtual std::vector<std::string>
    GetCurrentDeviceCacheGuidsForDeviceStatistics() = 0;
  };

  // Starts the periodic recording of metrics once per day: If there was no
  // previous invocation today, kicks off a recording run immediately. If
  // metrics *were* recorded today already, and every time a recording run
  // finishes, schedules another run for the next day.
  // `delegate, `pref_service`, and `identity_manager` must not be null and must
  // outlive this object.
  DeviceStatisticsScheduler(Delegate* delegate,
                            PrefService* pref_service,
                            signin::IdentityManager* identity_manager,
                            const GURL& sync_server_url);
  ~DeviceStatisticsScheduler() override;

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // IdentityManager::Observer:
  void OnRefreshTokensLoaded() override;

 private:
  base::Time ComputeEarliestAllowedTimeToRun() const;

  void ScheduleNextRun();
  void Run();
  void RunDone();

  const base::Time creation_time_;
  const raw_ptr<Delegate> delegate_;
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const GURL sync_server_url_;

  // Only used while the refresh tokens aren't loaded yet.
  base::ScopedObservation<signin::IdentityManager, DeviceStatisticsScheduler>
      identity_manager_observation_{this};

  // Timer to schedule the next metrics recording run. Not running while a run
  // is ongoing (i.e. `tracker_` is non-null).
  base::WallClockTimer next_run_timer_;

  std::unique_ptr<DeviceStatisticsTracker> tracker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_
