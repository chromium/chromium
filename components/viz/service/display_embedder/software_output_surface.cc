// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/software_output_device.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/latency/latency_info.h"

namespace viz {

SoftwareOutputSurface::SoftwareOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> device)
    : OutputSurface(std::move(device)) {
  capabilities_.pending_swap_params.max_pending_swaps =
      software_device()->MaxFramesPending();
  capabilities_.resize_based_on_root_surface =
      software_device()->SupportsOverridePlatformSize();
}

SoftwareOutputSurface::~SoftwareOutputSurface() = default;

void SoftwareOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SoftwareOutputSurface::EnsureBackbuffer() {
  software_device()->EnsureBackbuffer();
}

void SoftwareOutputSurface::DiscardBackbuffer() {
  software_device()->DiscardBackbuffer();
}

void SoftwareOutputSurface::Reshape(const ReshapeParams& params) {
  software_device()->Resize(params.size, params.device_scale_factor);
}

void SoftwareOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK(client_);
  base::TimeTicks swap_time = base::TimeTicks::Now();
  for (auto& latency : frame.latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, swap_time);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, swap_time);
  }

  stored_latency_info_.emplace(std::move(frame.latency_info));

  software_device()->OnSwapBuffers(
      base::BindOnce(&SoftwareOutputSurface::SwapBuffersCallback,
                     weak_factory_.GetWeakPtr(), swap_time),
      frame.data);

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider && update_vsync_parameters_callback_) {
    vsync_provider->GetVSyncParameters(
        base::BindOnce(&SoftwareOutputSurface::UpdateVSyncParameters,
                       weak_factory_.GetWeakPtr()));
  }
}

bool SoftwareOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

void SoftwareOutputSurface::SwapBuffersCallback(base::TimeTicks swap_time,
                                                const gfx::Size& pixel_size) {
  latency_tracker_.OnGpuSwapBuffersCompleted(
      std::move(stored_latency_info_.front()));
  stored_latency_info_.pop();
  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = {swap_time, swap_time};
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  client_->DidReceiveSwapBuffersAck(params,
                                    /*release_fence=*/gfx::GpuFenceHandle());

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval_to_next_refresh =
      now.SnappedToNextTick(refresh_timebase_, refresh_interval_) - now;
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  if (needs_swap_size_notifications_)
    client_->DidSwapWithSize(pixel_size);
#endif
  client_->DidReceivePresentationFeedback(
      gfx::PresentationFeedback(now, interval_to_next_refresh, 0u));
}

void SoftwareOutputSurface::UpdateVSyncParameters(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {
  DCHECK(update_vsync_parameters_callback_);
  refresh_timebase_ = timebase;
  refresh_interval_ = interval;
  update_vsync_parameters_callback_.Run(timebase, interval);
}

void SoftwareOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

gfx::OverlayTransform SoftwareOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void SoftwareOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}
#endif
}  // namespace viz
