// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/gwp_asan/common/allocator_state.h"

#include <algorithm>

#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace gwp_asan {
namespace internal {

AllocatorState::AllocatorState() {}

AllocatorState::GetMetadataReturnType AllocatorState::GetMetadataForAddress(
    uintptr_t exception_address,
    const SlotMetadata* metadata_arr,
    const MetadataIdx* slot_to_metadata,
    MetadataIdx* metadata_idx,
    std::string* error) const {
  CHECK(IsValid());
  CHECK(PointerIsMine(exception_address));

  AllocatorState::SlotIdx slot_idx = GetNearestSlot(exception_address);
  if (slot_idx >= total_reserved_pages) {
    *error = base::StringPrintf("Bad slot index %u >= %zu", slot_idx,
                                total_reserved_pages);
    return GetMetadataReturnType::kErrorBadSlot;
  }

  AllocatorState::MetadataIdx index = slot_to_metadata[slot_idx];
  if (index == kInvalidMetadataIdx)
    return GetMetadataReturnType::kGwpAsanCrashWithMissingMetadata;

  if (index >= num_metadata) {
    *error =
        base::StringPrintf("Bad metadata index %u >= %zu", index, num_metadata);
    return GetMetadataReturnType::kErrorBadMetadataIndex;
  }

  if (GetNearestSlot(metadata_arr[index].alloc_ptr) != slot_idx) {
    *error = base::StringPrintf(
        "Outdated metadata index %u: slot for %zx does not match %zx", index,
        metadata_arr[index].alloc_ptr, exception_address);
    return GetMetadataReturnType::kErrorOutdatedMetadataIndex;
  }

  *metadata_idx = index;
  return GetMetadataReturnType::kGwpAsanCrash;
}

bool AllocatorState::IsValid() const {
  if (!page_size || page_size != base::GetPageSize())
    return false;

  if (total_requested_pages == 0 || total_requested_pages > kMaxRequestedSlots)
    return false;

  if (total_reserved_pages == 0 || total_reserved_pages > kMaxReservedSlots ||
      total_reserved_pages < total_requested_pages)
    return false;

  if (num_metadata == 0 ||
      num_metadata > std::min(kMaxMetadata, total_requested_pages))
    return false;

  if (pages_base_addr % page_size != 0 || pages_end_addr % page_size != 0 ||
      first_page_addr % page_size != 0)
    return false;

  if (pages_base_addr >= pages_end_addr)
    return false;

  if (first_page_addr != pages_base_addr + page_size ||
      pages_end_addr - pages_base_addr !=
          page_size * (total_reserved_pages * 2 + 1))
    return false;

  if (!metadata_addr || !slot_to_metadata_addr)
    return false;

  return true;
}

uintptr_t AllocatorState::GetPageAddr(uintptr_t addr) const {
  const uintptr_t addr_mask = ~(page_size - 1ULL);
  return addr & addr_mask;
}

uintptr_t AllocatorState::GetNearestValidPage(uintptr_t addr) const {
  if (addr < first_page_addr)
    return first_page_addr;
  const uintptr_t last_page_addr = pages_end_addr - 2 * page_size;
  if (addr > last_page_addr)
    return last_page_addr;

  uintptr_t offset = addr - first_page_addr;
  // If addr is already on a valid page, just return addr.
  if ((offset >> base::bits::Log2Floor(page_size)) % 2 == 0)
    return addr;

  // ptr points to a guard page, so get nearest valid page.
  const size_t kHalfPageSize = page_size / 2;
  if ((offset >> base::bits::Log2Floor(kHalfPageSize)) % 2 == 0) {
    return addr - kHalfPageSize;  // Round down.
  }
  return addr + kHalfPageSize;  // Round up.
}

AllocatorState::SlotIdx AllocatorState::GetNearestSlot(uintptr_t addr) const {
  return AddrToSlot(GetPageAddr(GetNearestValidPage(addr)));
}

AllocatorState::ErrorType AllocatorState::GetErrorType(uintptr_t addr,
                                                       bool allocated,
                                                       bool deallocated) const {
  if (free_invalid_address)
    return ErrorType::kFreeInvalidAddress;
  if (!allocated)
    return ErrorType::kUnknown;
  if (double_free_address)
    return ErrorType::kDoubleFree;
  if (deallocated)
    return ErrorType::kUseAfterFree;
  if (addr < first_page_addr)
    return ErrorType::kBufferUnderflow;
  const uintptr_t last_page_addr = pages_end_addr - 2 * page_size;
  if (addr > last_page_addr)
    return ErrorType::kBufferOverflow;
  const uintptr_t offset = addr - first_page_addr;

  // If we hit this condition, it means we crashed on accessing an allocation
  // even though it's currently allocated [there is a if(deallocated) return
  // earlier.] This can happen when a use-after-free causes a crash and another
  // thread manages to allocate the page in another thread before it's stopped.
  // This can happen with low sampling frequencies and high parallel allocator
  // usage.
  if ((offset >> base::bits::Log2Floor(page_size)) % 2 == 0) {
    LOG(WARNING) << "Hit impossible error condition, likely caused by a racy "
                    "use-after-free";
    return ErrorType::kUnknown;
  }

  const size_t kHalfPageSize = page_size / 2;
  return (offset >> base::bits::Log2Floor(kHalfPageSize)) % 2 == 0
             ? ErrorType::kBufferOverflow
             : ErrorType::kBufferUnderflow;
}

uintptr_t AllocatorState::SlotToAddr(AllocatorState::SlotIdx slot) const {
  DCHECK_LE(slot, kMaxReservedSlots);
  return first_page_addr + 2 * slot * page_size;
}

AllocatorState::SlotIdx AllocatorState::AddrToSlot(uintptr_t addr) const {
  DCHECK_EQ(addr % page_size, 0ULL);
  uintptr_t offset = addr - first_page_addr;
  DCHECK_EQ((offset >> base::bits::Log2Floor(page_size)) % 2, 0ULL);
  size_t slot = (offset >> base::bits::Log2Floor(page_size)) / 2;
  DCHECK_LE(slot, kMaxReservedSlots);
  return static_cast<SlotIdx>(slot);
}

AllocatorState::SlotMetadata::SlotMetadata() = default;

}  // namespace internal
}  // namespace gwp_asan
