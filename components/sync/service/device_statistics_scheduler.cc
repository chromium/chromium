// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_scheduler.h"

#include <utility>

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/time.h"
#include "components/sync/service/device_statistics_request.h"
#include "components/sync/service/device_statistics_tracker.h"

namespace syncer {

namespace {

constexpr char kLastAttemptedToRecordPref[] =
    "sync.device_statistics_timestamp";

}  // namespace

DeviceStatisticsScheduler::DeviceStatisticsScheduler(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    const GURL& sync_server_url,
    RequestFactory request_factory,
    GetCurrentDeviceCacheGuidsCallback get_current_device_cache_guids)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_server_url_(sync_server_url),
      request_factory_(std::move(request_factory)),
      get_current_device_cache_guids_(
          std::move(get_current_device_cache_guids)) {
  CHECK(pref_service_);
  CHECK(identity_manager_);
  CHECK(request_factory_);
  CHECK(get_current_device_cache_guids_);

  StartTracker();
}

DeviceStatisticsScheduler::~DeviceStatisticsScheduler() = default;

// static
void DeviceStatisticsScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastAttemptedToRecordPref, base::Time());
}

void DeviceStatisticsScheduler::StartTracker() {
  // Only record metrics once per day.
  const base::Time last_recorded_at =
      pref_service_->GetTime(kLastAttemptedToRecordPref);
  const base::Time now = base::Time::Now();
  const bool can_issue_requests =
      last_recorded_at.is_null() ||
      last_recorded_at.LocalMidnight() < now.LocalMidnight();
  if (!can_issue_requests) {
    return;
  }

  pref_service_->SetTime(kLastAttemptedToRecordPref, now);

  tracker_ = std::make_unique<DeviceStatisticsTracker>(
      identity_manager_, sync_server_url_, request_factory_,
      get_current_device_cache_guids_.Run());

  tracker_->Start(base::BindOnce(&DeviceStatisticsScheduler::TrackerDone,
                                 base::Unretained(this)));
}

void DeviceStatisticsScheduler::TrackerDone() {
  tracker_.reset();
}

}  // namespace syncer
