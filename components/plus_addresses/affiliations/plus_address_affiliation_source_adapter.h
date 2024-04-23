// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_
#define COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/affiliations/core/browser/affiliation_source.h"
#include "components/plus_addresses/plus_address_service.h"

namespace plus_addresses {

// This class represents a source for plus addresses related data requiring
// affiliation updates. It utilizes PlusAddressService's information and
// monitors changes to notify observers.
class PlusAddressAffiliationSourceAdapter
    : public affiliations::AffiliationSource,
      public PlusAddressService::Observer {
 public:
  explicit PlusAddressAffiliationSourceAdapter(PlusAddressService* service);
  ~PlusAddressAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving(AffiliationSource::Observer* observer) override;

  // PlusAddressService::Observer:
  void OnPlusAddressesChanged(
      const std::vector<PlusAddressDataChange>& changes) override;

 private:
  // The observer (i.e. the AffiliationsService) owns and outlives the adapter.
  raw_ptr<AffiliationSource::Observer> observer_ = nullptr;

  const raw_ptr<PlusAddressService> service_;
  base::ScopedObservation<PlusAddressService, PlusAddressService::Observer>
      service_observation_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_
