// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include <string>
#include <vector>

#include "base/barrier_callback.h"
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
    : plus_address_service_(*plus_address_service),
      affiliation_service_(*affiliation_service) {}

PlusAddressAffiliationMatchHelper::~PlusAddressAffiliationMatchHelper() =
    default;

void PlusAddressAffiliationMatchHelper::GetAffiliatedPlusProfiles(
    const PlusProfile& plus_profile,
    AffiliatedPlusProfilesCallback result_callback) {
  if (!base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressAffiliations)) {
    std::move(result_callback).Run({plus_profile});
    return;
  }

  FacetURI facet = absl::get<FacetURI>(plus_profile.facet);
  // TODO(b/324553908): Include other sources (e.g. credential sharing, grouped
  // affiliations).
  const int kCallsNumber = 1;
  auto barrier_callback = base::BarrierCallback<std::vector<PlusProfile>>(
      kCallsNumber,
      base::BindOnce(&PlusAddressAffiliationMatchHelper::MergeResults,
                     weak_factory_.GetWeakPtr(), std::move(result_callback)));

  GetPSLExtensions(base::BindOnce(
      &PlusAddressAffiliationMatchHelper::ProcessExactAndPSLMatches,
      weak_factory_.GetWeakPtr(), barrier_callback, facet));
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
  // TODO(b/324553908): Complete.
  std::move(matches_received_callback).Run({});
}

void PlusAddressAffiliationMatchHelper::MergeResults(
    AffiliatedPlusProfilesCallback result_callback,
    std::vector<std::vector<PlusProfile>> results) {
  // TODO(b/324553908): Complete.
  std::move(result_callback).Run({});
}

}  // namespace plus_addresses
