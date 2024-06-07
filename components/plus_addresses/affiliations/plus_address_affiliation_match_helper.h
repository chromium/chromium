// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_
#define COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "components/plus_addresses/plus_address_types.h"

namespace affiliations {
class AffiliationService;
class FacetURI;
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
  // Callback to return that set of PSL extensions.
  using PSLExtensionCallback =
      base::OnceCallback<void(const base::flat_set<std::string>&)>;

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

  // Returns the complete list of plus profiles that are affiliated with `facet`
  // based on their facet value. Only valid web facets must be passed-in.
  void GetAffiliatedPlusProfiles(
      const PlusProfile::facet_t& facet,
      AffiliatedPlusProfilesCallback result_callback);

  // Requests and caches the list of PSL extensions.
  void GetPSLExtensions(PSLExtensionCallback callback);

 private:
  void OnPSLExtensionsReceived(std::vector<std::string> psl_extensions);

  // Queries the plus address cache for entries with facets that satisfy either
  // exact or PSL matching against the provided `facet`.
  void ProcessExactAndPSLMatches(
      base::RepeatingCallback<void(std::vector<PlusProfile>)>
          matches_received_callback,
      const affiliations::FacetURI& facet,
      const base::flat_set<std::string>& psl_extensions);

  void OnGroupingInfoReceived(
      base::RepeatingCallback<void(std::vector<PlusProfile>)>
          matches_received_callback,
      const std::vector<affiliations::GroupedFacets>& results);

  // Merges results from various affiliation types, prioritizes them, eliminates
  // duplicates, and delivers a ranked list of affiliated plus profiles.
  void MergeResults(AffiliatedPlusProfilesCallback result_callback,
                    std::vector<std::vector<PlusProfile>> results);

  std::optional<base::flat_set<std::string>> psl_extensions_;
  std::vector<PSLExtensionCallback> psl_extensions_callbacks_;

  const raw_ref<PlusAddressService> plus_address_service_;
  const raw_ref<affiliations::AffiliationService> affiliation_service_;

  base::WeakPtrFactory<PlusAddressAffiliationMatchHelper> weak_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_
