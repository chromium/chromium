// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_output_surface.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

namespace viz {

FakeSoftwareOutputSurface::FakeSoftwareOutputSurface(
    std::unique_ptr<SoftwareOutputDevice> software_device)
    : OutputSurface(std::move(software_device)) {
  DCHECK(OutputSurface::software_device());
}

FakeSoftwareOutputSurface::~FakeSoftwareOutputSurface() = default;

void FakeSoftwareOutputSurface::Reshape(const ReshapeParams& params) {
  software_device()->Resize(params.size, params.device_scale_factor);
  last_reshape_color_space_ = params.color_space;
}

void FakeSoftwareOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  last_sent_frame_ = std::make_unique<OutputSurfaceFrame>(std::move(frame));
  ++num_sent_frames_;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FakeSoftwareOutputSurface::SwapBuffersAck,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeSoftwareOutputSurface::SwapBuffersAck() {
  base::TimeTicks now = base::TimeTicks::Now();

  gpu::SwapBuffersCompleteParams params;
  params.swap_response.timings = {now, now};
  params.swap_response.result = gfx::SwapResult::SWAP_ACK;
  client_->DidReceiveSwapBuffersAck(params,
                                    /*release_fence=*/gfx::GpuFenceHandle());
  client_->DidReceivePresentationFeedback({now, base::TimeDelta(), 0});
}

void FakeSoftwareOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void FakeSoftwareOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {}

void FakeSoftwareOutputSurface::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  if (support_display_transform_hint_)
    display_transform_hint_ = transform;
}

gfx::OverlayTransform FakeSoftwareOutputSurface::GetDisplayTransform() {
  return support_display_transform_hint_ ? display_transform_hint_
                                         : gfx::OVERLAY_TRANSFORM_NONE;
}

// TODO(crbug.com/40118868): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
void FakeSoftwareOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {}
#endif

}  // namespace viz
