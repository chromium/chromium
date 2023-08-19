// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_DEVICE_H_
#define COMPONENTS_METAL_UTIL_DEVICE_H_

#include "components/metal_util/metal_util_export.h"

@protocol MTLDevice;

namespace metal {

// Return a low-power device, if one exists, otherwise return the system default
// device.
id<MTLDevice> METAL_UTIL_EXPORT GetDefaultDevice();

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_DEVICE_H_
