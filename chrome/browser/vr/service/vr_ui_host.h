// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SERVICE_VR_UI_HOST_H_
#define CHROME_BROWSER_VR_SERVICE_VR_UI_HOST_H_

#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace vr {

// Abstract class to break a dependency loop between the "vr_common" component
// when accessing "browser" component functionality such as the permission
// manager. A concrete VRUiHostImpl object is injected through a factory method
// registered from chrome_browser_main.cc. The concrete VRUiHost owns a thread
// which draws browser UI such as permission prompts, and starts and stops that
// thread as needed.
class VR_EXPORT VRUiHost {
 public:
  virtual ~VRUiHost() = 0;

  using Factory = std::unique_ptr<VRUiHost>(
      device::mojom::XRDeviceId device_id,
      mojo::PendingRemote<device::mojom::XRCompositorHost> compositor);

  static void SetFactory(Factory* factory);
  static Factory* GetFactory();
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SERVICE_VR_UI_HOST_H_
