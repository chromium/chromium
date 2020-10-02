// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/hash_affiliation_fetcher.h"

#include <memory>

#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace password_manager {

HashAffiliationFetcher::HashAffiliationFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    AffiliationFetcherDelegate* delegate)
    : AffiliationFetcherBase(std::move(url_loader_factory), delegate) {}

HashAffiliationFetcher::~HashAffiliationFetcher() = default;

// TODO: Add an implementation.
void HashAffiliationFetcher::StartRequest(
    const std::vector<FacetURI>& facet_uris,
    RequestInfo request_info) {}

const std::vector<FacetURI>& HashAffiliationFetcher::GetRequestedFacetURIs()
    const {
  return requested_facet_uris_;
}
// static
GURL HashAffiliationFetcher::BuildQueryURL() {
  return net::AppendQueryParameter(
      GURL("https://www.googleapis.com/affiliation/v1/"
           "affiliation:lookupByHashPrefix"),
      "key", google_apis::GetAPIKey());
}

}  // namespace password_manager
