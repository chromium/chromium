// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user {

namespace {

// UMA histogram FamilyUser.WebFilterType
// Reports WebFilterType which indicates web filter behaviour are used for
// current Family Link user.
constexpr char kWebFilterTypeHistogramName[] = "FamilyUser.WebFilterType";

// UMA histogram FamilyUser.ManualSiteListType
// Reports ManualSiteListType which indicates approved list and blocked list
// usage for current Family Link user.
constexpr char kManagedSiteListHistogramName[] = "FamilyUser.ManagedSiteList";

// UMA histogram FamilyUser.ManagedSiteListCount.Approved
// Reports the number of approved urls and domains for current Family Link user.
constexpr char kApprovedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Approved";

// UMA histogram FamilyUser.ManagedSiteListCount.Blocked
// Reports the number of blocked urls and domains for current Family Link user.
constexpr char kBlockedSitesCountHistogramName[] =
    "FamilyUser.ManagedSiteListCount.Blocked";

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
    SupervisedUserService& supervised_user_service,
    std::unique_ptr<SupervisedUserMetricsServiceExtensionDelegate>
        extensions_metrics_delegate)
    : pref_service_(pref_service),
      supervised_user_service_(supervised_user_service),
      extensions_metrics_delegate_(std::move(extensions_metrics_delegate)) {
  DCHECK(pref_service_);
  supervised_user_service_observation_.Observe(&supervised_user_service);
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
  int current_day_id = GetDayId(base::Time::Now());
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id < current_day_id) {
    bool should_update_day_id = false;
    // Since this service's periodical check runs independently from the
    // SupervisedUserService, do not emit if the filtering expected to be
    // inactive.
    if (supervised_user_service_->GetURLFilter()->GetWebFilterType() !=
        WebFilterType::kDisabled) {
      ClearMetricsCache();
      EmitMetrics();
      should_update_day_id = true;
    }

    if (extensions_metrics_delegate_ &&
        extensions_metrics_delegate_->RecordExtensionsMetrics()) {
      should_update_day_id = true;
    }
    if (should_update_day_id) {
      pref_service_->SetInteger(prefs::kSupervisedUserMetricsDayId,
                                current_day_id);
    }
  }
}

void SupervisedUserMetricsService::OnURLFilterChanged() {
  EmitMetrics();
}

void SupervisedUserMetricsService::EmitMetrics() {
  if (!last_recorded_web_filter_type_.has_value() ||
      *last_recorded_web_filter_type_ !=
          supervised_user_service_->GetURLFilter()->GetWebFilterType()) {
    WebFilterType web_filter_type =
        supervised_user_service_->GetURLFilter()->GetWebFilterType();

    base::UmaHistogramEnumeration(kWebFilterTypeHistogramName, web_filter_type);
    last_recorded_web_filter_type_ = web_filter_type;
  }

  if (!last_recorded_statistics_.has_value() ||
      *last_recorded_statistics_ !=
          supervised_user_service_->GetURLFilter()->GetFilteringStatistics()) {
    SupervisedUserURLFilter::Statistics statistics =
        supervised_user_service_->GetURLFilter()->GetFilteringStatistics();

    base::UmaHistogramCounts1000(
        kApprovedSitesCountHistogramName,
        statistics.allowed_hosts_count + statistics.allowed_urls_count);
    base::UmaHistogramCounts1000(
        kBlockedSitesCountHistogramName,
        statistics.blocked_hosts_count + statistics.blocked_urls_count);
    base::UmaHistogramEnumeration(kManagedSiteListHistogramName,
                                  statistics.GetManagedSiteList());
    last_recorded_statistics_ = statistics;
  }
}

void SupervisedUserMetricsService::ClearMetricsCache() {
  last_recorded_statistics_ = std::nullopt;
  last_recorded_web_filter_type_ = std::nullopt;
}

}  // namespace supervised_user
