// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_
#define CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/vr_export.h"
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom-forward.h"

namespace gfx {
class Transform;
}

namespace vr {

class SchedulerBrowserRendererInterface;

// The SchedulerDelegate is responsible for starting the draw calls of the
// BrowserRenderer, given different signals, such as WebXR frames submitted or
// VSync events.
class VR_EXPORT SchedulerDelegate {
 public:
  virtual ~SchedulerDelegate() {}

  virtual void OnPause() = 0;
  virtual void OnResume() = 0;

  virtual void OnExitPresent() = 0;
  virtual void SetWebXrMode(bool enabled) = 0;
  virtual void SetShowingVrDialog(bool showing) = 0;
  virtual void SetBrowserRenderer(
      SchedulerBrowserRendererInterface* browser_renderer) = 0;
  virtual void SubmitDrawnFrame(FrameType frame_type,
                                const gfx::Transform& head_pose) = 0;
  virtual void AddInputSourceState(
      device::mojom::XRInputSourceStatePtr state) = 0;
  virtual void ConnectPresentingService(
      device::mojom::XRRuntimeSessionOptionsPtr options) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_SCHEDULER_DELEGATE_H_
