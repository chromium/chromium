// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_display_client.h"

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/accelerated_widget_mac/ca_layer_frame_sink.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <utility>

#include "components/viz/common/display/use_layered_window.h"
#include "components/viz/host/layered_window_updater_impl.h"
#include "ui/base/win/internal_constants.h"
#endif

namespace viz {

HostDisplayClient::HostDisplayClient(gfx::AcceleratedWidget widget) {
#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
  widget_ = widget;
#endif
}

HostDisplayClient::~HostDisplayClient() = default;

mojo::PendingRemote<mojom::DisplayClient> HostDisplayClient::GetBoundRemote(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return receiver_.BindNewPipeAndPassRemote(task_runner);
}

#if BUILDFLAG(IS_APPLE)
void HostDisplayClient::OnDisplayReceivedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  ui::CALayerFrameSink* ca_layer_frame_sink =
      ui::CALayerFrameSink::FromAcceleratedWidget(widget_);
  if (ca_layer_frame_sink)
    ca_layer_frame_sink->UpdateCALayerTree(ca_layer_params);
  else
    DLOG(WARNING) << "Received frame for non-existent widget.";
}
#endif

#if BUILDFLAG(IS_WIN)
void HostDisplayClient::CreateLayeredWindowUpdater(
    mojo::PendingReceiver<mojom::LayeredWindowUpdater> receiver) {
  if (!NeedsToUseLayerWindow(widget_)) {
    DLOG(ERROR) << "HWND shouldn't be using a layered window";
    return;
  }

  layered_window_updater_ =
      std::make_unique<LayeredWindowUpdaterImpl>(widget_, std::move(receiver));
}
void HostDisplayClient::AddChildWindowToBrowser(
    gpu::SurfaceHandle child_window) {
  NOTREACHED_IN_MIGRATION();
}
#endif

#if BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)
void HostDisplayClient::DidCompleteSwapWithNewSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_X11)

#if BUILDFLAG(IS_CHROMEOS_ASH)
void HostDisplayClient::SetPreferredRefreshRate(float refresh_rate) {
  NOTREACHED();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace viz
