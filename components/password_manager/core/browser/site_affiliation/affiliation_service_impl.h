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
#include "components/password_manager/core/browser/site_affiliation/affiliation_fetcher_factory_impl.h"

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
  // performed. Creates a unique fetcher and appends it to |pending_fetches_|
  // along with |urls| and |callback|.
  void PrefetchChangePasswordURLs(const std::vector<GURL>& urls,
                                  base::OnceClosure callback) override;

  // Clears the |change_password_urls_| map and cancels prefetch requests if
  // still running.
  void Clear() override;

  // In case no valid URL was found, a method returns an empty URL.
  GURL GetChangePasswordURL(const GURL& url) const override;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = std::move(url_loader_factory);
  }

  void SetFetcherFactoryForTesting(
      std::unique_ptr<AffiliationFetcherFactory> fetcher_factory) {
    fetcher_factory_ = std::move(fetcher_factory);
  }

  void SetSyncServiceForTesting(syncer::SyncService* sync_service) {
    sync_service_ = sync_service;
  }

 private:
  struct FetchInfo;

  // AffiliationFetcherDelegate:
  void OnFetchSucceeded(
      AffiliationFetcherInterface* fetcher,
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed(AffiliationFetcherInterface* fetcher) override;
  void OnMalformedResponse(AffiliationFetcherInterface* fetcher) override;

  // Creates AffiliationFetcher and starts a request to retrieve affiliations
  // for given |urls|. |Request_info| defines what info should be requested.
  // When prefetch is finished or a fetcher gets destroyed as a result of
  // Clear() a callback is run.
  void RequestFacetsAffiliations(
      const std::vector<GURL>& urls,
      const AffiliationFetcherInterface::RequestInfo request_info,
      base::OnceClosure callback);

  syncer::SyncService* sync_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::map<url::SchemeHostPort, ChangePasswordUrlMatch> change_password_urls_;
  std::vector<FetchInfo> pending_fetches_;
  std::unique_ptr<AffiliationFetcherFactory> fetcher_factory_;
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SITE_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
