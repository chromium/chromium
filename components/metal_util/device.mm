// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/device.h"

#import <Metal/Metal.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace metal {

MTLDevicePtr CreateDefaultDevice() {
  // First attempt to find a low power device to use.
  base::scoped_nsprotocol<id<MTLDevice>> device_to_use;
#if BUILDFLAG(IS_MAC)
  base::scoped_nsobject<NSArray<id<MTLDevice>>> devices(MTLCopyAllDevices());
  for (id<MTLDevice> device in devices.get()) {
    if ([device isLowPower]) {
      device_to_use.reset(device, base::scoped_policy::RETAIN);
      break;
    }
  }
#endif
  // Failing that, use the system default device.
  if (!device_to_use)
    device_to_use.reset(MTLCreateSystemDefaultDevice());
  if (!device_to_use) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nullptr;
  }
  return device_to_use.release();
}

}  // namespace metal
