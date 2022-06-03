// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METAL_UTIL_DEVICE_REMOVAL_H_
#define COMPONENTS_METAL_UTIL_DEVICE_REMOVAL_H_

#include "components/metal_util/metal_util_export.h"

namespace metal {

void METAL_UTIL_EXPORT RegisterGracefulExitOnDeviceRemoval();

}  // namespace metal

#endif  // COMPONENTS_METAL_UTIL_DEVICE_REMOVAL_H_
