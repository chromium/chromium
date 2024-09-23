// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/memory/raw_ptr.h"
#include "components/affiliations/core/browser/affiliation_fetcher_delegate.h"
#include "components/affiliations/core/browser/affiliation_fetcher_factory.h"
#include "components/affiliations/core/browser/affiliation_fetcher_interface.h"

namespace affiliations {

// A fake AffiliationFetcher that can be used in tests to return fake API
// responses to users of AffiliationFetcher.
class FakeAffiliationFetcher : public AffiliationFetcherInterface {
 public:
  FakeAffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);
  ~FakeAffiliationFetcher() override;

  // Simulates successful completion of the request with |fake_result|. Note
  // that the consumer may choose to destroy |this| from within this call.
  void SimulateSuccess(
      std::unique_ptr<AffiliationFetcherDelegate::Result> fake_result);

  // Simulates completion of the request with failure. Note that the consumer
  // may choose to destroy |this| from within this call.
  void SimulateFailure();

  // AffiliationFetcherInterface
  void StartRequest(const std::vector<FacetURI>& facet_uris,
                    RequestInfo request_info) override;
  const std::vector<FacetURI>& GetRequestedFacetURIs() const override;

 private:
  const raw_ptr<AffiliationFetcherDelegate> delegate_;

  std::vector<FacetURI> facets_;
};

// Used in tests to return fake API responses to users of AffiliationFetcher.
class FakeAffiliationFetcherFactory : public AffiliationFetcherFactory {
 public:
  FakeAffiliationFetcherFactory();
  ~FakeAffiliationFetcherFactory() override;

  // Returns the next FakeAffiliationFetcher instance previously produced, so
  // that that the testing code can inject a response and simulate completion
  // or failure of the request. The fetcher is removed from the queue of pending
  // fetchers.
  //
  // Note that the factory does not retain ownership of the produced fetchers,
  // so that the tests should ensure that the corresponding production code will
  // not destroy them before they are accessed here.
  FakeAffiliationFetcher* PopNextFetcher();

  // Same as above, but the fetcher is not removed from the queue of pending
  // fetchers.
  FakeAffiliationFetcher* PeekNextFetcher();

  bool has_pending_fetchers() const { return !pending_fetchers_.empty(); }

  // AffiliationFetcherFactory:
  std::unique_ptr<AffiliationFetcherInterface> CreateInstance(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate) override;

 private:
  // Fakes created by this factory.
  base::queue<raw_ptr<FakeAffiliationFetcher, CtnExperimental>>
      pending_fetchers_;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_FETCHER_H_
