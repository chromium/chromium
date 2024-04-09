// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"

#include "base/strings/strcat.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
}

PlusAddressAffiliationSourceAdapter::PlusAddressAffiliationSourceAdapter(
    PlusAddressService* service)
    : service_(*service) {}

PlusAddressAffiliationSourceAdapter::~PlusAddressAffiliationSourceAdapter() =
    default;

void PlusAddressAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  std::vector<PlusProfile> profiles = service_->GetPlusProfiles();
  std::vector<FacetURI> facets;
  facets.reserve(profiles.size());
  for (const PlusProfile& profile : profiles) {
    // TODO(b/324553908): Filter only valid facets once the service starts
    // working with full domains.
    facets.push_back(FacetURI::FromPotentiallyInvalidSpec(
        base::StrCat({"https://", profile.facet})));
  }

  std::move(response_callback).Run(std::move(facets));
}

void PlusAddressAffiliationSourceAdapter::StartObserving(
    AffiliationSource::Observer* observer) {
  CHECK(!observer_);
  observer_ = observer;
  // TODO(b/324553908): Observe plus addresses changes.
}

}  // namespace plus_addresses
