// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/affiliation/fake_affiliation_service.h"

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
    ResultCallback result_callback) {}
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

void FakeAffiliationService::InjectAffiliationAndBrandingInformation(
    std::vector<std::unique_ptr<PasswordForm>> forms,
    AffiliationService::StrategyOnCacheMiss strategy_on_cache_miss,
    PasswordFormsOrErrorCallback result_callback) {
  std::move(result_callback).Run(std::move(forms));
}

}  // namespace password_manager
