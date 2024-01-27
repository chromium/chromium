// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_INTERVENTION_MANAGER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_INTERVENTION_MANAGER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

class GURL;

namespace base {
class Clock;
}

namespace content {
class NavigationHandle;
}

// The subresource filter activation status associated with an ads
// intervention during page load.
enum class AdsInterventionStatus {
  // The ads intervention occurred longer than
  // subresource_filter::kAdsInterventionDuration ago.
  kExpired,
  // Ads interventions are in dry run mode and the subresource filter would
  // have activated due to an active intervention.
  kWouldBlock,
  // Ads interventions are activate and there is an active intervention.
  kBlocking,
};

namespace subresource_filter {

// This class tracks ads interventions that have occurred on origins and is
// bound to the user's profile. The ads intervention manager operates in two
// modes set by the feature flag kAdsInterventionsEnforced:
// 1. Dry run: Ads are not blocked on sites with ad interventions, however,
//    the ads intervention manager records metrics as if ads were blocked.
//    If the ads intervention manager is asked to intervene on the same URL
//    in the period where we would block ads during enforcement, it will only
//    record the first seen intervention.
// 2. Enforced: Ads are blocked on sites with ad interventions.
//
// The duration of an ad intervention is set by the feature flag
// kAdsInterventionDuration.
//
// This class maintain's metadata for ads interventions in the user's website
// settings. This is persisted to disk and cleared with browsing history. The
// content subresource filter manager expires ads intervention metadata after
// 7 days. As a result, kAdsInterventionDuration should be less than 7 days
// to prevent expiry from impacting metrics. The metadata is scoped to each
// url's origin. This API would ideally work with Origins insead of GURLs,
// however, downstream APIs use GURL's.
class AdsInterventionManager {
 public:
  // Struct representing the last triggered ads intervention.
  struct LastAdsIntervention {
    base::TimeDelta duration_since;
    mojom::AdsViolation ads_violation;
  };

  // Gets the duration for the |violation| given, as different violations may
  // have different durations associated with them.
  static base::TimeDelta GetInterventionDuration(mojom::AdsViolation violation);

  // The content_settings_manager should outlive the ads intervention manager.
  // This is satisfied as the SubresourceFilterContentSettingsManager and the
  // AdsInterventionManager are both bound to the profile.
  explicit AdsInterventionManager(
      SubresourceFilterContentSettingsManager* content_settings_manager);
  ~AdsInterventionManager();
  AdsInterventionManager(const AdsInterventionManager&) = delete;
  AdsInterventionManager& operator=(const AdsInterventionManager&) = delete;

  // The ads intervention manager should trigger an ads intervention on each
  // subsequent page load to |url| for kAdsInterventionDuration. The active
  // intervention is recorded in the user's website settings and updates
  // |url| site metadata with the last active intervention.
  void TriggerAdsInterventionForUrlOnSubsequentLoads(
      const GURL& url,
      mojom::AdsViolation ads_violation);

  // Returns the last active ads intervention written to metadata,
  // otherwise std::nullopt is returned. When retrieving ads interventions
  // for a navigation, should_record_metrics should be true to record
  // per-navigation ads intervention metrics.
  std::optional<LastAdsIntervention> GetLastAdsIntervention(
      const GURL& url) const;

  // Returns whether the subresource filter should activate for
  // |navigation_handle| based on feature status and current active
  // intervention. This should only be called once per page to calculate
  // activation as it records per page intervention information.
  bool ShouldActivate(content::NavigationHandle* navigation_handle) const;

  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

 private:
  // The SubresourceFilterContentSettingsManager is guaranteed to outlive the
  // AdsInterventionManager. Both are bound to the profile.
  raw_ptr<SubresourceFilterContentSettingsManager> settings_manager_ = nullptr;

  raw_ptr<base::Clock, DanglingUntriaged> clock_;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_ADS_INTERVENTION_MANAGER_H_
