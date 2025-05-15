// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_fetcher.h"

#include <utility>

#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace affiliations {

FakeAffiliationFetcher::FakeAffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {}

FakeAffiliationFetcher::~FakeAffiliationFetcher() = default;

void FakeAffiliationFetcher::SimulateSuccess(
    const ParsedFetchResponse& fake_result_data) {
  FetchResult result;
  result.data = fake_result_data;
  result.http_status_code = net::HTTP_OK;
  result.network_status = net::OK;
  std::move(result_callback_).Run(std::move(result));
}

void FakeAffiliationFetcher::SimulateFailure() {
  FetchResult result;
  result.http_status_code = net::HTTP_INTERNAL_SERVER_ERROR;
  result.network_status = net::ERR_HTTP_RESPONSE_CODE_FAILURE;
  std::move(result_callback_).Run(result);
}

void FakeAffiliationFetcher::StartRequest(
    const std::vector<FacetURI>& facet_uris,
    RequestInfo request_info,
    base::OnceCallback<void(FetchResult)> result_callback) {
  facets_ = facet_uris;
  request_info_ = request_info;
  result_callback_ = std::move(result_callback);
}
const std::vector<FacetURI>&
FakeAffiliationFetcher::GetRequestedFacetURIs() const {
  return facets_;
}

const AffiliationFetcherInterface::RequestInfo&
FakeAffiliationFetcher::GetRequestInfo() const {
  return request_info_;
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  auto fetcher =
      std::make_unique<FakeAffiliationFetcher>(std::move(url_loader_factory));
  pending_fetchers_.push(fetcher.get());
  return fetcher;
}

bool FakeAffiliationFetcherFactory::CanCreateFetcher() const {
  return true;
}

}  // namespace affiliations
