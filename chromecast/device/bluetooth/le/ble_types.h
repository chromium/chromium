// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_TYPES_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_TYPES_H_

#include <cstdint>

#include "build/build_config.h"

namespace chromecast {
namespace bluetooth {

#if BUILDFLAG(IS_FUCHSIA)
using HandleId = uint64_t;
#else
using HandleId = uint16_t;
#endif

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_BLE_TYPES_H_
