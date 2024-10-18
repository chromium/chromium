// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/fake_plus_address_allocator.h"

namespace plus_addresses {

FakePlusAddressAllocator::FakePlusAddressAllocator() = default;

FakePlusAddressAllocator::~FakePlusAddressAllocator() = default;

void FakePlusAddressAllocator::AllocatePlusAddress(
    const url::Origin& origin,
    AllocationMode mode,
    PlusAddressRequestCallback callback) {
  std::move(callback).Run(profile_or_error_);
}

std::optional<PlusProfile>
FakePlusAddressAllocator::AllocatePlusAddressSynchronously(
    const url::Origin& origin,
    AllocationMode mode) {
  if (!is_next_allocation_synchronous_ || !profile_or_error_.has_value()) {
    return std::nullopt;
  }
  return profile_or_error_.value();
}

bool FakePlusAddressAllocator::IsRefreshingSupported(
    const url::Origin& origin) const {
  return true;
}

void FakePlusAddressAllocator::RemoveAllocatedPlusAddress(
    const PlusAddress& plus_address) {}

}  // namespace plus_addresses
