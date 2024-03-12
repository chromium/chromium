// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_
#define COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/affiliations/core/browser/affiliation_source.h"

namespace plus_addresses {

class PlusAddressService;

// This class represents a source for plus addresses related data requiring
// affiliation updates. It utilizes PlusAddressService's information and
// monitors changes to notify observers.
class PlusAddressAffiliationSourceAdapter
    : public affiliations::AffiliationSource {
 public:
  PlusAddressAffiliationSourceAdapter(PlusAddressService* service,
                                      AffiliationSource::Observer* observer);
  ~PlusAddressAffiliationSourceAdapter() override;

  // AffiliationSource:
  void GetFacets(AffiliationSource::ResultCallback response_callback) override;
  void StartObserving() override;

 private:
  const raw_ref<PlusAddressService> service_;
  const raw_ref<AffiliationSource::Observer> observer_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_AFFILIATIONS_PLUS_ADDRESS_AFFILIATION_SOURCE_ADAPTER_H_
