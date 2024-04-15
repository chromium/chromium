// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <windows.h>

#include "base/check.h"
#include "build/build_config.h"

namespace gwp_asan {
namespace internal {

void* GuardedPageAllocator::MapRegion() {
  // Number of times to try to map the region in high memory before giving up.
  constexpr size_t kHintTries = 5;
  for (size_t i = 0; i < kHintTries; i++) {
    if (void* ptr = VirtualAlloc(MapRegionHint(), RegionSize(), MEM_RESERVE,
                                 PAGE_NOACCESS))
      return ptr;
  }

  return VirtualAlloc(nullptr, RegionSize(), MEM_RESERVE, PAGE_NOACCESS);
}

void GuardedPageAllocator::UnmapRegion() {
  CHECK(state_.pages_base_addr);
  [[maybe_unused]] BOOL err = VirtualFree(
      reinterpret_cast<void*>(state_.pages_base_addr), 0, MEM_RELEASE);
  DPCHECK(err) << "VirtualFree";
}

void GuardedPageAllocator::MarkPageReadWrite(void* ptr) {
  LPVOID ret = VirtualAlloc(ptr, state_.page_size, MEM_COMMIT, PAGE_READWRITE);
  PCHECK(ret != nullptr) << "VirtualAlloc";
}

void GuardedPageAllocator::MarkPageInaccessible(void* ptr) {
  BOOL err = VirtualFree(ptr, state_.page_size, MEM_DECOMMIT);
  PCHECK(err != 0) << "VirtualFree";
}

}  // namespace internal
}  // namespace gwp_asan
