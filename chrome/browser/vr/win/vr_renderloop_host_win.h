// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_VR_RENDERLOOP_HOST_WIN_H_
#define CHROME_BROWSER_VR_WIN_VR_RENDERLOOP_HOST_WIN_H_

#include "base/threading/thread.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

namespace vr {

class VRBrowserRendererThreadWin;

class VRBrowserRendererHostWin : public BrowserXRRuntimeObserver {
 public:
  // Called by IsolatedDeviceProvider when devices are added/removed.  These
  // manage the lifetime of VRBrowserRendererHostWin instances.
  static void AddCompositor(device::mojom::VRDisplayInfoPtr info,
                            device::mojom::XRCompositorHostPtr compositor);
  static void RemoveCompositor(device::mojom::XRDeviceId id);

 private:
  VRBrowserRendererHostWin(device::mojom::VRDisplayInfoPtr info,
                           device::mojom::XRCompositorHostPtr compositor);
  ~VRBrowserRendererHostWin() override;

  // Called by BrowserXRRuntime (we register to observe a BrowserXRDevice).
  // The parameter contents indicate which page is rendering with WebXR or WebVR
  // presentation.  When null, no page is presenting.
  void SetWebXRWebContents(content::WebContents* contents) override;

  void StartBrowserRenderer();
  void StopBrowserRenderer();

  device::mojom::XRCompositorHostPtr compositor_;
  std::unique_ptr<VRBrowserRendererThreadWin> render_thread_;
  device::mojom::VRDisplayInfoPtr info_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_RENDERLOOP_HOST_WIN_H_
