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
      const affiliations::FacetURI& facet,
      AffiliatedPlusProfilesCallback result_callback);

 private:
  void OnPSLExtensionsReceived(std::vector<std::string> psl_extensions);

  // Queries the affiliation service for group affiliated facets of the
  // requested `facet`. Notably, the response always contains the requested
  // facet, even when no entries were found on the local cache.
  void RequestGroupInfo(AffiliatedPlusProfilesCallback result_callback,
                        const affiliations::FacetURI& facet,
                        base::TimeTicks start_time,
                        const base::flat_set<std::string>& psl_extensions);

  void OnGroupingInfoReceived(
      AffiliatedPlusProfilesCallback result_callback,
      base::TimeTicks start_time,
      const base::flat_set<std::string>& psl_extensions,
      const std::vector<affiliations::GroupedFacets>& results);

  std::optional<base::flat_set<std::string>> psl_extensions_;
  std::vector<PSLExtensionCallback> psl_extensions_callbacks_;

  const raw_ref<PlusAddressService> plus_address_service_;
  const raw_ref<affiliations::AffiliationService> affiliation_service_;

  base::WeakPtrFactory<PlusAddressAffiliationMatchHelper> weak_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_MATCH_HELPER_H_
