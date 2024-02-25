// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_STATE_H_
#define COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_STATE_H_

#include <stdint.h>

#include <optional>

#include "components/gwp_asan/common/allocation_info.h"

namespace gwp_asan::internal {

// This enum is used during GWP-ASan initialization to control the
// Lightweight UAF Detector.
enum class LightweightDetectorMode : uint8_t {
  kOff,
  kBrpQuarantine,
  kRandom,
};

// Encapsulates Lightweight UAF Detector's state shared with with the crash
// handler process.
class LightweightDetectorState {
 public:
  using MetadataId = uint32_t;
  using PseudoAddress = uint64_t;

  // Maximum number of metadata slots used by Lightweight UAF Detector.
  static constexpr size_t kMaxMetadata = 32768;
  // Maximum number of stack trace frames collected by Lightweight UAF Detector.
  static constexpr size_t kMaxStackFrames = 25;
  // Number of bytes allocated by Lightweight UAF Detector for a packed
  // deallocation stack trace. (Allocation stack traces aren't collected.)
  static constexpr size_t kMaxPackedTraceLength = 90;

  // Use the following format to encode 32-bit metadata IDs into 64-bit pseudo
  // addresses:
  //  - Bits 0 - 15 are set to 0x8000 so that small increments/decrements
  //    don't affect the metadata ID stored in the higher bits.
  //  - Bits 16 - 47 contain the metadata ID.
  //  - Bits 48 - 63 are set to 0xEFED so that access at the address always
  //    results in a memory access error, and the address can be recognized by
  //    the crash handler.
  static constexpr uint64_t kMetadataIdMarker = 0xEFED000000000000;
  static constexpr uint64_t kMetadataIdMarkerMask = 0xFFFF000000000000;
  static constexpr uint64_t kMetadataIdShift = 16;
  static constexpr uint64_t kMetadataIdOffset = 0x8000;
  static constexpr uint64_t kMetadataRemainder = 0xED;

  struct SlotMetadata {
    SlotMetadata();

    // Size of the allocation
    size_t alloc_size = 0;
    // The allocation address.
    uintptr_t alloc_ptr = 0;
    // Holds the deallocation stack trace.
    uint8_t deallocation_stack_trace[kMaxPackedTraceLength];

    static_assert(
        std::numeric_limits<decltype(AllocationInfo::trace_len)>::max() >=
            kMaxPackedTraceLength,
        "AllocationInfo::trace_len can hold all possible length values.");

    AllocationInfo dealloc;

    // Used to make sure the metadata entry isn't stale.
    MetadataId id = std::numeric_limits<MetadataId>::max();
  };

  static PseudoAddress EncodeMetadataId(MetadataId metadata_id) {
    return kMetadataIdMarker |
           (static_cast<PseudoAddress>(metadata_id) << kMetadataIdShift) |
           kMetadataIdOffset;
  }

  static std::optional<MetadataId> ExtractMetadataId(PseudoAddress address) {
    if ((address & kMetadataIdMarkerMask) != kMetadataIdMarker) {
      return std::nullopt;
    }

    return (address & ~kMetadataIdMarkerMask) >> kMetadataIdShift;
  }

  // Sanity check the state. This method is used to verify that it is well
  // formed when the crash handler analyzes the state from a crashing process.
  // This method is security-sensitive, it must validate parameters to ensure
  // that an attacker with the ability to modify the state can not cause the
  // crash handler to misbehave and cause memory errors.
  bool IsValid() const;

  // Returns a reference to the metadata entry in Lightweight UAF Detector's
  // ring buffer. Different IDs may point to the same slot.
  SlotMetadata& GetSlotMetadataById(MetadataId, SlotMetadata* metadata_arr);

  // The relationship between a metadata slot and an ID is one-to-many.
  // This function returns true if the ID stored in the slot matches
  // the ID that's used to access the slot.
  bool HasMetadataForId(MetadataId, SlotMetadata* metadata_arr);

  LightweightDetectorMode mode = LightweightDetectorMode::kOff;
  // Number of entries in |metadata_addr|.
  size_t num_metadata = 0;
  // Pointer to an array of metadata about every allocation, including its size,
  // offset, and pointers to the deallocation stack trace.
  uintptr_t metadata_addr = 0;
};

// Ensure that the detector state is a plain-old-data. That way we can safely
// initialize it by copying memory from out-of-process without worrying about
// destructors operating on the fields in an unexpected way.
static_assert(std::is_trivially_copyable<LightweightDetectorState>(),
              "LightweightDetectorState must be POD");
static_assert(
    std::is_trivially_copyable<LightweightDetectorState::SlotMetadata>(),
    "LightweightDetectorState::SlotMetadata must be POD");

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_STATE_H_
