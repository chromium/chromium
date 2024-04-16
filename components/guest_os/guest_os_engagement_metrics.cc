// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_os/guest_os_engagement_metrics.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/system/sys_info.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/trace_event.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "components/exo/wm_helper.h"
#include "components/guest_os/guest_os_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"

namespace guest_os {

namespace {

constexpr base::TimeDelta kUpdateEngagementTimePeriod = base::Minutes(1);
constexpr base::TimeDelta kSaveEngagementTimeToPrefsPeriod = base::Minutes(30);

int GetDayId(const base::Clock* clock) {
  return clock->Now().LocalMidnight().since_origin().InDays();
}

}  // namespace

GuestOsEngagementMetrics::GuestOsEngagementMetrics(
    PrefService* pref_service,
    WindowMatcher window_matcher,
    const std::string& pref_prefix,
    const std::string& uma_name)
    : GuestOsEngagementMetrics(pref_service,
                               window_matcher,
                               pref_prefix,
                               uma_name,
                               base::DefaultClock::GetInstance(),
                               base::DefaultTickClock::GetInstance()) {}

// Private, for testing use only
GuestOsEngagementMetrics::GuestOsEngagementMetrics(
    PrefService* pref_service,
    WindowMatcher window_matcher,
    const std::string& pref_prefix,
    const std::string& uma_name,
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : pref_service_(pref_service),
      window_matcher_(window_matcher),
      pref_prefix_(pref_prefix),
      uma_name_(uma_name),
      clock_(clock),
      tick_clock_(tick_clock),
      last_update_ticks_(tick_clock_->NowTicks()) {
  // WMHelper, SessionManager and PowerManagerClient may not exist in tests.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->AddActivationObserver(this);

  if (session_manager::SessionManager::Get()) {
    session_active_ =
        (session_manager::SessionManager::Get()->session_state() ==
         session_manager::SessionState::ACTIVE);
    session_manager::SessionManager::Get()->AddObserver(this);
  }

  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->AddObserver(this);

  DCHECK(pref_service_);
  RestoreEngagementTimeFromPrefs();
  update_engagement_time_timer_.Start(
      FROM_HERE, kUpdateEngagementTimePeriod, this,
      &GuestOsEngagementMetrics::UpdateEngagementTime);
  save_engagement_time_to_prefs_timer_.Start(
      FROM_HERE, kSaveEngagementTimeToPrefsPeriod, this,
      &GuestOsEngagementMetrics::SaveEngagementTimeToPrefs);
}

GuestOsEngagementMetrics::~GuestOsEngagementMetrics() {
  save_engagement_time_to_prefs_timer_.Stop();
  update_engagement_time_timer_.Stop();
  UpdateEngagementTime();
  SaveEngagementTimeToPrefs();

  // Observers for WMHelper, SessionManager and PowerManagerClient may not
  // have been added in tests.
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
  if (session_manager::SessionManager::Get())
    session_manager::SessionManager::Get()->RemoveObserver(this);

  // If WMHelper is already destroyed, do nothing.
  // TODO(crbug.com/40531599): Fix shutdown order.
  if (exo::WMHelper::HasInstance())
    exo::WMHelper::GetInstance()->RemoveActivationObserver(this);
}

void GuestOsEngagementMetrics::SetBackgroundActive(bool background_active) {
  if (background_active_ == background_active)
    return;
  UpdateEngagementTime();
  background_active_ = background_active;
}

void GuestOsEngagementMetrics::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  UpdateEngagementTime();
  matched_window_active_ = gained_active && window_matcher_.Run(gained_active);
}

void GuestOsEngagementMetrics::OnSessionStateChanged() {
  TRACE_EVENT0("login", "GuestOsEngagementMetrics::OnSessionStateChanged");
  UpdateEngagementTime();
  session_active_ = session_manager::SessionManager::Get()->session_state() ==
                    session_manager::SessionState::ACTIVE;
}

void GuestOsEngagementMetrics::ScreenIdleStateChanged(
    const power_manager::ScreenIdleState& proto) {
  UpdateEngagementTime();
  screen_dimmed_ = proto.dimmed();
}

void GuestOsEngagementMetrics::RestoreEngagementTimeFromPrefs() {
  // Restore accumulated results only if they were recorded on the same OS
  // version.
  if (pref_service_->GetString(pref_prefix_ +
                               prefs::kEngagementTimeOsVersion) ==
      base::SysInfo::OperatingSystemVersion()) {
    day_id_ =
        pref_service_->GetInteger(pref_prefix_ + prefs::kEngagementTimeDayId);
    engagement_time_total_ =
        pref_service_->GetTimeDelta(pref_prefix_ + prefs::kEngagementTimeTotal);
    engagement_time_foreground_ = pref_service_->GetTimeDelta(
        pref_prefix_ + prefs::kEngagementTimeForeground);
    engagement_time_background_ = pref_service_->GetTimeDelta(
        pref_prefix_ + prefs::kEngagementTimeBackground);
  } else {
    ResetEngagementTimePrefs();
  }

  RecordEngagementTimeToUmaIfNeeded();
}

void GuestOsEngagementMetrics::SaveEngagementTimeToPrefs() {
  DCHECK(pref_service_);

  pref_service_->SetString(pref_prefix_ + prefs::kEngagementTimeOsVersion,
                           base::SysInfo::OperatingSystemVersion());
  pref_service_->SetInteger(pref_prefix_ + prefs::kEngagementTimeDayId,
                            day_id_);
  pref_service_->SetTimeDelta(pref_prefix_ + prefs::kEngagementTimeTotal,
                              engagement_time_total_);
  pref_service_->SetTimeDelta(pref_prefix_ + prefs::kEngagementTimeForeground,
                              engagement_time_foreground_);
  pref_service_->SetTimeDelta(pref_prefix_ + prefs::kEngagementTimeBackground,
                              engagement_time_background_);
}

void GuestOsEngagementMetrics::UpdateEngagementTime() {
  VLOG(2) << "last state: screen_dimmed=" << screen_dimmed_
          << " session_active=" << session_active_
          << " background_active=" << background_active_
          << " matched_window_active=" << matched_window_active_;

  base::TimeTicks now = tick_clock_->NowTicks();
  base::TimeDelta elapsed = now - last_update_ticks_;

  if (ShouldAccumulateEngagementTotalTime()) {
    VLOG(2) << "accumulate to total time " << elapsed;
    engagement_time_total_ += elapsed;
    if (ShouldAccumulateEngagementForegroundTime()) {
      VLOG(2) << "accumulate to foreground time " << elapsed;
      engagement_time_foreground_ += elapsed;
    } else if (ShouldAccumulateEngagementBackgroundTime()) {
      VLOG(2) << "accumulate to background time " << elapsed;
      engagement_time_background_ += elapsed;
    }
  }

  last_update_ticks_ = now;
  RecordEngagementTimeToUmaIfNeeded();
}

void GuestOsEngagementMetrics::RecordEngagementTimeToUmaIfNeeded() {
  if (!ShouldRecordEngagementTimeToUma())
    return;
  VLOG(2) << "day changed, recording engagement time to UMA";
  UmaHistogramCustomTimes(uma_name_ + ".EngagementTime.Total",
                          engagement_time_total_, base::Seconds(1),
                          base::Days(1) + kUpdateEngagementTimePeriod, 50);
  UmaHistogramCustomTimes(
      uma_name_ + ".EngagementTime." + uma_name_ + "Total",
      engagement_time_foreground_ + engagement_time_background_,
      base::Seconds(1), base::Days(1) + kUpdateEngagementTimePeriod, 50);
  UmaHistogramCustomTimes(uma_name_ + ".EngagementTime.Foreground",
                          engagement_time_foreground_, base::Seconds(1),
                          base::Days(1) + kUpdateEngagementTimePeriod, 50);
  UmaHistogramCustomTimes(uma_name_ + ".EngagementTime.Background",
                          engagement_time_background_, base::Seconds(1),
                          base::Days(1) + kUpdateEngagementTimePeriod, 50);
  ResetEngagementTimePrefs();
}

void GuestOsEngagementMetrics::ResetEngagementTimePrefs() {
  day_id_ = GetDayId(clock_);
  engagement_time_total_ = base::TimeDelta();
  engagement_time_foreground_ = base::TimeDelta();
  engagement_time_background_ = base::TimeDelta();
  SaveEngagementTimeToPrefs();
}

bool GuestOsEngagementMetrics::ShouldAccumulateEngagementTotalTime() const {
  return session_active_ && !screen_dimmed_;
}

bool GuestOsEngagementMetrics::ShouldAccumulateEngagementForegroundTime()
    const {
  return matched_window_active_;
}

bool GuestOsEngagementMetrics::ShouldAccumulateEngagementBackgroundTime()
    const {
  return background_active_;
}

bool GuestOsEngagementMetrics::ShouldRecordEngagementTimeToUma() const {
  return day_id_ != GetDayId(clock_);
}

std::unique_ptr<GuestOsEngagementMetrics>
GuestOsEngagementMetrics::GetEngagementMetricsForTesting(
    PrefService* pref_service,
    WindowMatcher window_matcher,
    const std::string& pref_prefix,
    const std::string& uma_name,
    const base::Clock* clock,
    const base::TickClock* tick_clock) {
  GuestOsEngagementMetrics* metrics = new GuestOsEngagementMetrics(
      pref_service, window_matcher, pref_prefix, uma_name, clock, tick_clock);
  // WrapUnique is needed because the constructor is private.
  return base::WrapUnique(metrics);
}

}  // namespace guest_os
