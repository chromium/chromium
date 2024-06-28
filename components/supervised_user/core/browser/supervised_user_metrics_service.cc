// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
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
    supervised_user::SupervisedUserURLFilter* url_filter,
    std::unique_ptr<SupervisedUserMetricsServiceExtensionDelegate>
        extensions_metrics_delegate)
    : pref_service_(pref_service),
      url_filter_(url_filter),
      extensions_metrics_delegate_(std::move(extensions_metrics_delegate)) {
  DCHECK(pref_service_);
  DCHECK(url_filter_);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  CHECK(extensions_metrics_delegate_)
      << "Extensions metrics delegate must exist on Win/Linux/Mac";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  CheckForNewDay();
  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &SupervisedUserMetricsService::CheckForNewDay);
}

SupervisedUserMetricsService::~SupervisedUserMetricsService() = default;

void SupervisedUserMetricsService::Shutdown() {
  CheckForNewDay();
  timer_.Stop();
}

void SupervisedUserMetricsService::CheckForNewDay() {
  int day_id = pref_service_->GetInteger(prefs::kSupervisedUserMetricsDayId);
  base::Time now = base::Time::Now();
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  bool should_update_day_id = false;
  if (day_id < GetDayId(now)) {
    if (url_filter_->EmitURLFilterMetrics()) {
      should_update_day_id = true;
    }
    if (extensions_metrics_delegate_ &&
        extensions_metrics_delegate_->RecordExtensionsMetrics()) {
      should_update_day_id = true;
    }
    if (should_update_day_id) {
      pref_service_->SetInteger(prefs::kSupervisedUserMetricsDayId,
                                GetDayId(now));
    }
  }
}

}  // namespace supervised_user
