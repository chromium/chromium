// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/affiliation/affiliation_service.h"

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_delegate.h"
#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_interface.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_factory_impl.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}

namespace url {
class SchemeHostPort;
}

namespace password_manager {

class AffiliationBackend;
struct PasswordFormDigest;

extern const char kGetChangePasswordURLMetricName[];

class AffiliationServiceImpl : public AffiliationService,
                               public AffiliationFetcherDelegate {
 public:
  struct ChangePasswordUrlMatch {
    GURL change_password_url;
    bool group_url_override;
  };

  explicit AffiliationServiceImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<base::SequencedTaskRunner> backend_task_runner);
  ~AffiliationServiceImpl() override;

  AffiliationServiceImpl(const AffiliationServiceImpl& other) = delete;
  AffiliationServiceImpl& operator=(const AffiliationServiceImpl& rhs) = delete;

  // Initializes the service by creating its backend and transferring it to the
  // thread corresponding to |backend_task_runner_|.
  void Init(network::NetworkConnectionTracker* network_connection_tracker,
            const base::FilePath& db_path);

  // Shutdowns the service by deleting its backend.
  void Shutdown() override;

  // Prefetches change password URLs and saves them to |change_password_urls_|
  // map. Creates a unique fetcher and appends it to |pending_fetches_|
  // along with |urls| and |callback|. When prefetch is finished or a fetcher
  // gets destroyed as a result of Clear() a callback is run.
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

  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      AffiliationService::StrategyOnCacheMiss cache_miss_strategy,
      ResultCallback result_callback) override;

  void Prefetch(const FacetURI& facet_uri,
                const base::Time& keep_fresh_until) override;

  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until) override;
  void TrimCacheForFacetURI(const FacetURI& facet_uri) override;
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) override;
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetAllGroups(GroupsCallback callback) const override;
  void InjectAffiliationAndBrandingInformation(
      std::vector<std::unique_ptr<PasswordForm>> forms,
      AffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
      PasswordFormsOrErrorCallback result_callback) override;

  // Returns whether or not |form| represents an Android credential.
  static bool IsValidAndroidCredential(const PasswordFormDigest& form);

  AffiliationBackend* GetBackendForTesting() { return backend_; }

 private:
  struct FetchInfo;

  // AffiliationFetcherDelegate:
  void OnFetchSucceeded(
      AffiliationFetcherInterface* fetcher,
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed(AffiliationFetcherInterface* fetcher) override;
  void OnMalformedResponse(AffiliationFetcherInterface* fetcher) override;

  // Called back by AffiliationService to supply the list of facets
  // affiliated with the Android credential in |form|. Injects affiliation and
  // branding information by setting |affiliated_web_realm|, |app_display_name|
  // and |app_icon_url| on |form| if |success| is true and |results| is
  // non-empty. Invokes |barrier_closure|.
  void CompleteInjectAffiliationAndBrandingInformation(
      PasswordForm* form,
      base::OnceClosure barrier_closure,
      const AffiliatedFacets& results,
      bool success);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::map<url::SchemeHostPort, ChangePasswordUrlMatch> change_password_urls_;
  std::vector<FetchInfo> pending_fetches_;
  std::unique_ptr<AffiliationFetcherFactory> fetcher_factory_;

  // The backend, owned by this AffiliationService instance, but
  // living on the backend thread. It will be deleted asynchronously during
  // shutdown on the backend thread, so it will outlive |this| along with all
  // its in-flight tasks.
  raw_ptr<AffiliationBackend, DanglingUntriaged> backend_;

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AffiliationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace password_manager
#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_AFFILIATION_SERVICE_IMPL_H_
