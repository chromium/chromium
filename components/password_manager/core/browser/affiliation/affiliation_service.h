// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_H_

#include <vector>

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_store_backend_error.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

class GURL;

namespace password_manager {

struct PasswordForm;

// A service that can be used to query the list of facets that are affiliated
// with a given facet, i.e., facets that belong to the same logical application.
// See affiliation_utils.h for details of what this means.
class AffiliationService : public KeyedService {
 public:
  // Controls whether to send a network request or fail on a cache miss.
  enum class StrategyOnCacheMiss { FETCH_OVER_NETWORK, FAIL };

  using ResultCallback =
      base::OnceCallback<void(const AffiliatedFacets& /* results */,
                              bool /* success */)>;

  using PasswordFormsOrErrorCallback = base::OnceCallback<void(
      absl::variant<std::vector<std::unique_ptr<PasswordForm>>,
                    PasswordStoreBackendError>)>;

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

  // Wipes results of on-demand fetches and expired prefetches from the
  // cache, but retains information corresponding to facets that are being
  // kept fresh. As no required data is deleted, there will be no network
  // requests directly triggered by this call. It will only potentially
  // remove data corresponding to the given |facet_uri|, but still only as
  // long as the data is no longer needed.
  virtual void TrimCacheForFacetURI(const FacetURI& facet_uri) = 0;

  // Wipes results from cache which don't correspond to the any facet from
  // |facet_uris|.
  virtual void TrimUnusedCache(std::vector<FacetURI> facet_uris) = 0;

  // Retrieves all stored facet groups from the cache. This information can be
  // used to group passwords together.
  virtual void GetAllGroups(GroupsCallback callback) const = 0;

  // Retrieves affiliation and branding information about the Android
  // credentials in |forms|, sets |affiliated_web_realm|, |app_display_name| and
  // |app_icon_url| of forms, and invokes |result_callback|.
  // NOTE: When |strategy_on_cache_miss| is set to |FAIL|, this will not issue
  // an on-demand network request. And if a request to cache fails, no
  // affiliation and branding information will be injected into corresponding
  // form.
  virtual void InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<PasswordForm>> forms,
      AffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
      PasswordFormsOrErrorCallback result_callback) = 0;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_H_
