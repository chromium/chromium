// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"

#include "base/util/ranges/algorithm.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_fetcher.h"
#include "components/password_manager/core/browser/password_store_factory_util.h"
#include "components/sync/driver/sync_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"

namespace password_manager {

namespace {

// Creates a look-up (Facet URI : change password URL) map for facets from
// requested |groupings|. If a facet does not have change password URL it gets
// paired with another facet's URL, which belongs to the same group. In case
// none of the group's facets have change password URLs then those facets are
// not inserted to the map.
std::map<FacetURI, GURL> CreateFacetUriToChangePasswordUrlMap(
    const std::vector<GroupedFacets>& groupings) {
  std::map<FacetURI, GURL> uri_to_url;
  for (const auto& grouped_facets : groupings) {
    std::vector<FacetURI> uris_without_urls;
    GURL fallback_url;
    for (const auto& facet : grouped_facets) {
      if (!facet.change_password_url.is_valid()) {
        uris_without_urls.push_back(facet.uri);
        continue;
      }
      uri_to_url[facet.uri] = facet.change_password_url;
      fallback_url = facet.change_password_url;
    }
    if (fallback_url.is_valid()) {
      for (const auto& uri : uris_without_urls)
        uri_to_url[uri] = fallback_url;
    }
  }
  return uri_to_url;
}

}  // namespace

AffiliationServiceImpl::AffiliationServiceImpl(
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : sync_service_(sync_service),
      url_loader_factory_(std::move(url_loader_factory)) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::PrefetchChangePasswordURLs(
    const std::vector<url::SchemeHostPort>& tuple_origins) {
  if (ShouldAffiliationBasedMatchingBeActive(sync_service_)) {
    RequestFacetsAffiliations(
        ConvertMissingSchemeHostPortsToFacets(tuple_origins),
        {.change_password_info = true});
  }
}

void AffiliationServiceImpl::Clear() {
  fetcher_.reset();
  change_password_urls_.clear();
}

GURL AffiliationServiceImpl::GetChangePasswordURL(
    const url::SchemeHostPort& tuple) const {
  auto it = change_password_urls_.find(tuple);
  return it != change_password_urls_.end() ? it->second : GURL();
}

void AffiliationServiceImpl::OnFetchSucceeded(
    std::unique_ptr<AffiliationFetcherDelegate::Result> result) {
  fetcher_.reset();

  std::map<FacetURI, GURL> uri_to_url =
      CreateFacetUriToChangePasswordUrlMap(result->groupings);
  for (const url::SchemeHostPort& requested_tuple : requested_tuple_origins_) {
    auto it = uri_to_url.find(
        FacetURI::FromPotentiallyInvalidSpec(requested_tuple.Serialize()));
    if (it != uri_to_url.end())
      change_password_urls_[requested_tuple] = it->second;
  }

  requested_tuple_origins_.clear();
}

void AffiliationServiceImpl::OnFetchFailed() {
  fetcher_.reset();
  requested_tuple_origins_.clear();
}

void AffiliationServiceImpl::OnMalformedResponse() {
  fetcher_.reset();
  requested_tuple_origins_.clear();
}

std::vector<FacetURI>
AffiliationServiceImpl::ConvertMissingSchemeHostPortsToFacets(
    const std::vector<url::SchemeHostPort>& tuple_origins) {
  std::vector<FacetURI> facets;
  for (const auto& tuple : tuple_origins) {
    if (tuple.IsValid() && !base::Contains(change_password_urls_, tuple)) {
      requested_tuple_origins_.push_back(tuple);
      facets.push_back(FacetURI::FromCanonicalSpec(tuple.Serialize()));
    }
  }
  return facets;
}

// TODO(crbug.com/1117045): New request resets the pointer to
// AffiliationFetcher, therefore the previous request gets canceled.
void AffiliationServiceImpl::RequestFacetsAffiliations(
    const std::vector<FacetURI>& facets,
    const AffiliationFetcherInterface::RequestInfo request_info) {
  fetcher_ = AffiliationFetcher::Create(url_loader_factory_, this);
  fetcher_->StartRequest(facets, request_info);
}

}  // namespace password_manager
