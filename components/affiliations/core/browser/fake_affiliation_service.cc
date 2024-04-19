// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/fake_affiliation_service.h"

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "components/affiliations/core/browser/affiliation_utils.h"

namespace affiliations {

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
void FakeAffiliationService::TrimUnusedCache(std::vector<FacetURI> facet_uris) {
}
void FakeAffiliationService::GetGroupingInfo(std::vector<FacetURI> facet_uris,
                                             GroupsCallback callback) {
  // Put each facet into its own group because AffiliationService is supposed to
  // always return result for each requested facet.
  std::vector<GroupedFacets> result(facet_uris.size());
  result.reserve(facet_uris.size());
  for (size_t i = 0; i < facet_uris.size(); i++) {
    result[i].facets = {Facet(std::move(facet_uris[i]))};
  }
  std::move(callback).Run(std::move(result));
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
void FakeAffiliationService::RegisterSource(
    std::unique_ptr<AffiliationSource> source) {}

}  // namespace affiliations
