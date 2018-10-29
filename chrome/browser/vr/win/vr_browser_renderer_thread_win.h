// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
#define CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_

#include "base/threading/thread.h"
#include "content/public/browser/web_contents.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"

#include "chrome/browser/vr/win/simple_overlay_renderer_win.h"

#include "chrome/browser/vr/service/browser_xr_runtime.h"

namespace vr {

class VRBrowserRendererThreadWin : base::Thread {
 public:
  VRBrowserRendererThreadWin();
  ~VRBrowserRendererThreadWin() override;

  // base::Thread overrides
  void CleanUp() override;

  // Initially we are just rendering a solid-color rectangle overlay as a
  // proof-of-concept.  Eventually, this will draw real content.
  void StartOverlay(device::mojom::XRCompositorHost* host);

 private:
  void StartOverlayOnRenderThread(
      device::mojom::ImmersiveOverlayPtrInfo overlay);
  void OnPose(device::mojom::XRFrameDataPtr data);
  void SubmitResult(bool success);

  SimpleOverlayRenderer renderer_;
  device::mojom::ImmersiveOverlayPtr overlay_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_WIN_VR_BROWSER_RENDERER_THREAD_WIN_H_
