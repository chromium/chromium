// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_HEAP_PROFILING_ALLOCATION_H_
#define COMPONENTS_SERVICES_HEAP_PROFILING_ALLOCATION_H_

#include <unordered_map>
#include <vector>

#include "components/services/heap_profiling/public/mojom/heap_profiling_client.mojom.h"

namespace heap_profiling {

using Address = uint64_t;
using mojom::AllocatorType;

// The struct is a descriptor of an allocation site. It is used as a unique
// key in the AllocationMap.
struct AllocationSite {
  AllocationSite(AllocatorType allocator,
                 std::vector<Address>&& stack,
                 int context_id);

  AllocationSite(const AllocationSite&) = delete;
  AllocationSite& operator=(const AllocationSite&) = delete;

  ~AllocationSite();

  // Type of the allocator responsible for the allocation. Possible values are
  // kMalloc, kPartitionAlloc, or kOilpan.
  const AllocatorType allocator;

  // Program call stack at the moment of allocation. Each address is correspond
  // to a code memory location in the inspected process.
  const std::vector<Address> stack;

  // Each allocation call may be associated with a context string.
  // This field contains the id of the context string. The string itself is
  // stored in |context_map| array in ExportParams class.
  const int context_id;

  struct Hash {
    size_t operator()(const AllocationSite& alloc) const { return alloc.hash_; }
  };

 private:
  const size_t hash_;
};

inline bool operator==(const AllocationSite& a, const AllocationSite& b) {
  return a.allocator == b.allocator && a.stack == b.stack &&
         a.context_id == b.context_id;
}

// Data associated with an allocation site in the AllocationMap.
struct AllocationMetrics {
  // Total size of allocations associated with a given sample.
  size_t size = 0;

  // Number of allocations associated with the sample.
  float count = 0;
};

using AllocationMap =
    std::unordered_map<AllocationSite, AllocationMetrics, AllocationSite::Hash>;

}  // namespace heap_profiling

#endif  // COMPONENTS_SERVICES_HEAP_PROFILING_ALLOCATION_H_
