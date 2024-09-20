// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/organization/trigger_policies.h"

#include <cmath>
#include <numbers>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/metrics/desktop_session_duration/desktop_session_duration_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/organization/prefs.h"
#include "components/prefs/pref_service.h"

UsageTickClock::UsageTickClock(const base::TickClock* base_clock)
    : base_clock_(base_clock), start_time_(base_clock_->NowTicks()) {
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    auto* const tracker = metrics::DesktopSessionDurationTracker::Get();
    tracker->AddObserver(this);
    if (tracker->in_session()) {
      current_usage_session_start_time_ = start_time_;
    }
  }
}

UsageTickClock::~UsageTickClock() {
  if (metrics::DesktopSessionDurationTracker::IsInitialized()) {
    metrics::DesktopSessionDurationTracker::Get()->RemoveObserver(this);
  }
}

base::TimeTicks UsageTickClock::NowTicks() const {
  const base::TimeTicks completed_session_time =
      start_time_ + usage_time_in_completed_sessions_;
  if (current_usage_session_start_time_.has_value()) {
    return completed_session_time + (base_clock_->NowTicks() -
                                     current_usage_session_start_time_.value());
  }
  return completed_session_time;
}

void UsageTickClock::OnSessionStarted(base::TimeTicks session_start) {
  DCHECK(!current_usage_session_start_time_.has_value());

  // Ignore `session_start`; it doesn't come from `base_clock_`.
  current_usage_session_start_time_ = base_clock_->NowTicks();
}

void UsageTickClock::OnSessionEnded(base::TimeDelta session_length,
                                    base::TimeTicks session_end) {
  DCHECK(current_usage_session_start_time_.has_value());

  // Ignore `session_length`/`session_end`; they don't come from `base_clock_`.
  usage_time_in_completed_sessions_ +=
      base_clock_->NowTicks() - current_usage_session_start_time_.value();
  current_usage_session_start_time_ = std::nullopt;
}

ProfilePrefBackoffLevelProvider::ProfilePrefBackoffLevelProvider(
    content::BrowserContext* context)
    : prefs_(Profile::FromBrowserContext(context)->GetPrefs()) {}

ProfilePrefBackoffLevelProvider::~ProfilePrefBackoffLevelProvider() = default;

unsigned int ProfilePrefBackoffLevelProvider::Get() const {
  return prefs_->GetInteger(
      tab_organization_prefs::kTabOrganizationNudgeBackoffCount);
}

void ProfilePrefBackoffLevelProvider::Increment() {
  prefs_->SetInteger(tab_organization_prefs::kTabOrganizationNudgeBackoffCount,
                     Get() + 1);
}

void ProfilePrefBackoffLevelProvider::Decrement() {
  prefs_->SetInteger(tab_organization_prefs::kTabOrganizationNudgeBackoffCount,
                     std::max(1u, Get()) - 1);
}

TargetFrequencyTriggerPolicy::TargetFrequencyTriggerPolicy(
    std::unique_ptr<base::TickClock> clock,
    base::TimeDelta base_period,
    float backoff_base,
    BackoffLevelProvider* backoff_level_provider)
    : clock_(std::move(clock)),
      base_period_(base_period),
      backoff_base_(backoff_base),
      backoff_level_provider_(backoff_level_provider),
      cycle_start_time_(clock_->NowTicks()) {}

TargetFrequencyTriggerPolicy::~TargetFrequencyTriggerPolicy() = default;

bool TargetFrequencyTriggerPolicy::ShouldTrigger(float score) {
  const base::TimeTicks current_time = clock_->NowTicks();
  const base::TimeDelta period =
      base_period_ * std::pow(backoff_base_, backoff_level_provider_->Get());

  // Restart the cycle if `period_` has elapsed.
  if (current_time > cycle_start_time_ + period) {
    cycle_start_time_ += period;
    best_score = std::nullopt;
    base::UmaHistogramBoolean("Tab.Organization.Trigger.TriggeredInPeriod",
                              has_triggered_);
    has_triggered_ = false;
  }

  // Update the best score if we're in the observation phase.
  const base::TimeDelta observation_period = period / std::numbers::e_v<float>;
  if (current_time < cycle_start_time_ + observation_period) {
    best_score =
        best_score.has_value() ? std::max(best_score.value(), score) : score;
    return false;
  }

  // Trigger if we haven't triggered yet and have a new high score.
  if (!has_triggered_ && best_score.has_value() && score > best_score) {
    best_score = std::nullopt;
    has_triggered_ = true;
    return true;
  }

  return false;
}

void TargetFrequencyTriggerPolicy::OnTriggerSucceeded() {
  backoff_level_provider_->Decrement();
}

void TargetFrequencyTriggerPolicy::OnTriggerFailed() {
  backoff_level_provider_->Increment();
}

bool NeverTriggerPolicy::ShouldTrigger(float score) {
  return false;
}

bool DemoTriggerPolicy::ShouldTrigger(float score) {
  return true;
}
