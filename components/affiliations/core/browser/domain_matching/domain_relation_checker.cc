// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/domain_matching/domain_relation_checker.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace affiliations {

namespace {

std::optional<MatchType> CombineMatches(
    std::vector<std::optional<MatchType>> results) {
  std::optional<MatchType> combined_match;
  for (const auto& match : results) {
    if (match.has_value()) {
      combined_match |= *match;
    }
  }
  return combined_match;
}

}  // namespace

DomainRelationChecker::DomainRelationChecker(
    AffiliationService& affiliation_service)
    : affiliation_service_(affiliation_service) {}

DomainRelationChecker::~DomainRelationChecker() = default;

void DomainRelationChecker::Check(
    const url::Origin& origin_1,
    const url::Origin& origin_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  if (origin_1.IsSameOriginWith(origin_2)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_cb), MatchType::kExact));
    return;
  }
  if (origin_1.opaque() || origin_2.opaque()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_cb), std::nullopt));
    return;
  }
  CheckAffiliations(origin_1.GetTupleOrPrecursorTupleIfOpaque(),
                    origin_2.GetTupleOrPrecursorTupleIfOpaque(),
                    std::move(result_cb));
}

void DomainRelationChecker::Check(
    const url::SchemeHostPort& tuple_1,
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  if (tuple_1 == tuple_2) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(result_cb), MatchType::kExact));
    return;
  }
  CheckAffiliations(tuple_1, tuple_2, std::move(result_cb));
}

void DomainRelationChecker::CheckAffiliations(
    const url::SchemeHostPort& tuple_1,
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  affiliation_service_->GetAffiliationsAndBranding(
      FacetURI::FromPotentiallyInvalidSpec(tuple_1.Serialize()),
      base::BindOnce(&DomainRelationChecker::OnAffiliationsAvailabilityChecked,
                     weak_ptr_factory_.GetWeakPtr(), tuple_1, tuple_2,
                     std::move(result_cb)));
}

void DomainRelationChecker::OnAffiliationsAvailabilityChecked(
    const url::SchemeHostPort& tuple_1,
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb,
    const AffiliatedFacets& origin_1_affiliations,
    bool success) {
  if (success) {
    OnCacheUpdated(tuple_1, tuple_2, origin_1_affiliations,
                   std::move(result_cb));
    return;
  }

  std::vector<FacetURI> facets = {
      FacetURI::FromPotentiallyInvalidSpec(tuple_1.Serialize()),
      FacetURI::FromPotentiallyInvalidSpec(tuple_2.Serialize())};

  affiliation_service_->UpdateAffiliationsAndBranding(
      facets, base::BindOnce(&DomainRelationChecker::OnCacheUpdated,
                             weak_ptr_factory_.GetWeakPtr(), tuple_1, tuple_2,
                             /*origin_1_affiliations=*/std::nullopt,
                             std::move(result_cb)));
}

void DomainRelationChecker::OnCacheUpdated(
    const url::SchemeHostPort& tuple_1,
    const url::SchemeHostPort& tuple_2,
    std::optional<AffiliatedFacets> origin_1_affiliations,
    base::OnceCallback<void(std::optional<MatchType>)> result_cb) {
  auto barrier_callback = base::BarrierCallback<std::optional<MatchType>>(
      3, base::BindOnce(&CombineMatches).Then(std::move(result_cb)));

  if (origin_1_affiliations.has_value()) {
    OnAffiliationsRetrieved(tuple_2, barrier_callback, *origin_1_affiliations,
                            /*success=*/true);
  } else {
    affiliation_service_->GetAffiliationsAndBranding(
        FacetURI::FromPotentiallyInvalidSpec(tuple_1.Serialize()),
        base::BindOnce(&DomainRelationChecker::OnAffiliationsRetrieved,
                       weak_ptr_factory_.GetWeakPtr(), tuple_2,
                       barrier_callback));
  }

  affiliation_service_->GetPSLExtensions(base::BindOnce(
      &DomainRelationChecker::OnPSLExtensionsRetrieved,
      weak_ptr_factory_.GetWeakPtr(), tuple_1, tuple_2, barrier_callback));

  affiliation_service_->GetGroupingInfo(
      {FacetURI::FromPotentiallyInvalidSpec(tuple_1.Serialize())},
      base::BindOnce(&DomainRelationChecker::OnGroupingInfoRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), tuple_2,
                     barrier_callback));
}

void DomainRelationChecker::OnAffiliationsRetrieved(
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> callback,
    const AffiliatedFacets& origin_1_affiliations,
    bool success) {
  if (!success) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  FacetURI target_uri =
      FacetURI::FromPotentiallyInvalidSpec(tuple_2.Serialize());
  bool is_affiliated = std::ranges::any_of(
      origin_1_affiliations,
      [&](const Facet& facet) { return facet.uri == target_uri; });
  std::move(callback).Run(is_affiliated
                              ? std::make_optional(MatchType::kAffiliated)
                              : std::nullopt);
}

void DomainRelationChecker::OnPSLExtensionsRetrieved(
    const url::SchemeHostPort& tuple_1,
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> callback,
    std::vector<std::string> psl_extensions) {
  base::flat_set<std::string> psl_extensions_set(std::move(psl_extensions));
  bool psl_match = IsExtendedPublicSuffixDomainMatch(
      tuple_1.GetURL(), tuple_2.GetURL(), psl_extensions_set);
  std::move(callback).Run(psl_match ? std::make_optional(MatchType::kPSL)
                                    : std::nullopt);
}

void DomainRelationChecker::OnGroupingInfoRetrieved(
    const url::SchemeHostPort& tuple_2,
    base::OnceCallback<void(std::optional<MatchType>)> callback,
    const std::vector<GroupedFacets>& origin_1_groups) {
  // Since grouping info was requested for exactly one facet, the service must
  // return exactly one group.
  CHECK_EQ(1u, origin_1_groups.size());
  FacetURI target_uri =
      FacetURI::FromPotentiallyInvalidSpec(tuple_2.Serialize());
  bool is_grouped = std::ranges::any_of(
      origin_1_groups[0].facets,
      [&](const Facet& facet) { return facet.uri == target_uri; });

  std::move(callback).Run(is_grouped ? std::make_optional(MatchType::kGrouped)
                                     : std::nullopt);
}

}  // namespace affiliations
