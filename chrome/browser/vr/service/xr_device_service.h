// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_XR_DEVICE_SERVICE_H_
#define CHROME_BROWSER_VR_SERVICE_XR_DEVICE_SERVICE_H_

#include "base/callback.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace vr {

// Acquires a remote handle to the sandboxed isolated XR Device Service
// instance, launching a process to host the service if necessary.
VR_EXPORT const mojo::Remote<device::mojom::XRDeviceService>&
GetXRDeviceService();

// Allows tests to perform extra initialization steps on any new XR Device
// Service instance before other client code can use it. Any time a new instance
// of the service is started by |GetXRDeviceService()|, this callback (if
// non-null) is invoked.
VR_EXPORT void SetXRDeviceServiceStartupCallbackForTesting(
    base::RepeatingClosure callback);

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_XR_DEVICE_SERVICE_H_
