// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: Read the class comment of AffiliationService for the definition
// of the terms used below.
//
// On-demand fetching strategy
//
// A GetAffiliationsAndBranding() request concerning facet X will be served from
// the cache as long as the cache contains fresh affiliation information for
// facet X, that is, if there is an equivalence class in the cache that contains
// X and has been fetched less than |kCacheHardExpiryInHours| hours ago.
//
// Otherwise, a network request is issued against the Affiliation API as soon as
// possible, that is, immediately if there is no fetch in flight, or right after
// completion of the fetch in flight if there is one, provided that the required
// data is not incidentally returned by the first fetch.
//
//
// Proactive fetching strategy
//
// A Prefetch() request concerning facet Y can trigger an initial network fetch,
// or periodic refetches only when:
//   * The prefetch request is not already expired, i.e., its |keep_fresh_until|
//     threshold is strictly in the future (that is, prefetch intervals are open
//     from the right).
//   * Affiliation information in the cache pertaining to facet Y will get stale
//     strictly before the specified |keep_fresh_until| threshold.
//
// An initial fetch will be issued as soon as possible if, in addition to the
// two necessery conditions above, and at the time of the Prefetch() call, the
// cache contains no affiliation information regarding facet Y, or if the data
// in the cache for facet Y is near-stale, that is, it has been fetched more
// than |kCacheHardExpiryInHours| hours ago.
//
// A refetch will be issued every time the data in the cache regarding facet Y
// becomes near-stale, that is, exactly |kCacheSoftExpiry| hours after the last
// fetch, provided that the above two necessary conditions are also met.
//
// Fetches are triggered already when the data gets near-stale, as opposed to
// waiting until the data would get stale, in an effort to keep the data fresh
// even in face of temporary network errors lasting no more than the difference
// between soft and hard expiry times.
//
// The current fetch scheduling logic, however, can only deal with at most one
// such 'early' fetch between taking place between the prior fetch and the
// corresponding hard expiry time of the data, therefore it is assumed that:
//
//   kCacheSoftExpiryInHours < kCacheHardExpiryInHours, and
//   2 * kCacheSoftExpiryInHours > kCacheHardExpiryInHours.
//
//
// Cache freshness terminology
//
//
//      Fetch (t=0)              kCacheSoftExpiry        kCacheHardExpiry
//      /                        /                       /
//  ---o------------------------o-----------------------o-----------------> t
//     |                        |                       |
//     |                        [-- Cache near-stale --------------------- ..
//     [--------------- Cache is fresh ----------------)[-- Cache is stale ..
//

#include "components/affiliations/core/browser/facet_manager.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/facet_manager_host.h"

namespace affiliations {

// statics
const int FacetManager::kCacheSoftExpiryInHours = 21;
const int FacetManager::kCacheHardExpiryInHours = 24;

static_assert(
    FacetManager::kCacheSoftExpiryInHours <
        FacetManager::kCacheHardExpiryInHours,
    "Soft expiry period must be shorter than the hard expiry period.");

static_assert(
    2 * FacetManager::kCacheSoftExpiryInHours >
        FacetManager::kCacheHardExpiryInHours,
    "Soft expiry period must be longer than half of the hard expiry period.");

// Encapsulates the details of a pending GetAffiliationsAndBranding() request.
struct FacetManager::RequestInfo {
  AffiliationService::ResultCallback callback;
  scoped_refptr<base::TaskRunner> callback_task_runner;
};

FacetManager::FacetManager(const FacetURI& facet_uri,
                           FacetManagerHost* backend,
                           base::Clock* clock)
    : facet_uri_(facet_uri), backend_(backend), clock_(clock) {
  AffiliatedFacetsWithUpdateTime affiliations;
  if (backend_->ReadAffiliationsAndBrandingFromDatabase(facet_uri_,
                                                        &affiliations))
    last_update_time_ = affiliations.last_update_time;
}

FacetManager::~FacetManager() {
  // The manager will be destroyed while there are pending requests only if the
  // entire backend is going away. Fail pending requests in this case.
  for (auto& request_info : pending_requests_)
    ServeRequestWithFailure(std::move(request_info));
}

void FacetManager::GetAffiliationsAndBranding(
    StrategyOnCacheMiss cache_miss_strategy,
    AffiliationService::ResultCallback callback,
    const scoped_refptr<base::TaskRunner>& callback_task_runner) {
  RequestInfo request_info;
  request_info.callback = std::move(callback);
  request_info.callback_task_runner = callback_task_runner;
  if (IsCachedDataFresh()) {
    AffiliatedFacetsWithUpdateTime affiliation;
    if (!backend_->ReadAffiliationsAndBrandingFromDatabase(facet_uri_,
                                                           &affiliation)) {
      ServeRequestWithFailure(std::move(request_info));
      return;
    }
    DCHECK_EQ(affiliation.last_update_time, last_update_time_) << facet_uri_;
    ServeRequestWithSuccess(std::move(request_info), affiliation.facets);
  } else if (cache_miss_strategy == StrategyOnCacheMiss::FETCH_OVER_NETWORK) {
    pending_requests_.push_back(std::move(request_info));
    backend_->SignalNeedNetworkRequest();
  } else if (cache_miss_strategy ==
             StrategyOnCacheMiss::TRY_ONCE_OVER_NETWORK) {
    pending_one_time_requests_.push_back(std::move(request_info));
    backend_->SignalNeedNetworkRequest();
  } else {
    ServeRequestWithFailure(std::move(request_info));
  }
}

void FacetManager::Prefetch(const base::Time& keep_fresh_until) {
  keep_fresh_until_thresholds_.insert(keep_fresh_until);

  // If an initial fetch if needed, trigger that (the refetch will be scheduled
  // once the initial fetch completes). Otherwise schedule the next refetch.
  base::Time next_required_fetch(GetNextRequiredFetchTimeDueToPrefetch());
  if (next_required_fetch <= clock_->Now()) {
    backend_->SignalNeedNetworkRequest();
  } else if (next_required_fetch < base::Time::Max()) {
    backend_->RequestNotificationAtTime(facet_uri_, next_required_fetch);
  }

  // For a finite |keep_fresh_until|, schedule a callback so that once the
  // prefetch expires, it can be removed from |keep_fresh_untils_|, and also the
  // manager can get a chance to be destroyed unless it is otherwise needed.
  if (keep_fresh_until > clock_->Now() && keep_fresh_until < base::Time::Max())
    backend_->RequestNotificationAtTime(facet_uri_, keep_fresh_until);
}

void FacetManager::CancelPrefetch(const base::Time& keep_fresh_until) {
  auto iter = keep_fresh_until_thresholds_.find(keep_fresh_until);
  if (iter != keep_fresh_until_thresholds_.end())
    keep_fresh_until_thresholds_.erase(iter);
}

void FacetManager::OnFetchSucceeded(
    const AffiliatedFacetsWithUpdateTime& affiliation) {
  last_update_time_ = affiliation.last_update_time;
  DCHECK(IsCachedDataFresh()) << facet_uri_;
  for (auto& request_info : pending_requests_)
    ServeRequestWithSuccess(std::move(request_info), affiliation.facets);
  pending_requests_.clear();
  for (auto& request_info : pending_one_time_requests_) {
    ServeRequestWithSuccess(std::move(request_info), affiliation.facets);
  }
  pending_one_time_requests_.clear();

  base::Time next_required_fetch(GetNextRequiredFetchTimeDueToPrefetch());
  if (next_required_fetch < base::Time::Max())
    backend_->RequestNotificationAtTime(facet_uri_, next_required_fetch);
}

void FacetManager::OnFetchFailed() {
  for (auto& request_info : pending_one_time_requests_) {
    ServeRequestWithFailure(std::move(request_info));
  }
  pending_one_time_requests_.clear();
}

void FacetManager::NotifyAtRequestedTime() {
  base::Time next_required_fetch(GetNextRequiredFetchTimeDueToPrefetch());
  if (next_required_fetch <= clock_->Now()) {
    backend_->SignalNeedNetworkRequest();
  } else if (next_required_fetch < base::Time::Max()) {
    backend_->RequestNotificationAtTime(facet_uri_, next_required_fetch);
  }

  auto iter_first_non_expired =
      keep_fresh_until_thresholds_.upper_bound(clock_->Now());
  keep_fresh_until_thresholds_.erase(keep_fresh_until_thresholds_.begin(),
                                     iter_first_non_expired);
}

bool FacetManager::CanBeDiscarded() const {
  return pending_requests_.empty() && pending_one_time_requests_.empty() &&
         GetMaximumKeepFreshUntilThreshold() <= clock_->Now();
}

bool FacetManager::CanCachedDataBeDiscarded() const {
  return GetMaximumKeepFreshUntilThreshold() <= clock_->Now() ||
         !IsCachedDataFresh();
}

bool FacetManager::DoesRequireFetch() const {
  return ((!pending_requests_.empty() || !pending_one_time_requests_.empty()) &&
          !IsCachedDataFresh()) ||
         GetNextRequiredFetchTimeDueToPrefetch() <= clock_->Now();
}

bool FacetManager::IsCachedDataFresh() const {
  return clock_->Now() < GetCacheHardExpiryTime();
}

bool FacetManager::IsCachedDataNearStale() const {
  return GetCacheSoftExpiryTime() <= clock_->Now();
}

base::Time FacetManager::GetCacheSoftExpiryTime() const {
  return last_update_time_ + base::Hours(kCacheSoftExpiryInHours);
}

base::Time FacetManager::GetCacheHardExpiryTime() const {
  return last_update_time_ + base::Hours(kCacheHardExpiryInHours);
}

base::Time FacetManager::GetMaximumKeepFreshUntilThreshold() const {
  return !keep_fresh_until_thresholds_.empty()
             ? *keep_fresh_until_thresholds_.rbegin()
             : base::Time();
}

base::Time FacetManager::GetNextRequiredFetchTimeDueToPrefetch() const {
  // If there is at least one non-expired Prefetch() request that requires the
  // data to be kept fresh until some time later than its current hard expiry
  // time, then a fetch is needed once the cached data becomes near-stale.
  if (clock_->Now() < GetMaximumKeepFreshUntilThreshold() &&
      GetCacheHardExpiryTime() < GetMaximumKeepFreshUntilThreshold()) {
    return GetCacheSoftExpiryTime();
  }
  return base::Time::Max();
}

// static
void FacetManager::ServeRequestWithSuccess(
    RequestInfo request_info,
    const AffiliatedFacets& affiliation) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(request_info.callback), affiliation, true));
}

// static
void FacetManager::ServeRequestWithFailure(RequestInfo request_info) {
  request_info.callback_task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(request_info.callback),
                                AffiliatedFacets(), false));
}

}  // namespace affiliations
