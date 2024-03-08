// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_JIT_ALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_JIT_ALLOCATOR_H_

#include "components/plus_addresses/plus_address_allocator.h"

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "url/origin.h"

namespace plus_addresses {

class PlusAddressHttpClient;
class PlusAddressService;

class PlusAddressJitAllocator : public PlusAddressAllocator {
 public:
  explicit PlusAddressJitAllocator(PlusAddressHttpClient* http_client);
  ~PlusAddressJitAllocator() override;

  // PlusAddressAllocator:
  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override;
  bool IsRefreshingSupported(const url::Origin& origin) const override;

 private:
  // Responsible for server communication. Owned by the `PlusAddressService` and
  // outlives `this`.
  const raw_ref<PlusAddressHttpClient> http_client_;

  // Counts how many refresh attempts where made for an `Origin`. Serves to
  // limit the number of refresh requests per session.
  base::flat_map<url::Origin, int> refresh_attempts_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_JIT_ALLOCATOR_H_
