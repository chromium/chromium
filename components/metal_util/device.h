// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_DEVICE_H_
#define COMPONENTS_METAL_UTIL_DEVICE_H_

#include "components/metal_util/metal_util_export.h"
#include "components/metal_util/types.h"

namespace metal {

// Return a low-power device, if one exists, otherwise return the system default
// device.
MTLDevicePtr METAL_UTIL_EXPORT CreateDefaultDevice();

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_DEVICE_H_
