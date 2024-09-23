// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/sparky/sparky_util.h"

namespace sparky {

int64_t RoundByteSize(int64_t bytes) {
  if (bytes < 0) {
    return -1;
  }

  if (bytes == 0) {
    return 0;
  }

  // Subtract one to the original number of bytes.
  bytes--;
  // Set all the lower bits to 1.
  bytes |= bytes >> 1;
  bytes |= bytes >> 2;
  bytes |= bytes >> 4;
  bytes |= bytes >> 8;
  bytes |= bytes >> 16;
  bytes |= bytes >> 32;
  // Add one. The one bit beyond the highest set bit is set to 1. All the lower
  // bits are set to 0.
  bytes++;

  return bytes;
}

}  // namespace sparky
