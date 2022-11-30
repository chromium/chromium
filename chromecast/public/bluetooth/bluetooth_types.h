// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_BLUETOOTH_BLUETOOTH_TYPES_H_
#define CHROMECAST_PUBLIC_BLUETOOTH_BLUETOOTH_TYPES_H_

#include <array>
#include <cstdint>

namespace chromecast {
namespace bluetooth_v2_shlib {

static constexpr size_t kAddrLen = 6;
static constexpr size_t kUuidLen = 16;
using Addr = std::array<uint8_t, kAddrLen>;  // Little endian
using Uuid = std::array<uint8_t, kUuidLen>;  // Big endian

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_BLUETOOTH_BLUETOOTH_TYPES_H_
