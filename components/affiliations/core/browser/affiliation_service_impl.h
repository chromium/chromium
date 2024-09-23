// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_IMPL_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_IMPL_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory_impl.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_prefetcher.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace network {
class NetworkConnectionTracker;
class SharedURLLoaderFactory;
}  // namespace network

namespace url {
class SchemeHostPort;
}

namespace affiliations {

extern const char kGetChangePasswordURLMetricName[];

// Change password info request requires branding_info enabled.
constexpr AffiliationFetcherInterface::RequestInfo
    kChangePasswordUrlRequestInfo{.branding_info = true,
                                  .change_password_info = true};

// Used to record metrics for the usage and timing of the GetChangePasswordUrl
// call. These values are persisted to logs. Entries should not be renumbered
// and numeric values should never be reused.
enum class GetChangePasswordUrlMetric {
  // Used when GetChangePasswordUrl is called before the response
  // arrives.
  kNotFetchedYet = 0,
  // Used when a url was used, which corresponds to the requested site.
  kUrlOverrideUsed = 1,
  // Used when no override url was available.
  kNoUrlOverrideAvailable = 2,
  // Used when a url was used, which corresponds to a site from within same
  // FacetGroup.
  kGroupUrlOverrideUsed = 3,
  kMaxValue = kGroupUrlOverrideUsed,
};

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
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) override;
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetGroupingInfo(std::vector<FacetURI> facet_uris,
                       GroupsCallback callback) override;
  void GetPSLExtensions(base::OnceCallback<void(std::vector<std::string>)>
                            callback) const override;
  void UpdateAffiliationsAndBranding(const std::vector<FacetURI>& facets,
                                     base::OnceClosure callback) override;
  void RegisterSource(std::unique_ptr<AffiliationSource> source) override;

  AffiliationBackend* GetBackendForTesting() { return backend_.get(); }

 private:
  struct FetchInfo;

  template <typename Method, typename... Args>
  void PostToBackend(const Method& method, Args&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // If `backend` is destroyed there is nothing to do.
    if (!backend_) {
      return;
    }

    backend_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(method, base::Unretained(backend_.get()),
                                  std::forward<Args>(args)...));
  }

  // AffiliationFetcherDelegate:
  void OnFetchSucceeded(
      AffiliationFetcherInterface* fetcher,
      std::unique_ptr<AffiliationFetcherDelegate::Result> result) override;
  void OnFetchFailed(AffiliationFetcherInterface* fetcher) override;
  void OnMalformedResponse(AffiliationFetcherInterface* fetcher) override;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::map<url::SchemeHostPort, ChangePasswordUrlMatch> change_password_urls_;
  std::vector<FetchInfo> pending_fetches_;
  std::unique_ptr<AffiliationFetcherFactory> fetcher_factory_;
  AffiliationPrefetcher prefetcher_{this};

  // The backend, owned by this AffiliationService instance, but
  // living on the backend thread. It will be deleted asynchronously during
  // shutdown on the backend thread, so it will outlive `this` along with all
  // its in-flight tasks.
  std::unique_ptr<AffiliationBackend> backend_;

  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<AffiliationServiceImpl> weak_ptr_factory_{this};
};

}  // namespace affiliations
#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_IMPL_H_
