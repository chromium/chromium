// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_HASH_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_HASH_AFFILIATION_FETCHER_H_

#include "components/password_manager/core/browser/affiliation/affiliation_fetcher_base.h"

namespace password_manager {

// Fetches authoritative information about facets' affiliations with additional
// privacy layer. It uses SHA-256 to hash facet URLs and sends only a specified
// amount of hash prefixes to eventually retrieve a larger group of affiliations
// including those actually required.
class HashAffiliationFetcher : public AffiliationFetcherBase {
 public:
  HashAffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);
  ~HashAffiliationFetcher() override;

  void StartRequest(const std::vector<FacetURI>& facet_uris,
                    RequestInfo request_info) override;

  // AffiliationFetcherInterface
  const std::vector<FacetURI>& GetRequestedFacetURIs() const override;

  // Builds the URL for the Affiliation API's lookup method.
  static GURL BuildQueryURL();

 private:
  std::vector<FacetURI> requested_facet_uris_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_HASH_AFFILIATION_FETCHER_H_
