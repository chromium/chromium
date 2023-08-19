// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/device.h"

#import <Metal/Metal.h>

#include "base/logging.h"

namespace metal {

id<MTLDevice> GetDefaultDevice() {
  // First attempt to find a low power device to use.
#if BUILDFLAG(IS_MAC)
  for (id<MTLDevice> device in MTLCopyAllDevices()) {
    if (device.lowPower) {
      return device;
    }
  }
#endif
  // Failing that, use the system default device.
  id<MTLDevice> system_default = MTLCreateSystemDefaultDevice();
  if (!system_default) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nil;
  }

  return system_default;
}

}  // namespace metal
