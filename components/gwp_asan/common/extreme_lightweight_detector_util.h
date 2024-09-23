// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_
#define COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_

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
  static void Zap(void* ptr, size_t size) {
    uint64_t zap_value = kMarker | kDefaultMetadata | kOffset;
    size_t count = size / sizeof zap_value;
    std::fill_n(static_cast<decltype(zap_value)*>(ptr), count, zap_value);
    size_t remainder_offset = sizeof zap_value * count;
    size_t remainder_size = size - remainder_offset;
    std::fill_n(static_cast<uint8_t*>(ptr) + remainder_offset, remainder_size,
                kRemainderZapValue);
  }

 private:
  static constexpr uint64_t kMarker = 0xECEC000000000000u;
  static constexpr uint64_t kMarkerMask = 0xFFFF000000000000u;
  static constexpr uint64_t kDefaultMetadata = 0x0000EFEFEFEF0000u;
  static constexpr uint64_t kOffset = 0x8000u;
  static constexpr uint8_t kRemainderZapValue = 0xED;
};

}  // namespace gwp_asan::internal

#endif  // COMPONENTS_GWP_ASAN_COMMON_EXTREME_LIGHTWEIGHT_DETECTOR_UTIL_H_
