// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/affiliation_backend.h"

#include <stdint.h>
#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_database.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetch_throttler.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/android_affiliation/facet_manager.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace password_manager {

AffiliationBackend::AffiliationBackend(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    base::Clock* time_source,
    const base::TickClock* time_tick_source)
    : task_runner_(task_runner),
      clock_(time_source),
      tick_clock_(time_tick_source),
      construction_time_(clock_->Now()),
      weak_ptr_factory_(this) {
  DCHECK_LT(base::Time(), clock_->Now());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AffiliationBackend::~AffiliationBackend() {
}

void AffiliationBackend::Initialize(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_info,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::FilePath& db_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!throttler_);
  throttler_.reset(new AffiliationFetchThrottler(
      this, task_runner_, network_connection_tracker, tick_clock_));

  // TODO(engedy): Currently, when Init() returns false, it always poisons the
  // DB, so subsequent operations will silently fail. Consider either fully
  // committing to this approach and making Init() a void, or handling the
  // return value here. See: https://crbug.com/478831.
  cache_.reset(new AffiliationDatabase());
  cache_->Init(db_path);
  DCHECK(url_loader_factory_info);
  DCHECK(!url_loader_factory_);
  url_loader_factory_ = network::SharedURLLoaderFactory::Create(
      std::move(url_loader_factory_info));
}

void AffiliationBackend::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    StrategyOnCacheMiss cache_miss_strategy,
    const AffiliationService::ResultCallback& callback,
    const scoped_refptr<base::TaskRunner>& callback_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FacetManager* facet_manager = GetOrCreateFacetManager(facet_uri);
  DCHECK(facet_manager);
  facet_manager->GetAffiliationsAndBranding(cache_miss_strategy, callback,
                                            callback_task_runner);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::Prefetch(const FacetURI& facet_uri,
                                  const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  FacetManager* facet_manager = GetOrCreateFacetManager(facet_uri);
  DCHECK(facet_manager);
  facet_manager->Prefetch(keep_fresh_until);

  if (facet_manager->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::CancelPrefetch(const FacetURI& facet_uri,
                                        const base::Time& keep_fresh_until) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto facet_manager_it = facet_managers_.find(facet_uri);
  if (facet_manager_it == facet_managers_.end())
    return;
  facet_manager_it->second->CancelPrefetch(keep_fresh_until);

  if (facet_manager_it->second->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

void AffiliationBackend::TrimCacheForFacetURI(const FacetURI& facet_uri) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AffiliatedFacetsWithUpdateTime affiliation;
  if (cache_->GetAffiliationsAndBrandingForFacetURI(facet_uri, &affiliation))
    DiscardCachedDataIfNoLongerNeeded(affiliation.facets);
}

// static
void AffiliationBackend::DeleteCache(const base::FilePath& db_path) {
  AffiliationDatabase::Delete(db_path);
}

FacetManager* AffiliationBackend::GetOrCreateFacetManager(
    const FacetURI& facet_uri) {
  std::unique_ptr<FacetManager>& facet_manager = facet_managers_[facet_uri];
  if (!facet_manager) {
    facet_manager = std::make_unique<FacetManager>(facet_uri, this, clock_);
  }
  return facet_manager.get();
}

void AffiliationBackend::DiscardCachedDataIfNoLongerNeeded(
    const AffiliatedFacets& affiliated_facets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Discard the equivalence class if there is no facet in the class whose
  // FacetManager claims that it needs to keep the data.
  for (const auto& facet : affiliated_facets) {
    auto facet_manager_it = facet_managers_.find(facet.uri);
    if (facet_manager_it != facet_managers_.end() &&
        !facet_manager_it->second->CanCachedDataBeDiscarded()) {
      return;
    }
  }

  CHECK(!affiliated_facets.empty());
  cache_->DeleteAffiliationsAndBrandingForFacetURI(affiliated_facets[0].uri);
}

void AffiliationBackend::OnSendNotification(const FacetURI& facet_uri) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto facet_manager_it = facet_managers_.find(facet_uri);
  if (facet_manager_it == facet_managers_.end())
    return;
  facet_manager_it->second->NotifyAtRequestedTime();

  if (facet_manager_it->second->CanBeDiscarded())
    facet_managers_.erase(facet_uri);
}

bool AffiliationBackend::ReadAffiliationsAndBrandingFromDatabase(
    const FacetURI& facet_uri,
    AffiliatedFacetsWithUpdateTime* affiliations) {
  return cache_->GetAffiliationsAndBrandingForFacetURI(facet_uri, affiliations);
}

void AffiliationBackend::SignalNeedNetworkRequest() {
  throttler_->SignalNetworkRequestNeeded();
}

void AffiliationBackend::RequestNotificationAtTime(const FacetURI& facet_uri,
                                                   base::Time time) {
  // TODO(engedy): Avoid spamming the task runner; only ever schedule the first
  // callback. crbug.com/437865.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AffiliationBackend::OnSendNotification,
                     weak_ptr_factory_.GetWeakPtr(), facet_uri),
      time - clock_->Now());
}

void AffiliationBackend::OnFetchSucceeded(
    std::unique_ptr<AffiliationFetcherDelegate::Result> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fetcher_.reset();
  throttler_->InformOfNetworkRequestComplete(true);

  for (const AffiliatedFacets& affiliated_facets : *result) {
    AffiliatedFacetsWithUpdateTime affiliation;
    affiliation.facets = affiliated_facets;
    affiliation.last_update_time = clock_->Now();

    std::vector<AffiliatedFacetsWithUpdateTime> obsoleted_affiliations;
    cache_->StoreAndRemoveConflicting(affiliation, &obsoleted_affiliations);

    // Cached data in contradiction with newly stored data automatically gets
    // removed from the DB, and will be stored into |obsoleted_affiliations|.
    // TODO(engedy): Currently, handling this is entirely meaningless unless in
    // the edge case when the user has credentials for two Android applications
    // which are now being de-associated. But even in that case, nothing will
    // explode and the only symptom will be that credentials for the Android
    // application that is not being fetched right now, if any, will not be
    // filled into affiliated applications until the next fetch. Still, this
    // should be implemented at some point by letting facet managers know if
    // data. See: https://crbug.com/478832.

    for (const auto& facet : affiliated_facets) {
      auto facet_manager_it = facet_managers_.find(facet.uri);
      if (facet_manager_it == facet_managers_.end())
        continue;
      FacetManager* facet_manager = facet_manager_it->second.get();
      facet_manager->OnFetchSucceeded(affiliation);
      if (facet_manager->CanBeDiscarded())
        facet_managers_.erase(facet.uri);
    }
  }

  // A subsequent fetch may be needed if any additional
  // GetAffiliationsAndBranding() requests came in while the current fetch was
  // in flight.
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch()) {
      throttler_->SignalNetworkRequestNeeded();
      return;
    }
  }
}

void AffiliationBackend::OnFetchFailed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  fetcher_.reset();
  throttler_->InformOfNetworkRequestComplete(false);

  // Trigger a retry if a fetch is still needed.
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch()) {
      throttler_->SignalNetworkRequestNeeded();
      return;
    }
  }
}

void AffiliationBackend::OnMalformedResponse() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(engedy): Potentially handle this case differently. crbug.com/437865.
  OnFetchFailed();
}

bool AffiliationBackend::OnCanSendNetworkRequest() {
  DCHECK(!fetcher_);
  std::vector<FacetURI> requested_facet_uris;
  for (const auto& facet_manager_pair : facet_managers_) {
    if (facet_manager_pair.second->DoesRequireFetch())
      requested_facet_uris.push_back(facet_manager_pair.first);
  }

  // In case a request is no longer needed, return false to indicate this.
  if (requested_facet_uris.empty())
    return false;

  fetcher_.reset(AffiliationFetcher::Create(url_loader_factory_,
                                            requested_facet_uris, this));
  fetcher_->StartRequest();
  ReportStatistics(requested_facet_uris.size());
  return true;
}

void AffiliationBackend::ReportStatistics(size_t requested_facet_uri_count) {
  UMA_HISTOGRAM_COUNTS_100("PasswordManager.AffiliationBackend.FetchSize",
                           requested_facet_uri_count);

  if (last_request_time_.is_null()) {
    base::TimeDelta delay = clock_->Now() - construction_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "PasswordManager.AffiliationBackend.FirstFetchDelay", delay,
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(3), 50);
  } else {
    base::TimeDelta delay = clock_->Now() - last_request_time_;
    UMA_HISTOGRAM_CUSTOM_TIMES(
        "PasswordManager.AffiliationBackend.SubsequentFetchDelay", delay,
        base::TimeDelta::FromSeconds(1), base::TimeDelta::FromDays(3), 50);
  }
  last_request_time_ = clock_->Now();
}

void AffiliationBackend::SetThrottlerForTesting(
    std::unique_ptr<AffiliationFetchThrottler> throttler) {
  throttler_ = std::move(throttler);
}

}  // namespace password_manager
