// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_surface.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
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

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(frame.data.swap_trace_id),
      [swap_trace_id = frame.data.swap_trace_id](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_BUFFER_SWAP_POST_SUBMIT);
        data->set_display_trace_id(swap_trace_id);
      });

  software_device()->OnSwapBuffers(
      base::BindOnce(&SoftwareOutputSurface::SwapBuffersCallback,
                     weak_factory_.GetWeakPtr(), swap_time,
                     frame.data.swap_trace_id),
      frame.data);

  gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
  if (vsync_provider && update_vsync_parameters_callback_) {
    vsync_provider->GetVSyncParameters(
        base::BindOnce(&SoftwareOutputSurface::UpdateVSyncParameters,
                       weak_factory_.GetWeakPtr()));
  }
}

void SoftwareOutputSurface::SwapBuffersCallback(base::TimeTicks swap_time,
                                                int64_t swap_trace_id,
                                                const gfx::Size& pixel_size) {
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(swap_trace_id), [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_FINISH_BUFFER_SWAP);
        data->set_display_trace_id(swap_trace_id);
      });

  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = {swap_time, swap_time};
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  params.swap_trace_id = swap_trace_id;
  client_->DidReceiveSwapBuffersAck(params,
                                    /*release_fence=*/gfx::GpuFenceHandle());

  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta interval_to_next_refresh =
      now.SnappedToNextTick(refresh_timebase_, refresh_interval_) - now;
// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
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
  // We should not be receiving 0 intervals.
  refresh_interval_ =
      interval.is_zero() ? BeginFrameArgs::DefaultInterval() : interval;
  update_vsync_parameters_callback_.Run(timebase, refresh_interval_);
}

void SoftwareOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  update_vsync_parameters_callback_ = std::move(callback);
}

gfx::OverlayTransform SoftwareOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void SoftwareOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}
#endif
}  // namespace viz
