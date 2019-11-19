// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/allocation.h"

#include "base/hash/hash.h"

namespace heap_profiling {

namespace {
uint32_t ComputeHash(const std::vector<Address>& addrs) {
  return base::Hash(addrs.data(), addrs.size() * sizeof(Address));
}
}  // namespace

AllocationSite::AllocationSite(AllocatorType allocator,
                               std::vector<Address>&& stack,
                               int context_id)
    : allocator(allocator),
      stack(std::move(stack)),
      context_id(context_id),
      hash_(ComputeHash(this->stack)) {}

AllocationSite::~AllocationSite() = default;

}  // namespace heap_profiling
