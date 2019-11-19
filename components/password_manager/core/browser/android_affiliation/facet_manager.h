// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FACET_MANAGER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FACET_MANAGER_H_

#include <set>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"

namespace base {
class Clock;
class TaskRunner;
}  // namespace base

namespace password_manager {

class FacetManagerHost;

// Encapsulates the state and logic required for handling affiliation requests
// concerning a single facet. The AffiliationBackend owns one instance for each
// facet that requires attention, and it itself implements the FacetManagerHost
// interface to provide shared functionality needed by all FacetManagers.
class FacetManager {
 public:
  using StrategyOnCacheMiss = AffiliationService::StrategyOnCacheMiss;

  // Both the |backend| and |clock| must outlive this object.
  FacetManager(const FacetURI& facet_uri,
               FacetManagerHost* backend,
               base::Clock* clock);
  ~FacetManager();

  // Facet-specific implementations for methods in AffiliationService of the
  // same name. See documentation in affiliation_service.h for details:
  void GetAffiliationsAndBranding(
      StrategyOnCacheMiss cache_miss_strategy,
      AffiliationService::ResultCallback callback,
      const scoped_refptr<base::TaskRunner>& callback_task_runner);
  void Prefetch(const base::Time& keep_fresh_until);
  void CancelPrefetch(const base::Time& keep_fresh_until);

  // Called when |affiliation| information regarding this facet has just been
  // fetched from the Affiliation API.
  void OnFetchSucceeded(const AffiliatedFacetsWithUpdateTime& affiliation);

  // Called by the backend when the time specified in RequestNotificationAtTime
  // has come to pass, so that |this| can perform delayed administrative tasks.
  void NotifyAtRequestedTime();

  // Returns whether this instance has becomes redundant, that is, it has no
  // more meaningful state than a newly created instance would have.
  bool CanBeDiscarded() const;

  // Returns whether or not cached data for this facet can be discarded without
  // harm when trimming the database.
  bool CanCachedDataBeDiscarded() const;

  // Returns whether or not affiliation information relating to this facet needs
  // to be fetched right now.
  bool DoesRequireFetch() const;

  // The members below are made public for the sake of tests.

  // Returns whether or not cached data for this facet is fresh (not stale).
  bool IsCachedDataFresh() const;

  // Returns whether or not cached data for this facet is near-stale or stale.
  bool IsCachedDataNearStale() const;

  // The duration after which cached affiliation data is considered near-stale.
  static const int kCacheSoftExpiryInHours;

  // The duration after which cached affiliation data is considered stale.
  static const int kCacheHardExpiryInHours;

 private:
  struct RequestInfo;

  // Returns the time when the cached data for this facet will become stale.
  // The data is considered stale with the returned time value inclusive.
  base::Time GetCacheHardExpiryTime() const;

  // Returns the time when cached data for this facet becomes near-stale.
  // The data is considered near-stale with the returned time value inclusive.
  base::Time GetCacheSoftExpiryTime() const;

  // Returns the maximum of |keep_fresh_thresholds_|, or the NULL time if the
  // set is empty.
  base::Time GetMaximumKeepFreshUntilThreshold() const;

  // Returns the next time affiliation data for this facet needs to be fetched
  // due to active prefetch requests, or base::Time::Max() if not at all.
  base::Time GetNextRequiredFetchTimeDueToPrefetch() const;

  // Posts the callback of the request described by |request_info| with success.
  static void ServeRequestWithSuccess(RequestInfo request_info,
                                      const AffiliatedFacets& affiliation);

  // Posts the callback of the request described by |request_info| with failure.
  static void ServeRequestWithFailure(RequestInfo request_info);

  FacetURI facet_uri_;
  FacetManagerHost* backend_;
  base::Clock* clock_;

  // The last time affiliation information was fetched for this facet, i.e. the
  // freshness of the data in the cache. If there is no corresponding data in
  // the database, this will contain the NULL time. Otherwise, the update time
  // in the database should match this value; it is stored to reduce disk I/O.
  base::Time last_update_time_;

  // Contains information about the GetAffiliationsAndBranding() requests that
  // are waiting for the result of looking up this facet.
  std::vector<RequestInfo> pending_requests_;

  // Keeps track of |keep_fresh_until| thresholds corresponding to Prefetch()
  // requests for this facet. Affiliation information for this facet must be
  // kept fresh by periodic refetches until at least the maximum time in this
  // set (exclusive).
  //
  // This is not a single timestamp but rather a multiset so that cancellation
  // of individual prefetches can be supported even if there are two requests
  // with the same |keep_fresh_until| threshold.
  std::multiset<base::Time> keep_fresh_until_thresholds_;

  DISALLOW_COPY_AND_ASSIGN(FacetManager);
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_FACET_MANAGER_H_
