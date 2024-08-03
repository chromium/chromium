// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_types.h"
#include "url/origin.h"

class PrefService;

namespace url {
class Origin;
}  // namespace url

namespace plus_addresses {

class PlusAddressHttpClient;
class PlusAddressService;

class PlusAddressPreallocator : public PlusAddressAllocator {
 public:
  PlusAddressPreallocator(PrefService* pref_service,
                          PlusAddressHttpClient* http_client);
  ~PlusAddressPreallocator() override;

  // PlusAddressAllocator:
  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override;
  bool IsRefreshingSupported(const url::Origin& origin) const override;

 private:
  // Deletes pre-allocated plus addresses that have reached their EOL and
  // updates the index of the next plus preallocated plus address.
  void PrunePreallocatedPlusAddresses();

  // Owned by the `Profile` that (indirectly) owns `this`.
  const raw_ref<PrefService> pref_service_;

  // Responsible for server communication. Owned by the `PlusAddressService` and
  // outlives `this`.
  const raw_ref<PlusAddressHttpClient> http_client_;

  base::WeakPtrFactory<PlusAddressPreallocator> weak_ptr_factory_{this};
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_PREALLOCATOR_H_
