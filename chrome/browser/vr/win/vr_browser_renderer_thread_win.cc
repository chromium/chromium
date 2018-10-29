// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"

namespace vr {

VRBrowserRendererThreadWin::VRBrowserRendererThreadWin()
    : base::Thread("VRBrowserRenderThread") {}

VRBrowserRendererThreadWin::~VRBrowserRendererThreadWin() {
  Stop();
}

void VRBrowserRendererThreadWin::StartOverlay(
    device::mojom::XRCompositorHost* compositor) {
  device::mojom::ImmersiveOverlayPtrInfo overlay_info;
  compositor->CreateImmersiveOverlay(mojo::MakeRequest(&overlay_info));

  if (!IsRunning()) {
    if (!renderer_.InitializeOnMainThread()) {
      return;
    }

    Start();
  }

  // Post a task to the thread to start an overlay.
  task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VRBrowserRendererThreadWin::StartOverlayOnRenderThread,
                     base::Unretained(this), std::move(overlay_info)));
}

void VRBrowserRendererThreadWin::CleanUp() {
  renderer_.Cleanup();
  overlay_ = nullptr;
}

void VRBrowserRendererThreadWin::StartOverlayOnRenderThread(
    device::mojom::ImmersiveOverlayPtrInfo overlay) {
  overlay_.Bind(std::move(overlay));

  renderer_.InitializeOnGLThread();

  overlay_->SetOverlayAndWebXRVisibility(true, true);
  overlay_->RequestNextOverlayPose(base::BindOnce(
      &VRBrowserRendererThreadWin::OnPose, base::Unretained(this)));
}

void VRBrowserRendererThreadWin::OnPose(device::mojom::XRFrameDataPtr data) {
  renderer_.Render();
  overlay_->SubmitOverlayTexture(
      data->frame_id, renderer_.GetTexture(), renderer_.GetLeft(),
      renderer_.GetRight(),
      base::BindOnce(&VRBrowserRendererThreadWin::SubmitResult,
                     base::Unretained(this)));
}

void VRBrowserRendererThreadWin::SubmitResult(bool success) {
  if (!success) {
    renderer_.ResetMemoryBuffer();
  }
  overlay_->RequestNextOverlayPose(base::BindOnce(
      &VRBrowserRendererThreadWin::OnPose, base::Unretained(this)));
}

}  // namespace vr
