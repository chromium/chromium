// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The AllocatorState class is the subset of the GuardedPageAllocator that is
// required by the crash handler to analyzer crashes and provide debug
// information. The crash handler initializes an instance of this class from
// the crashed processes memory. Because the out-of-process allocator could be
// corrupted or maliciously tampered with, this class is security sensitive and
// needs to be modified with care. It has been purposefully designed to be:
// - Minimal: This is the minimum set of methods and members required by the
//     crash handler.
// - Trivially copyable: An instance of this object is copied from another
//     processes memory. Ensuring it is trivially copyable means that the crash
//     handler will not accidentally trigger a complex destructor on objects
//     initialized from another processes memory.
// - Free of pointers: Pointers are all uintptr_t since none of these pointers
//     need to be directly dereferenced. Encourage users like the crash handler
//     to consider them addresses instead of pointers.
// - Validatable: The IsValid() method is intended to sanity check the internal
//     fields such that it's safe to call any method on a valid object. All
//     additional methods and fields need to be audited to ensure they maintain
//     this invariant!

#ifndef COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_
#define COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_

#include <atomic>
#include <limits>
#include <string>
#include <type_traits>

#include "base/threading/platform_thread.h"

namespace gwp_asan {
namespace internal {

class GuardedPageAllocator;

class AllocatorState {
 public:
  using MetadataIdx = uint16_t;
  using SlotIdx = uint16_t;

  // Maximum number of virtual memory slots (guard-page buffered pages) this
  // class can allocate.
  static constexpr size_t kMaxSlots = 4096;
  // Maximum number of concurrent allocations/metadata this class can allocate.
  static constexpr size_t kMaxMetadata = 2048;
  // Invalid metadata index.
  static constexpr MetadataIdx kInvalidMetadataIdx = kMaxMetadata;

  // Maximum number of stack trace frames to collect for an allocation or
  // deallocation.
  static constexpr size_t kMaxStackFrames = 100;
  // Number of bytes to allocate for both allocation and deallocation packed
  // stack traces. (Stack trace entries take ~3.5 bytes on average.)
  static constexpr size_t kMaxPackedTraceLength = 400;

  static_assert(std::numeric_limits<SlotIdx>::max() >= kMaxSlots - 1,
                "SlotIdx can hold all possible slot index values");
  static_assert(std::numeric_limits<MetadataIdx>::max() >= kMaxMetadata - 1,
                "MetadataIdx can hold all possible metadata index values");
  static_assert(kInvalidMetadataIdx >= kMaxMetadata,
                "kInvalidMetadataIdx can not reference a real index");

  enum class ErrorType {
    kUseAfterFree = 0,
    kBufferUnderflow = 1,
    kBufferOverflow = 2,
    kDoubleFree = 3,
    kUnknown = 4,
    kFreeInvalidAddress = 5,
  };

  enum class GetMetadataReturnType {
    kGwpAsanCrash = 0,
    kGwpAsanCrashWithMissingMetadata = 1,
    kErrorBadSlot = 2,
    kErrorBadMetadataIndex = 3,
    kErrorOutdatedMetadataIndex = 4,
  };

  // Structure for storing data about a slot.
  struct SlotMetadata {
    SlotMetadata();

    // Information saved for allocations and deallocations.
    struct AllocationInfo {
      // (De)allocation thread id or base::kInvalidThreadId if no (de)allocation
      // occurred.
      uint64_t tid = base::kInvalidThreadId;
      // Length used to encode the packed stack trace.
      uint16_t trace_len = 0;
      // Whether a stack trace has been collected for this (de)allocation.
      bool trace_collected = false;

      static_assert(std::numeric_limits<decltype(trace_len)>::max() >=
                        kMaxPackedTraceLength - 1,
                    "trace_len can hold all possible length values.");
    };

    // Size of the allocation
    size_t alloc_size = 0;
    // The allocation address.
    uintptr_t alloc_ptr = 0;
    // Used to synchronize whether a deallocation has occurred (e.g. whether a
    // double free has occurred) between threads.
    std::atomic<bool> deallocation_occurred{false};
    // Holds the combined allocation/deallocation stack traces. The deallocation
    // stack trace is stored immediately after the allocation stack trace to
    // optimize on space.
    uint8_t stack_trace_pool[kMaxPackedTraceLength];

    AllocationInfo alloc;
    AllocationInfo dealloc;
  };

  AllocatorState();

  // Returns true if address is in memory managed by this class.
  inline bool PointerIsMine(uintptr_t addr) const {
    return pages_base_addr <= addr && addr < pages_end_addr;
  }

  // Sanity check allocator internals. This method is used to verify that
  // the allocator base state is well formed when the crash handler analyzes the
  // allocator from a crashing process. This method is security-sensitive, it
  // must validate parameters to ensure that an attacker with the ability to
  // modify the allocator internals can not cause the crash handler to misbehave
  // and cause memory errors.
  bool IsValid() const;

  // This method is meant to be called from the crash handler with a validated
  // AllocatorState object read from the crashed process and an exception
  // address known to be in the GWP-ASan allocator region. Given the metadata
  // and slot to metadata arrays for the allocator, this method returns an enum
  // indicating an error or a GWP-ASan exception with or without metadata. If
  // metadata is available, the |metadata_idx| parameter stores the index of the
  // relevant metadata in the given array. If an error occurs, the |error|
  // parameter is filled out with an error string.
  GetMetadataReturnType GetMetadataForAddress(
      uintptr_t exception_address,
      const SlotMetadata* metadata_arr,
      const MetadataIdx* slot_to_metadata,
      MetadataIdx* metadata_idx,
      std::string* error) const;

  // Returns the likely error type given an exception address and whether its
  // previously been allocated and deallocated.
  ErrorType GetErrorType(uintptr_t addr,
                         bool allocated,
                         bool deallocated) const;

  // Returns the address of the page that addr resides on.
  uintptr_t GetPageAddr(uintptr_t addr) const;

  // Returns an address somewhere on the valid page nearest to addr.
  uintptr_t GetNearestValidPage(uintptr_t addr) const;

  // Returns the slot number for the page nearest to addr.
  SlotIdx GetNearestSlot(uintptr_t addr) const;

  uintptr_t SlotToAddr(SlotIdx slot) const;
  SlotIdx AddrToSlot(uintptr_t addr) const;

  uintptr_t pages_base_addr = 0;  // Points to start of mapped region.
  uintptr_t pages_end_addr = 0;   // Points to the end of mapped region.
  uintptr_t first_page_addr = 0;  // Points to first allocatable page.
  size_t num_metadata = 0;        // Number of entries in |metadata_addr|.
  size_t total_pages = 0;         // Virtual memory page pool size.
  size_t page_size = 0;           // Page size.

  // Pointer to an array of metadata about every allocation, including its size,
  // offset, and pointers to the allocation/deallocation stack traces (if
  // present.)
  uintptr_t metadata_addr = 0;
  // Pointer to an array that maps a slot index to a metadata index (or
  // kInvalidMetadataIdx if no such mapping exists) in |metadata_addr|.
  uintptr_t slot_to_metadata_addr;

  // Set to the address of a double freed allocation if a double free occurred.
  uintptr_t double_free_address = 0;
  // If an invalid pointer has been free()d, this is the address of that invalid
  // pointer.
  uintptr_t free_invalid_address = 0;

  DISALLOW_COPY_AND_ASSIGN(AllocatorState);
};

// Ensure that the allocator state is a plain-old-data. That way we can safely
// initialize it by copying memory from out-of-process without worrying about
// destructors operating on the fields in an unexpected way.
static_assert(std::is_trivially_copyable<AllocatorState>(),
              "AllocatorState must be POD");

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_ALLOCATOR_STATE_H_
