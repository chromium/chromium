// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/parental_control_metrics.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user {

namespace {

constexpr base::TimeDelta kTimerInterval = base::Minutes(10);

// Returns the number of days since the origin.
int GetDayId(base::Time time) {
  return time.LocalMidnight().since_origin().InDaysFloored();
}

}  // namespace

// static
void SupervisedUserMetricsService::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kSupervisedUserMetricsDayId, 0);
}

// static
int SupervisedUserMetricsService::GetDayIdForTesting(base::Time time) {
  return GetDayId(time);
}

SupervisedUserMetricsService::SupervisedUserMetricsService(
    PrefService* pref_service,
    supervised_user::SupervisedUserURLFilter* url_filter)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(url_filter);

  supervised_user_metrics_.push_back(
      std::make_unique<ParentalControlMetrics>(pref_service, url_filter));

  for (auto& supervised_user_metric : supervised_user_metrics_) {
    AddObserver(supervised_user_metric.get());
  }

  CheckForNewDay();
  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &SupervisedUserMetricsService::CheckForNewDay);
}

SupervisedUserMetricsService::~SupervisedUserMetricsService() = default;

void SupervisedUserMetricsService::Shutdown() {
  CheckForNewDay();
  observers_.Clear();
  supervised_user_metrics_.clear();
  timer_.Stop();
}

void SupervisedUserMetricsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SupervisedUserMetricsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SupervisedUserMetricsService::CheckForNewDay() {
  int day_id = pref_service_->GetInteger(prefs::kSupervisedUserMetricsDayId);
  base::Time now = base::Time::Now();
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id < GetDayId(now)) {
    for (Observer& observer : observers_) {
      observer.OnNewDay();
    }
    pref_service_->SetInteger(prefs::kSupervisedUserMetricsDayId,
                              GetDayId(now));
  }
}

}  // namespace supervised_user
