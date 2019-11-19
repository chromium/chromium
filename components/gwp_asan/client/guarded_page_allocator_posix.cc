// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <sys/mman.h>

#include "base/logging.h"

namespace gwp_asan {
namespace internal {

void* GuardedPageAllocator::MapRegion() {
  return mmap(MapRegionHint(), RegionSize(), PROT_NONE,
              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

void GuardedPageAllocator::UnmapRegion() {
  CHECK(state_.pages_base_addr);
  int err =
      munmap(reinterpret_cast<void*>(state_.pages_base_addr), RegionSize());
  DPCHECK(err == 0) << "munmap";
  (void)err;
}

void GuardedPageAllocator::MarkPageReadWrite(void* ptr) {
  int err = mprotect(ptr, state_.page_size, PROT_READ | PROT_WRITE);
  PCHECK(err == 0) << "mprotect";
}

void GuardedPageAllocator::MarkPageInaccessible(void* ptr) {
  // mmap() a PROT_NONE page over the address to release it to the system, if
  // we used mprotect() here the system would count pages in the quarantine
  // against the RSS.
  void* err = mmap(ptr, state_.page_size, PROT_NONE,
                   MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
  PCHECK(err == ptr) << "mmap";
}

}  // namespace internal
}  // namespace gwp_asan
