// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_service.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

FakeAffiliationService::FakeAffiliationService() = default;

FakeAffiliationService::~FakeAffiliationService() = default;

void FakeAffiliationService::AddAffiliationGroup(const AffiliatedFacets& group,
                                                 bool add_to_cache) {
  affiliation_groups_.push_back(group);
  if (add_to_cache) {
    for (const Facet& facet : group) {
      cache_.insert(facet.uri);
    }
  }
}

void FakeAffiliationService::AddGroupedFacets(const GroupedFacets& group,
                                              bool add_to_cache) {
  grouped_facets_.push_back(group);
  if (add_to_cache) {
    for (const Facet& facet : group.facets) {
      cache_.insert(facet.uri);
    }
  }
}

void FakeAffiliationService::SetPSLExtensions(
    std::vector<std::string> psl_extensions) {
  psl_extensions_ = std::move(psl_extensions);
}

void FakeAffiliationService::FetchChangePasswordURL(
    const GURL& url,
    base::OnceCallback<void(GURL)> callback) {
  std::move(callback).Run(GURL());
}

GURL FakeAffiliationService::GetChangePasswordURL(const GURL& url) const {
  return GURL();
}

void FakeAffiliationService::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    ResultCallback result_callback) {
  if (!cache_.contains(facet_uri)) {
    std::move(result_callback).Run(AffiliatedFacets(), /*success=*/false);
    return;
  }

  for (const AffiliatedFacets& group : affiliation_groups_) {
    if (std::ranges::any_of(group, [&](const Facet& facet) {
          return facet.uri == facet_uri;
        })) {
      std::move(result_callback).Run(group, /*success=*/true);
      return;
    }
  }

  // If no match found in seeded groups, guarantee self hit by returning a
  // single-element group containing the queried facet itself.
  std::move(result_callback)
      .Run({Facet(facet_uri, FacetBrandingInfo(), GURL())}, /*success=*/true);
}

void FakeAffiliationService::Prefetch(const FacetURI& facet_uri,
                                      const base::Time& keep_fresh_until) {}

void FakeAffiliationService::CancelPrefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {}

void FakeAffiliationService::KeepPrefetchForFacets(
    std::vector<FacetURI> facet_uris) {}

void FakeAffiliationService::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
}

void FakeAffiliationService::GetGroupingInfo(std::vector<FacetURI> facet_uris,
                                             GroupsCallback callback) {
  std::vector<GroupedFacets> result;
  result.reserve(facet_uris.size());
  for (const FacetURI& facet_uri : facet_uris) {
    GroupedFacets matched_group;
    matched_group.facets.push_back(Facet(facet_uri));

    for (const GroupedFacets& group : grouped_facets_) {
      if (std::ranges::any_of(group.facets, [&](const Facet& facet) {
            return facet.uri == facet_uri;
          })) {
        matched_group = group;
        break;
      }
    }
    result.push_back(std::move(matched_group));
  }
  std::move(callback).Run(std::move(result));
}

void FakeAffiliationService::GetPSLExtensions(
    base::OnceCallback<void(std::vector<std::string>)> callback) {
  std::move(callback).Run(psl_extensions_);
}

void FakeAffiliationService::UpdateAffiliationsAndBranding(
    const std::vector<FacetURI>& facets,
    base::OnceClosure callback) {
  for (const auto& facet : facets) {
    cache_.insert(facet);
  }
  std::move(callback).Run();
}

void FakeAffiliationService::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {}

base::WeakPtr<AffiliationService> FakeAffiliationService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace affiliations
