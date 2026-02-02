// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/browser/device_parental_controls.h"
#include "components/supervised_user/core/browser/family_link_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user {

namespace {
// Reports WebFilterType which indicates web filter behaviour are used for
// current Family Link user.
constexpr char kFamilyUserWebFilterTypeHistogramName[] =
    "FamilyUser.WebFilterType";
// Reports WebFilterType which indicates web filter behaviour are used for
// current Supervised User.
constexpr char kSupervisedUserWebFilterTypeHistogramBaseName[] =
    "SupervisedUsers.WebFilterType";

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

std::string GetWebFilterTypeHistogramName(bool is_family_link,
                                          bool is_locally_supervised) {
  CHECK(is_family_link || is_locally_supervised)
      << "Callsite should assume at least one supervision type";
  // When the system is recovering from two supervision types available at the
  // same time to settle in "Family Link", it will record in the target
  // "FamilyLink" histogram.

  // LINT.IfChange(supervised_user_web_filter_type_user_type)
  return base::StrCat({kSupervisedUserWebFilterTypeHistogramBaseName,
                       is_family_link ? ".FamilyLink" : ".LocallySupervised"});
  // LINT.ThenChange(//tools/metrics/histograms/metadata/families/histograms.xml:supervised_user_web_filter_type_user_type)
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
    SupervisedUserUrlFilteringService& url_filtering_service,
    DeviceParentalControls& device_parental_controls,
    std::unique_ptr<SupervisedUserMetricsServiceExtensionDelegate>
        extensions_metrics_delegate,
    std::unique_ptr<SynteticFieldTrialDelegate> synthetic_field_trial_delegate)
    : pref_service_(pref_service),
      supervised_user_service_(supervised_user_service),
      url_filtering_service_(url_filtering_service),
      device_parental_controls_(device_parental_controls),
      extensions_metrics_delegate_(std::move(extensions_metrics_delegate)),
      synthetic_field_trial_delegate_(
          std::move(synthetic_field_trial_delegate)) {
  DCHECK(pref_service_);
  if (base::FeatureList::IsEnabled(kSupervisedUserUseUrlFilteringService)) {
    url_filtering_service_observation_.Observe(&url_filtering_service);
  } else {
    supervised_user_service_observation_.Observe(&supervised_user_service);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  CHECK(extensions_metrics_delegate_)
      << "Extensions metrics delegate must exist on Win/Linux/Mac";
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  CheckForNewDay();
  // Check for a new day every |kTimerInterval| as well.
  timer_.Start(FROM_HERE, kTimerInterval, this,
               &SupervisedUserMetricsService::CheckForNewDay);

#if BUILDFLAG(IS_ANDROID)
  // Platforms that support parental controls must also provide a delegate to
  // register synthetic field trials.
  CHECK(synthetic_field_trial_delegate_)
      << "Synthetic field trial delegate must exist on Android";
  device_parental_controls_subscription_ =
      device_parental_controls.Subscribe(base::BindRepeating(
          &SupervisedUserMetricsService::OnDeviceParentalControlsChanged,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_ANDROID)
}

SupervisedUserMetricsService::~SupervisedUserMetricsService() = default;

void SupervisedUserMetricsService::Shutdown() {
  // Per histograms.xml description, FamilyUser.WebFilterType must not emit on
  // signout. Shutdown is also called on signout, and the timer most probably
  // already emitted for the given day.
  timer_.Stop();
}

void SupervisedUserMetricsService::CheckForNewDay() {
  int day_id = pref_service_->GetInteger(prefs::kSupervisedUserMetricsDayId);
  int current_day_id = GetDayId(base::Time::Now());
  // The OnNewDay() event can fire sooner or later than 24 hours due to clock or
  // time zone changes.
  if (day_id < current_day_id) {
    ClearMetricsCache();
    TryEmittingMetricsAndRecordCurrentDay();

    if (extensions_metrics_delegate_ &&
        extensions_metrics_delegate_->RecordExtensionsMetrics()) {
      // Note that TryEmittingMetricsAndRecordCurrentDay above records the day
      // internally, but the delegate is external and new day must be recorded
      // explicitly.
      RecordCurrentDay();
    }
  }
}

void SupervisedUserMetricsService::RecordCurrentDay() {
  pref_service_->SetInteger(prefs::kSupervisedUserMetricsDayId,
                            GetDayId(base::Time::Now()));
}

void SupervisedUserMetricsService::OnDeviceParentalControlsChanged(
    const DeviceParentalControls& device_parental_controls) {
  device_parental_controls.RegisterDeviceLevelSyntheticFieldTrials(
      *synthetic_field_trial_delegate_);

  // This might be also called from OnURLFilterChanged() for the very same
  // change (eg. if browser filter has changed, triggering url filtering
  // changes) but that's not problematic (in metrics' context) since this
  // recording is idempotent (subsequent emits within the same day are
  // squashed).
  TryEmittingMetricsAndRecordCurrentDay();
}

void SupervisedUserMetricsService::OnURLFilterChanged() {
  OnUrlFilteringServiceChanged();
}

void SupervisedUserMetricsService::OnUrlFilteringServiceChanged() {
  // See comments in OnAndroidParentalControlsChanged about idempotency.
  TryEmittingMetricsAndRecordCurrentDay();
}

bool SupervisedUserMetricsService::TryEmittingMetricsAndRecordCurrentDay() {
  bool emitted = false;
  if (IsSubjectToParentalControls(*pref_service_.get()) &&
      TryEmittingFamilyLinkMetrics()) {
    emitted = true;
  }
  if ((device_parental_controls_->IsEnabled() ||
       IsSubjectToParentalControls(*pref_service_.get())) &&
      TryEmittingSupervisedUserMetrics()) {
    emitted = true;
  }

  if (emitted) {
    RecordCurrentDay();
  }

  return emitted;
}

bool SupervisedUserMetricsService::TryEmittingFamilyLinkMetrics() {
  bool emitted = false;
  WebFilterType web_filter_type = url_filtering_service_->GetWebFilterType();
  if (!last_recorded_family_link_web_filter_type_.has_value() ||
      *last_recorded_family_link_web_filter_type_ != web_filter_type) {
    base::UmaHistogramEnumeration(kFamilyUserWebFilterTypeHistogramName,
                                  web_filter_type);
    last_recorded_family_link_web_filter_type_ = web_filter_type;
    emitted = true;
  }

  if (!last_recorded_statistics_.has_value() ||
      *last_recorded_statistics_ !=
          supervised_user_service_->GetURLFilter()->GetFilteringStatistics()) {
    FamilyLinkUrlFilter::Statistics statistics =
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
    emitted = true;
  }

  return emitted;
}

bool SupervisedUserMetricsService::TryEmittingSupervisedUserMetrics() {
  WebFilterType current = url_filtering_service_->GetWebFilterType();
  if (last_recorded_supervised_user_web_filter_type_ == current) {
    return false;
  }

  base::UmaHistogramEnumeration(
      GetWebFilterTypeHistogramName(
          IsSubjectToParentalControls(*pref_service_.get()),
          device_parental_controls_->IsEnabled()),
      current);
  last_recorded_supervised_user_web_filter_type_ = current;
  return true;
}

void SupervisedUserMetricsService::ClearMetricsCache() {
  last_recorded_statistics_ = std::nullopt;
  last_recorded_family_link_web_filter_type_ = std::nullopt;
  last_recorded_supervised_user_web_filter_type_ = std::nullopt;
}
}  // namespace supervised_user
