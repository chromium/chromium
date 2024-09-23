// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_ALLOCATOR_H_
#define COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_ALLOCATOR_H_

#include <optional>

#include "components/plus_addresses/plus_address_types.h"

namespace url {
class Origin;
}  // namespace url

namespace plus_addresses {

// An interface for plus address allocation. Implementers may pursue different
// strategies for plus address allocation, e.g. allocation on the fly or
// pre-allocation.
class PlusAddressAllocator {
 public:
  virtual ~PlusAddressAllocator() = default;

  // The maximum number of times that a user can choose to refresh the suggested
  // plus address for a domain. The limit need not be persisted to disk and,
  // thus, may only be enforced for the lifetime of the browser.
  static constexpr int kMaxPlusAddressRefreshesPerOrigin = 10;

  enum class AllocationMode {
    // The requested plus address can be any (unused) plus address, regardless
    // of whether it has been shown to the user before.
    kAny = 0,
    // The requested plus address should be one that the user has never seen
    // before.
    kNewPlusAddress = 1
  };

  // Attempts to allocate a plus address for `origin`.
  virtual void AllocatePlusAddress(const url::Origin& origin,
                                   AllocationMode mode,
                                   PlusAddressRequestCallback callback) = 0;

  // Attempts to allocate a plus address for `origin` synchronously. If none is
  // available synchronously, it returns `std::nullopt` and does no further
  // work.
  virtual std::optional<PlusProfile> AllocatePlusAddressSynchronously(
      const url::Origin& origin,
      AllocationMode mode) = 0;

  // Returns whether a plus address for `origin` may be refreshed.
  virtual bool IsRefreshingSupported(const url::Origin& origin) const = 0;

  // Removes `plus_address` from the allocation pool. Depending on the
  // implementation, this may be a no-op.
  virtual void RemoveAllocatedPlusAddress(const PlusAddress& plus_address) = 0;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_PLUS_ADDRESS_ALLOCATOR_H_
