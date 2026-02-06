// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_
#define COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

namespace signin {
class IdentityManager;
}  // namespace signin

struct CoreAccountInfo;
class PrefRegistrySimple;
class PrefService;

namespace syncer {

class DeviceStatisticsRequest;
class DeviceStatisticsTracker;

// Responsible for scheduling the recording of multi-account device metrics
// (from DeviceStatisticsTracker) once per day.
class DeviceStatisticsScheduler {
 public:
  using RequestFactory = base::RepeatingCallback<std::unique_ptr<
      DeviceStatisticsRequest>(const CoreAccountInfo&, const GURL&)>;

  using GetCurrentDeviceCacheGuidsCallback =
      base::RepeatingCallback<std::vector<std::string>()>;

  // Starts the periodic recording of metrics once per day: If there was no
  // previous invocation today, kicks off a recording run immediately. If
  // metrics *were* recorded today already, and every time a recording run
  // finishes, schedules another run for the next day.
  // `pref_service` and `identity_manager` must not be null.
  // TODO(crbug.com/465716865): Replace the two callbacks with a delegate
  // interface.
  DeviceStatisticsScheduler(
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      const GURL& sync_server_url,
      RequestFactory request_factory,
      GetCurrentDeviceCacheGuidsCallback get_current_device_cache_guids);
  ~DeviceStatisticsScheduler();

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 private:
  void StartTracker();
  void TrackerDone();

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;

  const GURL sync_server_url_;

  const RequestFactory request_factory_;

  const GetCurrentDeviceCacheGuidsCallback get_current_device_cache_guids_;

  std::unique_ptr<DeviceStatisticsTracker> tracker_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DEVICE_STATISTICS_SCHEDULER_H_
