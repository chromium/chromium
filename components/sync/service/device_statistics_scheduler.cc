// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_scheduler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/time.h"
#include "components/sync/service/device_statistics_request.h"
#include "components/sync/service/device_statistics_tracker.h"

namespace syncer {

namespace {

constexpr char kLastAttemptedToRecordPref[] =
    "sync.device_statistics_timestamp";

}  // namespace

DeviceStatisticsScheduler::DeviceStatisticsScheduler(
    Delegate* delegate,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    const GURL& sync_server_url)
    : delegate_(delegate),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_server_url_(sync_server_url) {
  CHECK(delegate_);
  CHECK(pref_service_);
  CHECK(identity_manager_);

  StartTracker();
}

DeviceStatisticsScheduler::~DeviceStatisticsScheduler() = default;

// static
void DeviceStatisticsScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastAttemptedToRecordPref, base::Time());
}

void DeviceStatisticsScheduler::StartTracker() {
  if (!delegate_->IsDeviceStatisticsMetricReportingEnabled()) {
    return;
  }

  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // It shouldn't happen in practice that the account info (refresh tokens)
    // still aren't fully loaded at this point. But if it does, attempt starting
    // the tracker again in a little while.
    // TODO(crbug.com/465716865): Reconsider whether repeatedly re-trying makes
    // sense.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DeviceStatisticsScheduler::StartTracker,
                       weak_factory_.GetWeakPtr()),
        base::Seconds(5));
    return;
  }

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

  // `Unretained` is safe because `this` owns the `tracker_`, and `delegate_`
  // must outlive `this`.
  tracker_ = std::make_unique<DeviceStatisticsTracker>(
      identity_manager_, sync_server_url_,
      base::BindRepeating(&Delegate::CreateDeviceStatisticsRequest,
                          base::Unretained(delegate_)),
      delegate_->GetCurrentDeviceCacheGuidsForDeviceStatistics());

  // `Unretained` is safe because `this` owns the `tracker_`.
  tracker_->Start(base::BindOnce(&DeviceStatisticsScheduler::TrackerDone,
                                 base::Unretained(this)));
}

void DeviceStatisticsScheduler::TrackerDone() {
  tracker_.reset();
}

}  // namespace syncer
