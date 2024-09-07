// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/affiliations/plus_address_affiliation_source_adapter.h"

#include "base/containers/span.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
namespace {
using affiliations::FacetURI;
}  // namespace

PlusAddressAffiliationSourceAdapter::PlusAddressAffiliationSourceAdapter(
    PlusAddressService* service)
    : service_(service) {
  // Immediate observation of the plus address service is essential to react to
  // service destruction. This prevents dangling pointers by automatically
  // removing observation and resetting the model pointer.
  service_observation_.Observe(service_.get());
}

PlusAddressAffiliationSourceAdapter::~PlusAddressAffiliationSourceAdapter() =
    default;

void PlusAddressAffiliationSourceAdapter::GetFacets(
    AffiliationSource::ResultCallback response_callback) {
  if (!service_) {
    std::move(response_callback).Run({});
    return;
  }
  base::span<const PlusProfile> profiles = service_->GetPlusProfiles();
  std::vector<FacetURI> facets;
  facets.reserve(profiles.size());
  for (const PlusProfile& profile : profiles) {
    facets.push_back(profile.facet);
  }
  std::move(response_callback).Run(std::move(facets));
}

void PlusAddressAffiliationSourceAdapter::StartObserving(
    AffiliationSource::Observer* observer) {
  CHECK(!observer_);
  observer_ = observer;
}

void PlusAddressAffiliationSourceAdapter::OnPlusAddressesChanged(
    const std::vector<PlusAddressDataChange>& changes) {
  if (!observer_) {
    return;
  }
  std::vector<FacetURI> added_facets;
  std::vector<FacetURI> removed_facets;
  for (const PlusAddressDataChange& change : changes) {
    FacetURI facet = change.profile().facet;
    switch (change.type()) {
      case PlusAddressDataChange::Type::kAdd: {
        added_facets.push_back(std::move(facet));
        break;
      }
      case PlusAddressDataChange::Type::kRemove: {
        removed_facets.push_back(std::move(facet));
        break;
      }
    }
  }

  // When a plus address is updated, `changes` will contain both a kRemove and
  // kAdd change for that facet. Cached affiliation data should not be deleted
  // in this case. A simple solution is to call `added` events always before
  // `removed` -- the trimming logic will detect that there is an active
  // prefetch and not delete the corresponding data.
  if (!added_facets.empty()) {
    observer_->OnFacetsAdded(added_facets);
  }

  if (!removed_facets.empty()) {
    observer_->OnFacetsRemoved(removed_facets);
  }
}

void PlusAddressAffiliationSourceAdapter::OnPlusAddressServiceShutdown() {
  service_observation_.Reset();
  service_ = nullptr;
}

}  // namespace plus_addresses
