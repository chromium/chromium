// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_MAC_CONVERT_BIG_ENDIAN_H_
#define CHROME_UTILITY_SAFE_BROWSING_MAC_CONVERT_BIG_ENDIAN_H_

#include <libkern/OSByteOrder.h>
#include <stdint.h>
#include <sys/types.h>

// This file contains byte swapping routines for use in safe_browsing::dmg. The
// pattern is to use type-based overloading of the form ConvertBigEndian(T*) to
// swap all structures from big-endian to host-endian. This file provides the
// implementations for scalars, which are inlined since the OSSwap functions
// themselves are macros that call compiler intrinsics.

namespace safe_browsing {
namespace dmg {

inline void ConvertBigEndian(uint16_t* x) {
  *x = OSSwapBigToHostInt16(*x);
}

inline void ConvertBigEndian(int16_t* x) {
  *x = OSSwapBigToHostInt16(*x);
}

inline void ConvertBigEndian(uint32_t* x) {
  *x = OSSwapBigToHostInt32(*x);
}

inline void ConvertBigEndian(uint64_t* x) {
  *x = OSSwapBigToHostInt64(*x);
}

}  // namespace dmg
}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_MAC_CONVERT_BIG_ENDIAN_H_
