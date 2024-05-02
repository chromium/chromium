// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_
#define COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/plus_addresses/plus_address_types.h"

namespace affiliations {
class AffiliationService;
}

namespace plus_addresses {

class PlusAddressService;

// Interacts with the AffiliationService on behalf of the PlusAddressService.
// For each `GetAffiliatedPlusProfiles()` incoming request, it supplies the
// PlusAddressService with a list of plus profiles that are relevant matches for
// the originally requested plus profile based on the affiliation of their
// facets.
class PlusAddressAffiliationMatchHelper {
 public:
  // Callback to return the list of affiliated plus profiles.
  using AffiliatedPlusProfilesCallback =
      base::OnceCallback<void(std::vector<PlusProfile>)>;

  // The `plus_address_service` and `affiliation_service` must outlive `this`.
  // Both arguments must be non-NULL.
  PlusAddressAffiliationMatchHelper(
      PlusAddressService* plus_address_service,
      affiliations::AffiliationService* affiliation_service);
  PlusAddressAffiliationMatchHelper(const PlusAddressAffiliationMatchHelper&) =
      delete;
  PlusAddressAffiliationMatchHelper& operator=(
      const PlusAddressAffiliationMatchHelper&) = delete;
  virtual ~PlusAddressAffiliationMatchHelper();

  // Returns the complete list of plus profiles, including the specified
  // `plus_profile`, that belong to the same affiliation group based on their
  // facet value.
  void GetAffiliatedPlusProfiles(
      const PlusProfile& plus_profile,
      AffiliatedPlusProfilesCallback result_callback);

 private:
  const raw_ref<PlusAddressService> plus_address_service_;
  const raw_ref<affiliations::AffiliationService> affiliation_service_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_
