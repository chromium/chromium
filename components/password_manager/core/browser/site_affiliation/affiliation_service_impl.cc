// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/site_affiliation/affiliation_service_impl.h"

#include "base/metrics/histogram_functions.h"
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
std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
CreateFacetUriToChangePasswordUrlMap(
    const std::vector<GroupedFacets>& groupings) {
  std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch> uri_to_url;
  for (const auto& grouped_facets : groupings) {
    std::vector<FacetURI> uris_without_urls;
    GURL fallback_url;
    for (const auto& facet : grouped_facets) {
      if (!facet.change_password_url.is_valid()) {
        uris_without_urls.push_back(facet.uri);
        continue;
      }
      uri_to_url[facet.uri] = AffiliationServiceImpl::ChangePasswordUrlMatch{
          .change_password_url = facet.change_password_url,
          .group_url_override = false};
      fallback_url = facet.change_password_url;
    }
    if (fallback_url.is_valid()) {
      for (const auto& uri : uris_without_urls) {
        uri_to_url[uri] = AffiliationServiceImpl::ChangePasswordUrlMatch{
            .change_password_url = fallback_url, .group_url_override = true};
      }
    }
  }
  return uri_to_url;
}

}  // namespace

const char kGetChangePasswordURLMetricName[] =
    "PasswordManager.AffiliationService.GetChangePasswordUsage";

AffiliationServiceImpl::AffiliationServiceImpl(
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : sync_service_(sync_service),
      url_loader_factory_(std::move(url_loader_factory)) {}

AffiliationServiceImpl::~AffiliationServiceImpl() = default;

void AffiliationServiceImpl::PrefetchChangePasswordURLs(
    const std::vector<GURL>& urls) {
  if (ShouldAffiliationBasedMatchingBeActive(sync_service_)) {
    RequestFacetsAffiliations(ConvertMissingURLsToFacets(urls),
                              {.change_password_info = true});
  }
}

void AffiliationServiceImpl::Clear() {
  fetcher_.reset();
  change_password_urls_.clear();
}

GURL AffiliationServiceImpl::GetChangePasswordURL(const GURL& url) const {
  auto it = change_password_urls_.find(url::SchemeHostPort(url));
  if (it != change_password_urls_.end()) {
    if (it->second.group_url_override) {
      base::UmaHistogramEnumeration(
          kGetChangePasswordURLMetricName,
          metrics_util::GetChangePasswordUrlMetric::kGroupUrlOverrideUsed);
    } else {
      base::UmaHistogramEnumeration(
          kGetChangePasswordURLMetricName,
          metrics_util::GetChangePasswordUrlMetric::kUrlOverrideUsed);
    }
    return it->second.change_password_url;
  }
  if (base::Contains(requested_tuple_origins_, url::SchemeHostPort(url))) {
    base::UmaHistogramEnumeration(
        kGetChangePasswordURLMetricName,
        metrics_util::GetChangePasswordUrlMetric::kNotFetchedYet);
  } else {
    base::UmaHistogramEnumeration(
        kGetChangePasswordURLMetricName,
        metrics_util::GetChangePasswordUrlMetric::kNoUrlOverrideAvailable);
  }
  return GURL();
}

void AffiliationServiceImpl::OnFetchSucceeded(
    std::unique_ptr<AffiliationFetcherDelegate::Result> result) {
  fetcher_.reset();
  std::map<FacetURI, AffiliationServiceImpl::ChangePasswordUrlMatch>
      uri_to_url = CreateFacetUriToChangePasswordUrlMap(result->groupings);
  for (const url::SchemeHostPort& requested_tuple : requested_tuple_origins_) {
    auto it = uri_to_url.find(
        FacetURI::FromPotentiallyInvalidSpec(requested_tuple.Serialize()));
    if (it != uri_to_url.end()) {
      change_password_urls_[requested_tuple] = it->second;
    }
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

std::vector<FacetURI> AffiliationServiceImpl::ConvertMissingURLsToFacets(
    const std::vector<GURL>& urls) {
  std::vector<FacetURI> facets;
  for (const auto& url : urls) {
    if (url.is_valid()) {
      url::SchemeHostPort scheme_host_port(url);
      if (!base::Contains(change_password_urls_, scheme_host_port)) {
        requested_tuple_origins_.push_back(std::move(scheme_host_port));
        facets.push_back(FacetURI::FromCanonicalSpec(url.spec()));
      }
    }
  }
  return facets;
}

// TODO(crbug.com/1117045): New request resets the pointer to
// AffiliationFetcher, therefore the previous request gets canceled.
void AffiliationServiceImpl::RequestFacetsAffiliations(
    const std::vector<FacetURI>& facets,
    const AffiliationFetcherInterface::RequestInfo request_info) {
  if (!facets.empty()) {
    fetcher_ = AffiliationFetcher::Create(url_loader_factory_, this);
    fetcher_->StartRequest(facets, request_info);
  }
}

}  // namespace password_manager
