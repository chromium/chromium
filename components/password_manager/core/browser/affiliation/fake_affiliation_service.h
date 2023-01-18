// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_FAKE_AFFILIATION_SERVICE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_FAKE_AFFILIATION_SERVICE_H_

#include "components/password_manager/core/browser/affiliation/affiliation_service.h"
#include "components/password_manager/core/browser/password_form.h"

namespace password_manager {

class FakeAffiliationService : public AffiliationService {
 public:
  FakeAffiliationService();
  ~FakeAffiliationService() override;

  void PrefetchChangePasswordURLs(const std::vector<GURL>& urls,
                                  base::OnceClosure callback) override;
  void Clear() override;
  GURL GetChangePasswordURL(const GURL& url) const override;
  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      AffiliationService::StrategyOnCacheMiss cache_miss_strategy,
      ResultCallback result_callback) override;
  void Prefetch(const FacetURI& facet_uri,
                const base::Time& keep_fresh_until) override;
  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until) override;
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) override;
  void TrimCacheForFacetURI(const FacetURI& facet_uri) override;
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetAllGroups(GroupsCallback callback) const override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_AFFILIATION_FAKE_AFFILIATION_SERVICE_H_
