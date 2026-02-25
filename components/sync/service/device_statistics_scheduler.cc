// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/device_statistics_scheduler.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
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
    : creation_time_(base::Time::Now()),
      delegate_(delegate),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_server_url_(sync_server_url) {
  CHECK(delegate_);
  CHECK(pref_service_);
  CHECK(identity_manager_);

  if (base::FeatureList::IsEnabled(kSyncRecordDeviceStatisticsMetrics)) {
    if (identity_manager_->AreRefreshTokensLoaded()) {
      ScheduleNextRun();
    } else {
      identity_manager_observation_.Observe(identity_manager_);
    }
  }
}

DeviceStatisticsScheduler::~DeviceStatisticsScheduler() = default;

// static
void DeviceStatisticsScheduler::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kLastAttemptedToRecordPref, base::Time());
}

void DeviceStatisticsScheduler::OnRefreshTokensLoaded() {
  identity_manager_observation_.Reset();

  ScheduleNextRun();
}

base::Time DeviceStatisticsScheduler::ComputeEarliestAllowedTimeToRun() const {
  const base::Time now = base::Time::Now();

  // Ensure the "last recorded" timestamp is not in the future.
  const base::Time last_recorded_at =
      std::min(pref_service_->GetTime(kLastAttemptedToRecordPref), now);

  // The metrics should be recorded once per N calendar days (determined by a
  // feature param), so the next possible time is midnight, N days after the
  // last recording.
  base::Time earliest_allowed =
      last_recorded_at.is_null()
          ? now
          : (last_recorded_at +
             base::Days(kSyncRecordDeviceStatisticsMetricsPeriodDays.Get()))
                .LocalMidnight();

  if (earliest_allowed > now) {
    // Recording has already happened today. Wait (somewhat arbitrarily) until
    // noon on the following day to record again. This avoids recording twice in
    // immediate succession if the previous recording happened just before
    // midnight.
    earliest_allowed += base::Hours(12);
  }

  // If metrics reporting is disabled, try again one day from now at the
  // earliest.
  if (!delegate_->IsDeviceStatisticsMetricReportingEnabled()) {
    earliest_allowed = std::max(earliest_allowed, now + base::Days(1));
  }

  // At browser startup, wait some time before recording for the first time.
  earliest_allowed =
      std::max(earliest_allowed,
               creation_time_ + kSyncRecordDeviceStatisticsMetricsDelay.Get());

  return earliest_allowed;
}

void DeviceStatisticsScheduler::ScheduleNextRun() {
  CHECK(base::FeatureList::IsEnabled(kSyncRecordDeviceStatisticsMetrics));
  CHECK(!next_run_timer_.IsRunning());
  CHECK(!tracker_);
  CHECK(identity_manager_->AreRefreshTokensLoaded());

  // Note: `ComputeEarliestAllowedTimeToRun()` may be in the past, in which case
  // `Run` will get posted immediately.
  next_run_timer_.Start(
      FROM_HERE, ComputeEarliestAllowedTimeToRun(),
      base::BindOnce(&DeviceStatisticsScheduler::Run, base::Unretained(this)));
}

void DeviceStatisticsScheduler::Run() {
  CHECK(!tracker_);

  const base::Time earliest_allowed = ComputeEarliestAllowedTimeToRun();
  const base::Time now = base::Time::Now();

  if (earliest_allowed > now) {
    // This shouldn't usually happen, since runs get scheduled for the time when
    // they'll be allowed. It could happen e.g. if the metrics opt-in changed,
    // or if there's something wrong with the device clock.
    ScheduleNextRun();
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
  tracker_->Start(base::BindOnce(&DeviceStatisticsScheduler::RunDone,
                                 base::Unretained(this)));
}

void DeviceStatisticsScheduler::RunDone() {
  tracker_.reset();

  ScheduleNextRun();
}

}  // namespace syncer
