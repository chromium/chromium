// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_
#define COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_

#include "base/compiler_specific.h"

namespace gwp_asan::internal {

// Provides utility functions shared by the quarantine code and crash handler.
// The class name comes from ExtremeLightweightDetector, but it's also known as
// Extreme Lightweight UAF Detector (Extreme LUD or ELUD).
class ExtremeLightweightDetectorUtil {
 public:
  // This is a static-members-only class.
  ExtremeLightweightDetectorUtil() = delete;

  // Returns true if the crash is caused by the Extreme Lightweight UAF
  // Detector.
  static bool IsELUDCrash(uint64_t candidate_address) {
    return (candidate_address & kMarkerMask) == kMarker;
  }

  // Zaps the given memory region with the special marker of Extreme Lightweight
  // UAF Detector.
  static void Zap(void* ptr,
                  size_t object_size,
                  size_t usable_size,
                  size_t alignment) {
    uint64_t usable_size_metadata = usable_size;
    if (usable_size > 0xFFFFu) [[unlikely]] {
      usable_size_metadata = 0u;
    }
    if (alignment != 0) [[unlikely]] {
      // The LSB of usable_size must always be zero because PartitionAlloc
      // allocates memory in size of multiples of 8. The LSB is used to
      // indicate whether it's alignment-specified deallocation or not.
      usable_size_metadata |= 1u;
    }
    uint64_t object_size_metadata = object_size;
    if (object_size > 0xFFFFu) [[unlikely]] {
      object_size_metadata = 0u;
    }
    // By taking the bitwise-not of size, it becomes easier to distinguish the
    // data from accidental data writes. E.g. when the program writes 0x0010u of
    // 16-bit data to the object_size_metadata region, it looks the size
    // 0xFFEFu, which is a very unlikely size. Therefore, it's easy to suspect
    // unintended data writes.
    uint64_t zap_value = kMarker | (~usable_size_metadata & 0xFFFFu) << 32 |
                         (~object_size_metadata & 0xFFFFu) << 16 | kOffset;
    size_t count = usable_size / sizeof zap_value;
    std::fill_n(static_cast<decltype(zap_value)*>(ptr), count, zap_value);
    size_t remainder_offset = sizeof zap_value * count;
    size_t remainder_size = usable_size - remainder_offset;
    std::fill_n(UNSAFE_TODO(static_cast<uint8_t*>(ptr) + remainder_offset),
                remainder_size, kRemainderZapValue);
  }

 private:
  static constexpr uint64_t kMarker = 0xEBAF000000000000u;
  static constexpr uint64_t kMarkerMask = 0xFFFF000000000000u;
  static constexpr uint64_t kOffset = 0x8000u;
  static constexpr uint8_t kRemainderZapValue = 0xED;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_
