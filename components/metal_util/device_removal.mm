// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metal_util/device_removal.h"

#import <Metal/Metal.h>

#include "base/process/process.h"

namespace metal {

void RegisterGracefulExitOnDeviceRemoval() {
  id<NSObject> device_observer = nil;
  // `MTLCopyAllDevicesWithObserver` is the only way to set an observer for the
  // change of the MTLDevice list. Because setting an observer is all that's
  // needed, ignore the result of the call.
  MTLCopyAllDevicesWithObserver(
      &device_observer,
      ^(id<MTLDevice> device, MTLDeviceNotificationName name) {
        if (name == MTLDeviceRemovalRequestedNotification ||
            name == MTLDeviceWasRemovedNotification) {
          // Exit the GPU process without error. The browser process sees
          // this error code as a graceful shutdown, so relaunches the GPU
          // process without incrementing the crash count.
          //
          // Note this wouldn't work nicely with in-process-gpu (it would
          // exit the browser), but we don't support that on macOS anyway.
          base::Process::TerminateCurrentProcessImmediately(0);
        }
      });
}

}  // namespace metal
