// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/device.h"

#import <Metal/Metal.h>

#include "base/mac/scoped_nsobject.h"

namespace metal {

MTLDevicePtr CreateDefaultDevice() {
  if (@available(macOS 10.11, *)) {
    // First attempt to find a low power device to use.
    base::scoped_nsprotocol<id<MTLDevice>> device_to_use;
    base::scoped_nsobject<NSArray<id<MTLDevice>>> devices(MTLCopyAllDevices());
    for (id<MTLDevice> device in devices.get()) {
      if ([device isLowPower]) {
        device_to_use.reset(device, base::scoped_policy::RETAIN);
        break;
      }
    }
    // Failing that, use the system default device.
    if (!device_to_use)
      device_to_use.reset(MTLCreateSystemDefaultDevice());
    if (!device_to_use) {
      DLOG(ERROR) << "Failed to find MTLDevice.";
      return nullptr;
    }
    return device_to_use.release();
  }
  // If no device was found, or if the macOS version is too old for Metal,
  // return no context provider.
  return nullptr;
}

}  // namespace metal
