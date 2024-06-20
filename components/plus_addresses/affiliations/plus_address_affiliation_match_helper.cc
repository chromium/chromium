// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
}  // namespace

PlusAddressAffiliationMatchHelper::PlusAddressAffiliationMatchHelper(
    PlusAddressService* plus_address_service,
    affiliations::AffiliationService* affiliation_service)
    : plus_address_service_(CHECK_DEREF(plus_address_service)),
      affiliation_service_(CHECK_DEREF(affiliation_service)) {}

PlusAddressAffiliationMatchHelper::~PlusAddressAffiliationMatchHelper() =
    default;

void PlusAddressAffiliationMatchHelper::GetAffiliatedPlusProfiles(
    const PlusProfile::facet_t& facet,
    AffiliatedPlusProfilesCallback result_callback) {
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAffiliations)) {
    std::vector<PlusProfile> results;
    if (std::optional<PlusProfile> profile =
            plus_address_service_->GetPlusProfile(facet)) {
      results.push_back(std::move(*profile));
    }
    std::move(result_callback).Run(std::move(results));
    return;
  }

  FacetURI facet_uri = absl::get<FacetURI>(facet);
  DCHECK(facet_uri.IsValidWebFacetURI());
  // The barrier is used to collect affiliated plus addresses from multiple
  // sources (i.e. grouped affiliations, PSL matches), combine and return them.
  const int kCallsNumber = 2;
  auto barrier_callback = base::BarrierCallback<std::vector<PlusProfile>>(
      kCallsNumber,
      base::BindOnce(&PlusAddressAffiliationMatchHelper::MergeResults,
                     weak_factory_.GetWeakPtr(), std::move(result_callback)));

  GetPSLExtensions(base::BindOnce(
      &PlusAddressAffiliationMatchHelper::ProcessExactAndPSLMatches,
      weak_factory_.GetWeakPtr(), barrier_callback, facet_uri));

  affiliation_service_->GetGroupingInfo(
      {facet_uri},
      base::BindOnce(&PlusAddressAffiliationMatchHelper::OnGroupingInfoReceived,
                     weak_factory_.GetWeakPtr(), barrier_callback));
}

void PlusAddressAffiliationMatchHelper::GetPSLExtensions(
    PSLExtensionCallback callback) {
  if (psl_extensions_.has_value()) {
    std::move(callback).Run(psl_extensions_.value());
    return;
  }

  psl_extensions_callbacks_.push_back(std::move(callback));
  if (psl_extensions_callbacks_.size() > 1) {
    // If there is more than one request in the queue, wait until the
    // OnPSLExtensionsReceived is triggered.
    return;
  }

  affiliation_service_->GetPSLExtensions(base::BindOnce(
      &PlusAddressAffiliationMatchHelper::OnPSLExtensionsReceived,
      weak_factory_.GetWeakPtr()));
}

void PlusAddressAffiliationMatchHelper::OnPSLExtensionsReceived(
    std::vector<std::string> psl_extensions) {
  psl_extensions_ = base::flat_set<std::string>(
      std::make_move_iterator(psl_extensions.begin()),
      std::make_move_iterator(psl_extensions.end()));

  for (auto& callback : std::exchange(psl_extensions_callbacks_, {})) {
    std::move(callback).Run(psl_extensions_.value());
  }
}

void PlusAddressAffiliationMatchHelper::ProcessExactAndPSLMatches(
    base::RepeatingCallback<void(std::vector<PlusProfile>)>
        matches_received_callback,
    const FacetURI& facet,
    const base::flat_set<std::string>& psl_extensions) {
  std::vector<PlusProfile> matches;
  for (const PlusProfile& stored_profile :
       plus_address_service_->GetPlusProfiles()) {
    FacetURI stored_profile_facet = absl::get<FacetURI>(stored_profile.facet);
    // Note that exact matches are also PSL matches.
    if (affiliations::IsExtendedPublicSuffixDomainMatch(
            GURL(stored_profile_facet.canonical_spec()),
            GURL(facet.canonical_spec()), psl_extensions)) {
      matches.push_back(std::move(stored_profile));
    }
  }
  std::move(matches_received_callback).Run(std::move(matches));
}

void PlusAddressAffiliationMatchHelper::OnGroupingInfoReceived(
    base::RepeatingCallback<void(std::vector<PlusProfile>)>
        matches_received_callback,
    const std::vector<affiliations::GroupedFacets>& results) {
  // GetGroupingInfo() returns a an affiliation group for each facet. Asking for
  // only one facet means that it must return only one group that includes
  // requested facet itself.
  CHECK_EQ(1U, results.size());

  std::vector<PlusProfile> matches;
  for (const affiliations::Facet& facet : results[0].facets) {
    std::optional<PlusProfile> profile =
        plus_address_service_->GetPlusProfile(facet.uri);
    if (profile) {
      matches.push_back(std::move(*profile));
    }
  }
  std::move(matches_received_callback).Run(std::move(matches));
}

void PlusAddressAffiliationMatchHelper::MergeResults(
    AffiliatedPlusProfilesCallback result_callback,
    std::vector<std::vector<PlusProfile>> results) {
  std::vector<PlusProfile> response;
  for (std::vector<PlusProfile>& profiles : results) {
    response.insert(response.end(), std::make_move_iterator(profiles.begin()),
                    std::make_move_iterator(profiles.end()));
  }
  // Remove duplicates.
  std::sort(response.begin(), response.end(), PlusProfileFacetComparator());
  response.erase(std::unique(response.begin(), response.end()), response.end());

  std::move(result_callback).Run(std::move(response));
}

}  // namespace plus_addresses
