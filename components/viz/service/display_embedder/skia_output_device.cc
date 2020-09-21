// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/latency/latency_tracker.h"

namespace viz {
namespace {

scoped_refptr<base::SequencedTaskRunner> CreateLatencyTracerRunner() {
  if (!base::ThreadPoolInstance::Get())
    return nullptr;
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
}

void ReportLatency(const gfx::SwapTimings& timings,
                   ui::LatencyTracker* tracker,
                   std::vector<ui::LatencyInfo> latency_info) {
  for (auto& latency : latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, timings.swap_end);
  }
  tracker->OnGpuSwapBuffersCompleted(std::move(latency_info));
}

}  // namespace

SkiaOutputDevice::ScopedPaint::ScopedPaint(SkiaOutputDevice* device)
    : device_(device), sk_surface_(device->BeginPaint(&end_semaphores_)) {
  DCHECK(sk_surface_);
}
SkiaOutputDevice::ScopedPaint::~ScopedPaint() {
  DCHECK(end_semaphores_.empty());
  device_->EndPaint();
}

SkiaOutputDevice::SkiaOutputDevice(
    GrDirectContext* gr_context,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      memory_type_tracker_(
          std::make_unique<gpu::MemoryTypeTracker>(memory_tracker)),
      latency_tracker_(std::make_unique<ui::LatencyTracker>()),
      latency_tracker_runner_(CreateLatencyTracerRunner()) {
  DCHECK(gr_context);
  capabilities_.max_render_target_size = gr_context->maxRenderTargetSize();
}

SkiaOutputDevice::~SkiaOutputDevice() {
  if (latency_tracker_runner_)
    latency_tracker_runner_->DeleteSoon(FROM_HERE, std::move(latency_tracker_));
}

void SkiaOutputDevice::PreGrContextSubmit() {}

void SkiaOutputDevice::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  NOTREACHED();
}

void SkiaOutputDevice::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  NOTREACHED();
}

bool SkiaOutputDevice::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  NOTREACHED();
  return false;
}

void SkiaOutputDevice::SetEnableDCLayers(bool enable) {
  NOTREACHED();
}

void SkiaOutputDevice::SetGpuVSyncEnabled(bool enabled) {
  NOTREACHED();
}

bool SkiaOutputDevice::IsPrimaryPlaneOverlay() const {
  return false;
}

void SkiaOutputDevice::SchedulePrimaryPlane(
    const base::Optional<OverlayProcessorInterface::OutputSurfaceOverlayPlane>&
        plane) {
  if (plane)
    NOTIMPLEMENTED();
}

void SkiaOutputDevice::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  NOTIMPLEMENTED();
}

void SkiaOutputDevice::EnsureBackbuffer() {}
void SkiaOutputDevice::DiscardBackbuffer() {}

void SkiaOutputDevice::SetDrawTimings(base::TimeTicks submitted,
                                      base::TimeTicks started) {
  viz_scheduled_draw_ = submitted;
  gpu_started_draw_ = started;
}

void SkiaOutputDevice::StartSwapBuffers(BufferPresentedCallback feedback) {
  DCHECK_LT(static_cast<int>(pending_swaps_.size()),
            capabilities_.max_frames_pending);

  pending_swaps_.emplace(++swap_id_, std::move(feedback), viz_scheduled_draw_,
                         gpu_started_draw_);
  viz_scheduled_draw_ = base::TimeTicks();
  gpu_started_draw_ = base::TimeTicks();
}

void SkiaOutputDevice::FinishSwapBuffers(
    gfx::SwapCompletionResult result,
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    const base::Optional<gfx::Rect>& damage_area,
    std::vector<gpu::Mailbox> released_overlays) {
  DCHECK(!pending_swaps_.empty());

  const gpu::SwapBuffersCompleteParams& params =
      pending_swaps_.front().Complete(std::move(result), damage_area,
                                      std::move(released_overlays));

  did_swap_buffer_complete_callback_.Run(params, size);

  pending_swaps_.front().CallFeedback();

  if (latency_tracker_runner_) {
    // Report latency off GPU main thread.
    latency_tracker_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ReportLatency, params.swap_response.timings,
                       latency_tracker_.get(), std::move(latency_info)));
  } else {
    ReportLatency(params.swap_response.timings, latency_tracker_.get(),
                  std::move(latency_info));
  }

  pending_swaps_.pop();
}

SkiaOutputDevice::SwapInfo::SwapInfo(
    uint64_t swap_id,
    SkiaOutputDevice::BufferPresentedCallback feedback,
    base::TimeTicks viz_scheduled_draw,
    base::TimeTicks gpu_started_draw)
    : feedback_(std::move(feedback)) {
  params_.swap_response.swap_id = swap_id;
  params_.swap_response.timings.swap_start = base::TimeTicks::Now();
  params_.swap_response.timings.viz_scheduled_draw = viz_scheduled_draw;
  params_.swap_response.timings.gpu_started_draw = gpu_started_draw;
}

SkiaOutputDevice::SwapInfo::SwapInfo(SwapInfo&& other) = default;

SkiaOutputDevice::SwapInfo::~SwapInfo() = default;

const gpu::SwapBuffersCompleteParams& SkiaOutputDevice::SwapInfo::Complete(
    gfx::SwapCompletionResult result,
    const base::Optional<gfx::Rect>& damage_rect,
    std::vector<gpu::Mailbox> released_overlays) {
  params_.swap_response.result = result.swap_result;
  params_.swap_response.timings.swap_end = base::TimeTicks::Now();
  params_.frame_buffer_damage_area = damage_rect;
  if (result.ca_layer_params)
    params_.ca_layer_params = *result.ca_layer_params;

  params_.released_overlays = std::move(released_overlays);
  return params_;
}

void SkiaOutputDevice::SwapInfo::CallFeedback() {
  if (feedback_) {
    uint32_t flags = 0;
    if (params_.swap_response.result != gfx::SwapResult::SWAP_ACK)
      flags = gfx::PresentationFeedback::Flags::kFailure;

    std::move(feedback_).Run(
        gfx::PresentationFeedback(params_.swap_response.timings.swap_start,
                                  /*interval=*/base::TimeDelta(), flags));
  }
}

}  // namespace viz
