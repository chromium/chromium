// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_H_
#define COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace gwp_asan::internal {

class LightweightDetector {
 public:
  using MetadataId = uint32_t;
  using PseudoAddresss = uint64_t;

  // This enum is used during allocator initialization to control the
  // Lightweight UaF Detector.
  enum class State : bool {
    kDisabled,
    kEnabled,
  };

  // Use the following format to encode 32-bit metadata IDs into 64-bit pseudo
  // addresses:
  //  - Bits 0 - 15 are set to 0x8000 so that small increments/decrements
  //    don't affect the metadata ID stored in the higher bits.
  //  - Bits 16 - 47 contain the metadata ID.
  //  - Bits 48 - 63 are set to 0xEDED so that access at the address always
  //    results in a memory access error, and the address can be recognized by
  //    the crash handler.
  static constexpr uint64_t kMetadataIdMarker = 0xEFED000000000000;
  static constexpr uint64_t kMetadataIdMarkerMask = 0xFFFF000000000000;
  static constexpr uint64_t kMetadataIdShift = 16;
  static constexpr uint64_t kMetadataIdOffset = 0x8000;
  static constexpr uint64_t kMetadataRemainder = 0xED;

  static PseudoAddresss EncodeMetadataId(MetadataId metadata_id) {
    return kMetadataIdMarker | (metadata_id << kMetadataIdShift) |
           kMetadataIdOffset;
  }

  static absl::optional<MetadataId> ExtractMetadataId(PseudoAddresss address) {
    if ((address & kMetadataIdMarkerMask) != kMetadataIdMarker) {
      return absl::nullopt;
    }

    return (address & ~kMetadataIdMarkerMask) >> kMetadataIdShift;
  }
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_COMMON_LIGHTWEIGHT_DETECTOR_H_
