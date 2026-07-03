// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_
#define COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/affiliations/core/browser/match_type.h"
#include "url/origin.h"

namespace affiliations {

class AffiliationService;

// Convenience class that uses `AffiliationService` to match origins.
class DomainRelationChecker {
 public:
  explicit DomainRelationChecker(AffiliationService& affiliation_service);
  ~DomainRelationChecker();

  DomainRelationChecker(const DomainRelationChecker&) = delete;
  DomainRelationChecker& operator=(const DomainRelationChecker&) = delete;

  // Asynchronously checks the relationship between `origin_1` and `origin_2`.
  //
  // Note: This method will issue live requests to `AffiliationService`, meaning
  // the user should be mindful about QPS increase and do their own
  // calculations.
  //
  // The `result_cb` will be run with the check result or `nullopt` if there is
  // no relation.
  void Check(const url::Origin& origin_1,
             const url::Origin& origin_2,
             base::OnceCallback<void(std::optional<MatchType>)> result_cb);

 private:
  // Callback run when the affiliations cache has been updated for both origins.
  void OnCacheUpdated(
      const url::Origin& origin_1,
      const url::Origin& origin_2,
      std::optional<AffiliatedFacets> origin_1_affiliations,
      base::OnceCallback<void(std::optional<MatchType>)> result_cb);

  // Callback run after checking if affiliations are available in the cache.
  void OnAffiliationsAvailabilityChecked(
      const url::Origin& origin_1,
      const url::Origin& origin_2,
      base::OnceCallback<void(std::optional<MatchType>)> result_cb,
      const AffiliatedFacets& origin_1_affiliations,
      bool success);

  // Callback run when the affiliation service returns the list of affiliated
  // facets.
  void OnAffiliationsRetrieved(
      const url::Origin& origin_2,
      base::OnceCallback<void(std::optional<MatchType>)> callback,
      const AffiliatedFacets& origin_1_affiliations,
      bool success);

  // Callback run when the affiliation service returns the PSL extension list.
  void OnPSLExtensionsRetrieved(
      const url::Origin& origin_1,
      const url::Origin& origin_2,
      base::OnceCallback<void(std::optional<MatchType>)> callback,
      std::vector<std::string> psl_extensions);

  // Callback run when the affiliation service returns the grouping info.
  void OnGroupingInfoRetrieved(
      const url::Origin& origin_2,
      base::OnceCallback<void(std::optional<MatchType>)> callback,
      const std::vector<GroupedFacets>& origin_1_groups);

  // The `AffiliationService` used to retrieve affiliations and PSL extensions.
  // It is owned by the `Profile` (as a `KeyedService`). The caller must ensure
  // that `affiliation_service_` outlives this instance.
  const raw_ref<AffiliationService> affiliation_service_;

  base::WeakPtrFactory<DomainRelationChecker> weak_ptr_factory_{this};
};

}  // namespace affiliations

#endif  // COMPONENTS_AFFILIATIONS_CORE_BROWSER_DOMAIN_MATCHING_DOMAIN_RELATION_CHECKER_H_
