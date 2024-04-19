// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "components/affiliations/core/browser/affiliation_source.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace affiliations {

// A service that can be used to query the list of facets that are affiliated
// with a given facet, i.e., facets that belong to the same logical application.
// See affiliation_utils.h for details of what this means.
class AffiliationService : public KeyedService {
 public:
  // Controls whether to send a network request or fail on a cache miss.
  enum class StrategyOnCacheMiss {
    // Affiliation service will keep trying to send request with exponential
    // backlog.
    FETCH_OVER_NETWORK,
    // Request will fail immediately.
    FAIL,
    // After first request failure affiliation service will stop trying.
    TRY_ONCE_OVER_NETWORK
  };

  using ResultCallback =
      base::OnceCallback<void(const AffiliatedFacets& /* results */,
                              bool /* success */)>;

  using GroupsCallback =
      base::OnceCallback<void(const std::vector<GroupedFacets>&)>;

  // Prefetches change password URLs for sites requested. Receives a callback to
  // run when the prefetch finishes.
  virtual void PrefetchChangePasswordURLs(const std::vector<GURL>& urls,
                                          base::OnceClosure callback) = 0;

  // Clears the result of URLs fetch.
  virtual void Clear() = 0;

  // Returns a URL with change password form for a site requested.
  virtual GURL GetChangePasswordURL(const GURL& url) const = 0;

  // Looks up facets affiliated with the facet identified by |facet_uri| and
  // branding information, and invokes |result_callback| with the results. It is
  // guaranteed that the results will contain one facet with URI equal to
  // |facet_uri| when |result_callback| is invoked with success set to true.
  //
  // If the local cache contains fresh affiliation and branding information for
  // |facet_uri|, the request will be served from cache. Otherwise,
  // |cache_miss_policy| controls whether to issue an on-demand network request,
  // or to fail the request without fetching.
  virtual void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      AffiliationService::StrategyOnCacheMiss cache_miss_strategy,
      ResultCallback result_callback) = 0;

  // Prefetches affiliation information for the facet identified by
  // |facet_uri|, and keeps the information fresh by periodic re-fetches (as
  // needed) until the clock strikes |keep_fresh_until| (exclusive), until a
  // matching call to CancelPrefetch(), or until Chrome is shut down,
  // whichever is sooner. It is a supported use-case to pass
  // base::Time::Max() as |keep_fresh_until|.
  //
  // Canceling can be useful when a password is deleted, so that resources
  // are no longer wasted on repeatedly refreshing affiliation information.
  // Note that canceling will not blow away data already stored in the cache
  // unless it becomes stale.
  virtual void Prefetch(const FacetURI& facet_uri,
                        const base::Time& keep_fresh_until) = 0;

  // Cancels the corresponding prefetch command, i.e., the one issued for
  // the same |facet_uri| and with the same |keep_fresh_until|.
  virtual void CancelPrefetch(const FacetURI& facet_uri,
                              const base::Time& keep_fresh_until) = 0;

  // Compares |facet_uris| with a actively prefetching list of facets. For any
  // facet which is present in the |facet_uris| but missing from the list a new
  // prefetch is scheduled. For any facet which is present in the list but
  // missing in |facet_uris| the corresponding prefetch command is canceled. It
  // also deletes cache which is no longer needed.
  virtual void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) = 0;

  // Wipes results from cache which don't correspond to the any facet from
  // |facet_uris|.
  virtual void TrimUnusedCache(std::vector<FacetURI> facet_uris) = 0;

  // Retrieves stored grouping info for |facet_uris|. If there is no cache for
  // requested facet, the facet will be added to it's own group. This
  // information can be used to group passwords together.
  virtual void GetGroupingInfo(std::vector<FacetURI> facet_uris,
                               GroupsCallback callback) = 0;

  // Retrieves psl extension list. This list includes domain which shouldn't be
  // considered as PSL match.
  virtual void GetPSLExtensions(
      base::OnceCallback<void(std::vector<std::string>)> callback) const = 0;

  // This method will fetch the latest affiliation and branding information for
  // |facets| even if local cache is still fresh. |callback| is invoked on
  // completion.
  virtual void UpdateAffiliationsAndBranding(
      const std::vector<FacetURI>& facets,
      base::OnceClosure callback) = 0;

  // Registers an affiliation source. Affiliation sources are used for
  // prefetching of affiliation data soon after start-up. They are owned by
  // the prefetcher, and observed for changes in their underlying data model to
  // keep an updated cache of affiliations.
  virtual void RegisterSource(std::unique_ptr<AffiliationSource> source) = 0;
};

}  // namespace affiliations
#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_H_
