// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/device.h"

#import <Metal/Metal.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"

namespace metal {

id<MTLDevice> GetDefaultDevice() {
  // First attempt to find a low power device to use.
#if BUILDFLAG(IS_MAC)
  base::scoped_nsobject<NSArray<id<MTLDevice>>> devices(MTLCopyAllDevices());
  for (id<MTLDevice> device in devices.get()) {
    if (device.lowPower) {
      return [[device retain] autorelease];
    }
  }
#endif
  // Failing that, use the system default device.
  id<MTLDevice> system_default = MTLCreateSystemDefaultDevice();
  if (!system_default) {
    DLOG(ERROR) << "Failed to find MTLDevice.";
    return nil;
  }

  return [system_default autorelease];
}

}  // namespace metal
