// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_

#include "components/affiliations/core/browser/affiliation_service.h"

namespace affiliations {

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
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetGroupingInfo(std::vector<FacetURI> facet_uris,
                       GroupsCallback callback) override;
  void GetPSLExtensions(base::OnceCallback<void(std::vector<std::string>)>
                            callback) const override;
  void UpdateAffiliationsAndBranding(const std::vector<FacetURI>& facets,
                                     base::OnceClosure callback) override;
  void RegisterSource(std::unique_ptr<AffiliationSource> source) override;
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_
