// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_ALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_ALLOCATOR_H_

#include "components/plus_addresses/plus_address_allocator.h"
#include "components/plus_addresses/plus_address_test_utils.h"

namespace plus_addresses {

class FakePlusAddressAllocator : public PlusAddressAllocator {
 public:
  FakePlusAddressAllocator();
  FakePlusAddressAllocator(const FakePlusAddressAllocator&) = delete;
  FakePlusAddressAllocator(FakePlusAddressAllocator&&) = delete;
  FakePlusAddressAllocator& operator=(const FakePlusAddressAllocator&) = delete;
  FakePlusAddressAllocator& operator=(FakePlusAddressAllocator&&) = delete;
  ~FakePlusAddressAllocator() override;

  void set_is_next_allocation_synchronous(bool is_next_allocation_synchronous) {
    is_next_allocation_synchronous_ = is_next_allocation_synchronous;
  }

  void set_profile_or_error(PlusProfileOrError profile_or_error) {
    profile_or_error_ = std::move(profile_or_error);
  }

  // PlusAddressAllocator:
  void AllocatePlusAddress(const url::Origin& origin,
                           AllocationMode mode,
                           PlusAddressRequestCallback callback) override;
  std::optional<PlusProfile> AllocatePlusAddressSynchronously(
      const url::Origin& origin,
      AllocationMode mode) override;
  bool IsRefreshingSupported(const url::Origin& origin) const override;
  void RemoveAllocatedPlusAddress(const PlusAddress& plus_address) override;

 private:
  bool is_next_allocation_synchronous_ = false;
  PlusProfileOrError profile_or_error_ = test::CreatePlusProfile();
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_FAKE_PLUS_ADDRESS_ALLOCATOR_H_
