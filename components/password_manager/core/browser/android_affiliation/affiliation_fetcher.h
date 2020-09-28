// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_

#include "components/password_manager/core/browser/site_affiliation/affiliation_fetcher_base.h"

namespace password_manager {

class AffiliationFetcher : public AffiliationFetcherBase {
 public:
  AffiliationFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      AffiliationFetcherDelegate* delegate);
  ~AffiliationFetcher() override;

  // AffiliationFetcherInterface
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

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_ANDROID_AFFILIATION_AFFILIATION_FETCHER_H_
