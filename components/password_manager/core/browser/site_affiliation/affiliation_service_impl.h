// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "components/password_manager/core/browser/site_affiliation/affiliation_service.h"

#include "base/memory/scoped_refptr.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher_interface.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace syncer {
class SyncService;
}

namespace url {
class SchemeHostPort;
}

namespace password_manager {

extern const char kGetChangePasswordURLMetricName[];

class AffiliationServiceImpl : public AffiliationService,
                               public AffiliationFetcherDelegate {
 public:
  struct ChangePasswordUrlMatch {
    GURL change_password_url;
    bool group_url_override;
  };

  explicit AffiliationServiceImpl(
      syncer::SyncService* sync_service,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AffiliationServiceImpl() override;

  // Prefetches change password URLs and saves them to |change_password_urls_|
  // map. The verification if affiliation based matching is enabled must be
  // performed. Assigns the callback to |result_callback_|.
  void PrefetchChangePasswordURLs(const std::vector<GURL>& urls,
                                  base::OnceClosure callback) override;

  // Clears the |change_password_urls_| map and cancels prefetch if still
  // running.
  void Clear() override;

  // In case no valid URL was found, a method returns an empty URL.
  GURL GetChangePasswordURL(const GURL& url) const override;

  AffiliationFetcherInterface* GetFetcherForTesting() { return fetcher_.get(); }

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = std::move(url_loader_factory);
  }

  void SetSyncServiceForTesting(syncer::SyncService* sync_service) {
    sync_service_ = sync_service;
  }

 private:
  // AffiliationFetcherDelegate:
  void OnFetchSucceeded(
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed() override;
  void OnMalformedResponse() override;

  // Converts new |urls| to facets and inserts them to the
  // |change_password_urls_|.
  std::vector<FacetURI> ConvertMissingURLsToFacets(
      const std::vector<GURL>& urls);

  // Calls Affiliation Fetcher and starts a request for |facets| affiliations.
  void RequestFacetsAffiliations(
      const std::vector<FacetURI>& facets,
      const AffiliationFetcherInterface::RequestInfo request_info);

  syncer::SyncService* sync_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::vector<url::SchemeHostPort> requested_tuple_origins_;
  std::map<url::SchemeHostPort, ChangePasswordUrlMatch> change_password_urls_;
  // TODO(crbug.com/1117045): A vector of pending fetchers to be created.
  std::unique_ptr<AffiliationFetcherInterface> fetcher_;
  // Callback is passed in PrefetchChangePasswordURLs and is run in
  // OnFetchSucceeded, OnFetchMalformed, OnFetchFailed to indicate the prefetch
  // has finished.
  base::OnceClosure result_callback_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
