// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/ads_intervention_manager.h"

#include <optional>

#include "base/metrics/histogram_macros.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "content/public/browser/navigation_handle.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

namespace subresource_filter {

namespace {

// Key into the website settings dict for last active ads violation.
const char kLastAdsViolationTimeKey[] = "LastAdsViolationTime";
const char kLastAdsViolationKey[] = "LastAdsViolation";

// Histograms
const char kAdsInterventionRecordedHistogramName[] =
    "SubresourceFilter.PageLoad.AdsInterventionTriggered";

AdsInterventionStatus GetAdsInterventionStatus(bool activation_status,
                                               bool intervention_active) {
  if (!intervention_active)
    return AdsInterventionStatus::kExpired;

  return activation_status ? AdsInterventionStatus::kBlocking
                           : AdsInterventionStatus::kWouldBlock;
}

}  // namespace

// static
base::TimeDelta AdsInterventionManager::GetInterventionDuration(
    mojom::AdsViolation violation) {
  switch (violation) {
    case mojom::AdsViolation::kHeavyAdsInterventionAtHostLimit:
      return base::Days(1);
    default:
      return kAdsInterventionDuration.Get();
  }
}

AdsInterventionManager::AdsInterventionManager(
    SubresourceFilterContentSettingsManager* settings_manager)
    : settings_manager_(settings_manager),
      clock_(base::DefaultClock::GetInstance()) {}

AdsInterventionManager::~AdsInterventionManager() = default;

void AdsInterventionManager::TriggerAdsInterventionForUrlOnSubsequentLoads(
    const GURL& url,
    mojom::AdsViolation ads_violation) {
  base::Value::Dict additional_metadata;

  double now = clock_->Now().InSecondsFSinceUnixEpoch();
  additional_metadata.Set(kLastAdsViolationTimeKey, now);
  additional_metadata.Set(kLastAdsViolationKey,
                          static_cast<int>(ads_violation));

  bool activated = base::FeatureList::IsEnabled(kAdsInterventionsEnforced);
  // This is a no-op if the metadata already exists for an active
  // ads intervention.
  settings_manager_->SetSiteMetadataBasedOnActivation(
      url, activated,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention,
      std::move(additional_metadata));

  UMA_HISTOGRAM_ENUMERATION(kAdsInterventionRecordedHistogramName,
                            ads_violation);
}

std::optional<AdsInterventionManager::LastAdsIntervention>
AdsInterventionManager::GetLastAdsIntervention(const GURL& url) const {
  // The last active ads intervention is stored in the site metadata.
  std::optional<base::Value::Dict> dict =
      settings_manager_->GetSiteMetadata(url);

  if (!dict)
    return std::nullopt;

  std::optional<int> ads_violation = dict->FindInt(kLastAdsViolationKey);
  std::optional<double> last_violation_time =
      dict->FindDouble(kLastAdsViolationTimeKey);

  if (ads_violation && last_violation_time) {
    base::TimeDelta diff =
        clock_->Now() -
        base::Time::FromSecondsSinceUnixEpoch(*last_violation_time);

    return LastAdsIntervention(
        {diff, static_cast<mojom::AdsViolation>(*ads_violation)});
  }

  return std::nullopt;
}

bool AdsInterventionManager::ShouldActivate(
    content::NavigationHandle* navigation_handle) const {
  const GURL& url(navigation_handle->GetURL());
  // TODO(crbug.com/40724530): Add new ads intervention
  // manager function to return struct with all ads intervention
  // metadata to reduce metadata accesses.
  std::optional<AdsInterventionManager::LastAdsIntervention> last_intervention =
      GetLastAdsIntervention(url);

  // Only activate the subresource filter if we are intervening on
  // ads.
  bool current_activation_status =
      settings_manager_->GetSiteActivationFromMetadata(url);
  bool has_active_ads_intervention = false;

  // TODO(crbug.com/40721691): If a host triggers multiple times on a single
  // navigate and the durations don't match, we'll use the last duration rather
  // than the longest. The metadata should probably store the activation with
  // the longest duration.
  if (last_intervention) {
    has_active_ads_intervention =
        last_intervention->duration_since <
        AdsInterventionManager::GetInterventionDuration(
            last_intervention->ads_violation);
    auto* ukm_recorder = ukm::UkmRecorder::Get();
    ukm::builders::AdsIntervention_LastIntervention builder(
        ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                               ukm::SourceIdType::NAVIGATION_ID));
    builder
        .SetInterventionType(static_cast<int>(last_intervention->ads_violation))
        .SetInterventionStatus(static_cast<int>(GetAdsInterventionStatus(
            current_activation_status, has_active_ads_intervention)));
    builder.Record(ukm_recorder->Get());
  }

  return current_activation_status && has_active_ads_intervention;
}

}  // namespace subresource_filter
