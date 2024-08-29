// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
constexpr char kUmaKeyResponseTime[] =
    "PlusAddresses.AffiliationRequest.ResponseTime";
}  // namespace

PlusAddressAffiliationMatchHelper::PlusAddressAffiliationMatchHelper(
    PlusAddressService* plus_address_service,
    affiliations::AffiliationService* affiliation_service)
    : plus_address_service_(CHECK_DEREF(plus_address_service)),
      affiliation_service_(CHECK_DEREF(affiliation_service)) {}

PlusAddressAffiliationMatchHelper::~PlusAddressAffiliationMatchHelper() =
    default;

void PlusAddressAffiliationMatchHelper::GetAffiliatedPlusProfiles(
    const affiliations::FacetURI& facet,
    AffiliatedPlusProfilesCallback result_callback) {
  PSLExtensionCallback callback =
      base::BindOnce(&PlusAddressAffiliationMatchHelper::RequestGroupInfo,
                     weak_factory_.GetWeakPtr(), std::move(result_callback),
                     facet, base::TimeTicks::Now());

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

void PlusAddressAffiliationMatchHelper::RequestGroupInfo(
    AffiliatedPlusProfilesCallback result_callback,
    const FacetURI& facet,
    base::TimeTicks start_time,
    const base::flat_set<std::string>& psl_extensions) {
  GURL url = GURL(facet.potentially_invalid_spec());
  std::string domain =
      affiliations::GetExtendedTopLevelDomain(url, psl_extensions);

  // See the `net::registry_controlled_domains::GetDomainAndRegistry`
  // documentation for cases where the extended domain can be empty.
  if (domain.empty()) {
    std::move(result_callback).Run({});
    return;
  }

  // Here the requested facet is replaced by the top level domain while honoring
  // the psl extension list. This is done in an effort to capture subdomains
  // that might have not included yet in the affiliation data. Notably, this
  // assumes that top level domains are included in the same affiliation group
  // as subdomains.
  GURL::Replacements repl;
  repl.SetHostStr(domain);
  url.ReplaceComponents(repl);
  FacetURI requested_facet =
      FacetURI::FromPotentiallyInvalidSpec(url.ReplaceComponents(repl).spec());
  affiliation_service_->GetGroupingInfo(
      {requested_facet},
      base::BindOnce(&PlusAddressAffiliationMatchHelper::OnGroupingInfoReceived,
                     weak_factory_.GetWeakPtr(), std::move(result_callback),
                     start_time, psl_extensions));
}

void PlusAddressAffiliationMatchHelper::OnGroupingInfoReceived(
    AffiliatedPlusProfilesCallback result_callback,
    base::TimeTicks start_time,
    const base::flat_set<std::string>& psl_extensions,
    const std::vector<affiliations::GroupedFacets>& results) {
  // GetGroupingInfo() returns an affiliation group for each facet. Asking for
  // only one facet means that it must return only one group that **always**
  // includes requested facet itself.
  CHECK_EQ(1U, results.size());

  std::vector<PlusProfile> matches;
  for (const affiliations::Facet& group_facet : results[0].facets) {
    // Android facets
    if (group_facet.uri.IsValidAndroidFacetURI()) {
      if (std::optional<PlusProfile> profile =
              plus_address_service_->GetPlusProfile(group_facet.uri)) {
        matches.push_back(std::move(*profile));
      }
      continue;
    }

    // Web facets
    for (const PlusProfile& stored_profile :
         plus_address_service_->GetPlusProfiles()) {
      if (affiliations::IsExtendedPublicSuffixDomainMatch(
              GURL(stored_profile.facet.potentially_invalid_spec()),
              GURL(group_facet.uri.potentially_invalid_spec()),
              psl_extensions)) {
        matches.push_back(std::move(stored_profile));
      }
    }
  }

  // Remove duplicates.
  std::sort(matches.begin(), matches.end(), PlusProfileFacetComparator());
  matches.erase(std::unique(matches.begin(), matches.end()), matches.end());

  base::UmaHistogramTimes(kUmaKeyResponseTime,
                          base::TimeTicks::Now() - start_time);

  std::move(result_callback).Run(std::move(matches));
}

}  // namespace plus_addresses
