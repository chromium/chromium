// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/affiliation_fetcher_manager.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory_impl.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace affiliations {

AffiliationFetcherManager::AffiliationFetcherManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory),
      fetcher_factory_(std::make_unique<AffiliationFetcherFactoryImpl>()) {}

AffiliationFetcherManager::~AffiliationFetcherManager() = default;

void AffiliationFetcherManager::Fetch(
    const std::vector<FacetURI>& facet_uris,
    AffiliationFetcherInterface::RequestInfo request_info,
    base::OnceCallback<void(AffiliationFetcherInterface::FetchResult)>
        completion_callback) {
  std::unique_ptr<AffiliationFetcherInterface> fetcher =
      fetcher_factory_->CreateInstance(url_loader_factory_);
  if (!fetcher) {
    std::move(completion_callback)
        .Run(AffiliationFetcherInterface::FetchResult());
    return;
  }

  auto cleanup_callback =
      std::move(completion_callback)
          .Then(base::BindOnce(&AffiliationFetcherManager::CleanUpFetcher,
                               weak_ptr_factory_.GetWeakPtr(), fetcher.get()));
  fetcher->StartRequest(facet_uris, request_info, std::move(cleanup_callback));
  fetchers_.push_back(std::move(fetcher));
}

void AffiliationFetcherManager::CleanUpFetcher(
    AffiliationFetcherInterface* fetcher) {
  auto fetcher_it = std::find_if(fetchers_.begin(), fetchers_.end(),
                                 [fetcher](const auto& stored_fetcher) {
                                   return stored_fetcher.get() == fetcher;
                                 });
  if (fetcher_it != fetchers_.end()) {
    fetchers_.erase(fetcher_it);
  }
}

std::vector<FacetURI> AffiliationFetcherManager::GetRequestedFacetURIs() const {
  std::vector<FacetURI> requested_facet_uris;
  for (const auto& fetcher : fetchers_) {
    auto facets = fetcher->GetRequestedFacetURIs();
    requested_facet_uris.insert(requested_facet_uris.end(), facets.begin(),
                                facets.end());
  }
  return requested_facet_uris;
}

bool AffiliationFetcherManager::IsFetchPossible() const {
  return fetcher_factory_->CanCreateFetcher();
}

}  // namespace affiliations
