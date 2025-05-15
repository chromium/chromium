// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_MANAGER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_MANAGER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {
// Class for managing instances of |AffiliationFetcherInterface| created for
// individual requests. Each fetcher will live between a call to |Fetch| and a
// completion of the started fetch.
class AffiliationFetcherManager {
 public:
  explicit AffiliationFetcherManager(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  AffiliationFetcherManager(const AffiliationFetcherManager&) = delete;
  AffiliationFetcherManager& operator=(const AffiliationFetcherManager&) =
      delete;
  ~AffiliationFetcherManager();

  // Starts a fetch for the given facet_uris and request_info. Internally this
  // will create a new |AffiliationFetcherInterface|, store it in |fetchers_|
  // and clean in up once the fetch is completed.
  void Fetch(const std::vector<FacetURI>& facet_uris,
             AffiliationFetcherInterface::RequestInfo request_info,
             base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
                 completion_callback);

  // Returns all the |FacetURI|s that are currently being fetched.
  std::vector<FacetURI> GetRequestedFacetURIs() const;

  // Returns whether Fetch can be scheduled.
  bool IsFetchPossible() const;

#if defined(UNIT_TEST)
  // Only for use in tests.
  std::vector<std::unique_ptr<AffiliationFetcherInterface>>*
  GetFetchersForTesting() {
    return &fetchers_;
  }

  void SetFetcherFactoryForTesting(
      std::unique_ptr<AffiliationFetcherFactory> fetcher_factory) {
    fetcher_factory_ = std::move(fetcher_factory);
  }
#endif

 private:
  // Erases |fetcher| from |fetchers_| after receiving the result and forwards
  // the result to the caller of |Fetch|.
  void CleanUpFetcher(AffiliationFetcherInterface* fetcher);

  std::vector<std::unique_ptr<AffiliationFetcherInterface>> fetchers_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<AffiliationFetcherFactory> fetcher_factory_;

  base::WeakPtrFactory<AffiliationFetcherManager> weak_ptr_factory_{this};
};
}  // namespace affiliations
#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_AFFILIATION_FETCHER_MANAGER_H_
