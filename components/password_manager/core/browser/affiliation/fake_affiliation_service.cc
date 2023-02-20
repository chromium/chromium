// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"
#include "components/password_manager/core/browser/affiliation/affiliation_utils.h"

namespace password_manager {

FakeAffiliationService::FakeAffiliationService() = default;

FakeAffiliationService::~FakeAffiliationService() = default;

void FakeAffiliationService::PrefetchChangePasswordURLs(
    const std::vector<GURL>& urls,
    base::OnceClosure callback) {}
void FakeAffiliationService::Clear() {}
GURL FakeAffiliationService::GetChangePasswordURL(const GURL& url) const {
  return GURL();
}
void FakeAffiliationService::GetAffiliationsAndBranding(
    const FacetURI& facet_uri,
    AffiliationService::StrategyOnCacheMiss cache_miss_strategy,
    ResultCallback result_callback) {
  AffiliatedFacets affiliations;
  affiliations.push_back(Facet(facet_uri, FacetBrandingInfo(), GURL()));
  std::move(result_callback).Run(affiliations, /*success=*/true);
}
void FakeAffiliationService::Prefetch(const FacetURI& facet_uri,
                                      const base::Time& keep_fresh_until) {}
void FakeAffiliationService::CancelPrefetch(
    const FacetURI& facet_uri,
    const base::Time& keep_fresh_until) {}
void FakeAffiliationService::KeepPrefetchForFacets(
    std::vector<FacetURI> facet_uris) {}
void FakeAffiliationService::TrimCacheForFacetURI(const FacetURI& facet_uri) {}
void FakeAffiliationService::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
}
void FakeAffiliationService::GetAllGroups(GroupsCallback callback) const {
  std::move(callback).Run({});
}
void FakeAffiliationService::GetPSLExtensions(
    base::OnceCallback<void(std::vector<std::string>)> callback) const {
  std::move(callback).Run({});
}
void FakeAffiliationService::UpdateAffiliationsAndBranding(
    const std::vector<FacetURI>& facets,
    base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace password_manager
