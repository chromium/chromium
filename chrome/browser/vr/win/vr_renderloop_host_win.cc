// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/win/vr_renderloop_host_win.h"
#include "chrome/browser/vr/service/browser_xr_runtime.h"
#include "chrome/browser/vr/service/xr_runtime_manager.h"
#include "chrome/browser/vr/win/vr_browser_renderer_thread_win.h"

namespace vr {

namespace {
VRBrowserRendererHostWin* g_vr_renderer_host = nullptr;
}

VRBrowserRendererHostWin::VRBrowserRendererHostWin(
    device::mojom::VRDisplayInfoPtr info,
    device::mojom::XRCompositorHostPtr compositor)
    : compositor_(std::move(compositor)), info_(std::move(info)) {
  BrowserXRRuntime* runtime =
      XRRuntimeManager::GetInstance()->GetRuntime(info_->id);
  if (runtime) {
    runtime->AddObserver(this);
  }
}

VRBrowserRendererHostWin::~VRBrowserRendererHostWin() {
  // We don't call BrowserXRRuntime::RemoveObserver, because if we are being
  // destroyed, it means the corresponding device has been removed from
  // XRRuntimeManager, and the BrowserXRRuntime has been destroyed.
}

void VRBrowserRendererHostWin::SetWebXRWebContents(
    content::WebContents* contents) {
  // Eventually the contents will be used to poll for permissions, or determine
  // what overlays should show.
  if (contents)
    StartBrowserRenderer();
  else
    StopBrowserRenderer();
}

void VRBrowserRendererHostWin::AddCompositor(
    device::mojom::VRDisplayInfoPtr info,
    device::mojom::XRCompositorHostPtr compositor) {
  // We only expect one device to be enabled at a time.
  DCHECK(!g_vr_renderer_host);
  g_vr_renderer_host =
      new VRBrowserRendererHostWin(std::move(info), std::move(compositor));
}

void VRBrowserRendererHostWin::RemoveCompositor(device::mojom::XRDeviceId id) {
  DCHECK(g_vr_renderer_host);
  g_vr_renderer_host->StopBrowserRenderer();
  delete g_vr_renderer_host;
  g_vr_renderer_host = nullptr;
}

void VRBrowserRendererHostWin::StartBrowserRenderer() {
// Only used for testing currently.  To see an example overlay, enable the
// following two lines.
#if 0
  render_thread_ = std::make_unique<VRBrowserRendererThreadWin>();
  render_thread_->StartOverlay(compositor_.get());
#endif
}

void VRBrowserRendererHostWin::StopBrowserRenderer() {
  render_thread_ = nullptr;
}

}  // namespace vr
