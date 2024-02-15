// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#include <utility>

namespace affiliations {

FakeAffiliationFetcher::FakeAffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : delegate_(delegate) {}

FakeAffiliationFetcher::~FakeAffiliationFetcher() = default;

void FakeAffiliationFetcher::SimulateSuccess(
    std::unique_ptr<AffiliationFetcherDelegate::Result> fake_result) {
  delegate_->OnFetchSucceeded(this, std::move(fake_result));
}

void FakeAffiliationFetcher::SimulateFailure() {
  delegate_->OnFetchFailed(this);
}

void FakeAffiliationFetcher::StartRequest(
    const std::vector<FacetURI>& facet_uris,
    RequestInfo request_info) {
  facets_ = facet_uris;
}
const std::vector<FacetURI>&
FakeAffiliationFetcher::GetRequestedFacetURIs() const {
  return facets_;
}

FakeAffiliationFetcherFactory::
    FakeAffiliationFetcherFactory() = default;

FakeAffiliationFetcherFactory::
    ~FakeAffiliationFetcherFactory() {
  CHECK(pending_fetchers_.empty());
}

FakeAffiliationFetcher* FakeAffiliationFetcherFactory::PopNextFetcher() {
  DCHECK(!pending_fetchers_.empty());
  FakeAffiliationFetcher* first = pending_fetchers_.front();
  pending_fetchers_.pop();
  return first;
}

FakeAffiliationFetcher* FakeAffiliationFetcherFactory::PeekNextFetcher() {
  DCHECK(!pending_fetchers_.empty());
  return pending_fetchers_.front();
}

std::unique_ptr<AffiliationFetcherInterface>
FakeAffiliationFetcherFactory::CreateInstance(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate) {
  auto fetcher = std::make_unique<FakeAffiliationFetcher>(
      std::move(url_loader_factory), delegate);
  pending_fetchers_.push(fetcher.get());
  return fetcher;
}

}  // namespace affiliations
