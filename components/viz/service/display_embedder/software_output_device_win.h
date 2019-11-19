// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_

#include <windows.h>

#include <memory>

#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"

namespace viz {

class OutputDeviceBacking;

// Creates an appropriate SoftwareOutputDevice implementation.
VIZ_SERVICE_EXPORT std::unique_ptr<SoftwareOutputDevice>
CreateSoftwareOutputDeviceWin(HWND hwnd,
                              OutputDeviceBacking* backing,
                              mojom::DisplayClient* display_client);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SOFTWARE_OUTPUT_DEVICE_WIN_H_
