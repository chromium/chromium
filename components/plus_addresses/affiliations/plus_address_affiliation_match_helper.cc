// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_match_helper.h"

#include "components/affiliations/core/browser/affiliation_service.h"
#include "components/plus_addresses/features.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {

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

  // TODO(b/324553908): Complete.
}

}  // namespace plus_addresses
