// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_IMPL_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_SERVICE_IMPL_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_backend.h"
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

namespace affiliations {

// Enables fetching of ePSL during the init of AffiliationServiceImpl. It's
// later used to fetch change password urls properly.
BASE_DECLARE_FEATURE(kCachePSLExtensions);

// Enables fetching patterns for change password URLs.
BASE_DECLARE_FEATURE(kFetchChangePasswordPatterns);

// Change password info request requires branding_info enabled.
constexpr AffiliationFetcherInterface::RequestInfo
    kChangePasswordUrlRequestInfo{.branding_info = true,
                                  .change_password_info = true};

class AffiliationServiceImpl : public AffiliationService {
 public:
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

  // Fetches the change password URL for `url` using a dedicated fetcher and
  // invokes `callback` with the result when the fetch finishes.
  void FetchChangePasswordURL(const GURL& url,
                              base::OnceCallback<void(GURL)> callback) override;

  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
    url_loader_factory_ = std::move(url_loader_factory);
  }

#if defined(UNIT_TEST)
  void SetFetcherFactoryForTesting(
      std::unique_ptr<AffiliationFetcherFactory> fetcher_factory) {
    fetcher_manager_->SetFetcherFactoryForTesting(std::move(fetcher_factory));
  }
#endif

  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      ResultCallback result_callback) override;

  void Prefetch(const FacetURI& facet_uri,
                const base::Time& keep_fresh_until) override;

  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until) override;
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) override;
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetGroupingInfo(std::vector<FacetURI> facet_uris,
                       GroupsCallback callback) override;
  void GetPSLExtensions(
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  void UpdateAffiliationsAndBranding(const std::vector<FacetURI>& facets,
                                     base::OnceClosure callback) override;
  void RegisterSource(std::unique_ptr<AffiliationSource> source) override;
  base::WeakPtr<AffiliationService> AsWeakPtr() override;

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

  void OnFetchFinished(FetchInfo fetch_info,
                       AffiliationFetcherInterface::FetchResult fetch_result);

  void OnPSLExtensionsLoaded(
      base::OnceCallback<void(std::vector<std::string>)> callback,
      std::vector<std::string> psl_extensions);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<AffiliationFetcherManager> fetcher_manager_;
  AffiliationPrefetcher prefetcher_{this};
  base::flat_set<std::string> psl_extension_list_;

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
