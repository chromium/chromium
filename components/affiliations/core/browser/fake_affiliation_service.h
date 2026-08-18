// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace affiliations {

// Fake affiliation service to be used in tests. Posts tasks to simulate
// asynchronous operations.
class FakeAffiliationService : public AffiliationService {
 public:
  FakeAffiliationService();
  explicit FakeAffiliationService(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~FakeAffiliationService() override;

  // Seeds a `group` of affiliated facets.
  //
  // `group` represents the group to seed to the "database" (mimics the fact
  // that some affiliation data is only available through live requests and not
  // through cache).
  // `add_to_cache` controls whether these facets are also pre-populated into
  // the local cache. If `true` (default), queries for these facets will succeed
  // immediately (cache hit). If `false`, queries will return `success=false`
  // (cache miss) until `UpdateAffiliationsAndBranding` is called for them.
  void AddAffiliationGroup(const AffiliatedFacets& group,
                           bool add_to_cache = true);

  // Seeds a `group` of grouped facets.
  //
  // `group` represents the group to seed to the "database" (mimics the fact
  // that some grouping data is only available through live requests and not
  // through cache).
  // `add_to_cache` controls whether these facets are also pre-populated into
  // the local cache. If `true` (default), queries for these facets will succeed
  // immediately (cache hit). If `false`, queries will return `success=false`
  // (cache miss) until `UpdateAffiliationsAndBranding` is called for them.
  void AddGroupedFacets(const GroupedFacets& group, bool add_to_cache = true);

  // Seeds the list of `psl_extensions` to return during queries.
  void SetPSLExtensions(std::vector<std::string> psl_extensions);

  // AffiliationService:
  void FetchChangePasswordURL(const GURL& url,
                              base::OnceCallback<void(GURL)> callback) override;
  GURL GetChangePasswordURL(const GURL& url) const override;
  void GetAffiliationsAndBranding(
      const FacetURI& facet_uri,
      ResultCallback result_callback) override;
  void Prefetch(const FacetURI& facet_uri,
                const base::Time& keep_fresh_until) override;
  void CancelPrefetch(const FacetURI& facet_uri,
                      const base::Time& keep_fresh_until) override;
  void KeepPrefetchForFacets(std::vector<FacetURI> facet_uris) override;
  void TrimUnusedCache(std::vector<FacetURI> facet_uris) override;
  void GetGroupingInfo(std::vector<FacetURI> facet_uris,
                       GroupsCallback callback) override;
  void GetPSLExtensions(
      base::OnceCallback<void(std::vector<std::string>)> callback) override;
  void UpdateAffiliationsAndBranding(const std::vector<FacetURI>& facets,
                                     base::OnceClosure callback) override;
  void RegisterSource(std::unique_ptr<AffiliationSource> source) override;
  base::WeakPtr<AffiliationService> AsWeakPtr() override;

 private:
  const scoped_refptr<base::SequencedTaskRunner>& GetTaskRunner() const;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Ground-truth affiliation groups in the simulated backend database.
  std::vector<AffiliatedFacets> affiliation_groups_;

  // Ground-truth grouping info in the simulated backend database.
  std::vector<GroupedFacets> grouped_facets_;

  // Seeded list of PSL extensions.
  std::vector<std::string> psl_extensions_;

  // Simulated local profile cache of cached facet URIs.
  absl::flat_hash_set<FacetURI, FacetURIHash> cache_;

  base::WeakPtrFactory<FakeAffiliationService> weak_ptr_factory_{this};
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_FAKE_AFFILIATION_SERVICE_H_
