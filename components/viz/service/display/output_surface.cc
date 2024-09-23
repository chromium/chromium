// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/output_surface.h"

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/swap_result.h"

namespace viz {

OutputSurface::Capabilities::Capabilities() = default;
OutputSurface::Capabilities::~Capabilities() = default;
OutputSurface::Capabilities::Capabilities(const Capabilities& capabilities) =
    default;
OutputSurface::Capabilities& OutputSurface::Capabilities::operator=(
    const Capabilities& capabilities) = default;

OutputSurface::OutputSurface() : type_(Type::kSkia) {}

OutputSurface::OutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : type_(Type::kSoftware), software_device_(std::move(software_device)) {
  DCHECK(software_device_);
}

OutputSurface::~OutputSurface() = default;

gfx::Rect OutputSurface::GetCurrentFramebufferDamage() const {
  return gfx::Rect();
}

SkiaOutputSurface* OutputSurface::AsSkiaOutputSurface() {
  return nullptr;
}

gpu::SurfaceHandle OutputSurface::GetSurfaceHandle() const {
  return gpu::kNullSurfaceHandle;
}

void OutputSurface::UpdateLatencyInfoOnSwap(
    const gfx::SwapResponse& response,
    std::vector<ui::LatencyInfo>* latency_info) {
  for (auto& latency : *latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, response.timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
        response.timings.swap_end);
  }
}

void OutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  DCHECK(!needs_swap_size_notifications);
}

#if BUILDFLAG(IS_ANDROID)
base::ScopedClosureRunner OutputSurface::GetCacheBackBufferCb() {
  return base::ScopedClosureRunner();
}
#endif

void OutputSurface::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  NOTREACHED_IN_MIGRATION();
}

void OutputSurface::ReadbackForTesting(
    CopyOutputRequest::CopyOutputRequestCallback result_callback) {
  NOTIMPLEMENTED();
}

}  // namespace viz
