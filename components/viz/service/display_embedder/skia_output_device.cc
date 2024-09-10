// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "services/tracing/public/cpp/perfetto/flow_event_utils.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "third_party/skia/include/gpu/graphite/Context.h"
#include "third_party/skia/include/gpu/graphite/Recording.h"
#include "third_party/skia/include/private/chromium/GrDeferredDisplayList.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"

#if BUILDFLAG(IS_WIN)
#include "components/viz/service/display/dc_layer_overlay.h"
#endif

namespace viz {
namespace {

void ReportLatency(const gfx::SwapTimings& timings,
                   std::vector<ui::LatencyInfo> latency_info) {
  for (auto& latency : latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT, timings.swap_end);
  }
}

}  // namespace

SkiaOutputDevice::ScopedPaint::ScopedPaint(
    std::vector<GrBackendSemaphore> end_semaphores,
    SkiaOutputDevice* device,
    SkSurface* sk_surface)
    : end_semaphores_(std::move(end_semaphores)),
      device_(device),
      sk_surface_(sk_surface) {}

SkiaOutputDevice::ScopedPaint::~ScopedPaint() {
  DCHECK(end_semaphores_.empty());
  device_->EndPaint();
}

SkCanvas* SkiaOutputDevice::ScopedPaint::GetCanvas() {
  return device_->GetCanvas(sk_surface_);
}

GrSemaphoresSubmitted SkiaOutputDevice::ScopedPaint::Flush(
    VulkanContextProvider* vulkan_context_provider,
    std::vector<GrBackendSemaphore> end_semaphores,
    base::OnceClosure on_finished) {
  return device_->Flush(sk_surface_, vulkan_context_provider,
                        std::move(end_semaphores), std::move(on_finished));
}

bool SkiaOutputDevice::ScopedPaint::Wait(
    int num_semaphores,
    const GrBackendSemaphore wait_semaphores[],
    bool delete_semaphores_after_wait) {
  return device_->Wait(sk_surface_, num_semaphores, wait_semaphores,
                       delete_semaphores_after_wait);
}

bool SkiaOutputDevice::ScopedPaint::Draw(
    sk_sp<const GrDeferredDisplayList> ddl) {
  return device_->Draw(sk_surface_, std::move(ddl));
}

bool SkiaOutputDevice::ScopedPaint::Draw(
    std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
    base::OnceClosure on_finished) {
  return device_->Draw(sk_surface_, std::move(graphite_recording),
                       std::move(on_finished));
}

SkiaOutputDevice::SkiaOutputDevice(
    GrDirectContext* gr_context,
    skgpu::graphite::Context* graphite_context,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback,
    ReleaseOverlaysCallback release_overlays_callback)
    : gr_context_(gr_context),
      graphite_context_(graphite_context),
      did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      release_overlays_callback_(std::move(release_overlays_callback)),
      memory_type_tracker_(
          std::make_unique<gpu::MemoryTypeTracker>(memory_tracker)) {
  if (gr_context_) {
    CHECK(!graphite_context_);
    capabilities_.max_render_target_size = gr_context->maxRenderTargetSize();
    capabilities_.max_texture_size = gr_context->maxTextureSize();
  } else {
    CHECK(graphite_context_);
    capabilities_.max_render_target_size = graphite_context->maxTextureSize();
    capabilities_.max_texture_size = graphite_context->maxTextureSize();
  }
}

SkiaOutputDevice::~SkiaOutputDevice() = default;

std::unique_ptr<SkiaOutputDevice::ScopedPaint>
SkiaOutputDevice::BeginScopedPaint() {
  std::vector<GrBackendSemaphore> end_semaphores;
  SkSurface* sk_surface = BeginPaint(&end_semaphores);
  if (!sk_surface) {
    return nullptr;
  }
  return std::make_unique<SkiaOutputDevice::ScopedPaint>(
      std::move(end_semaphores), this, sk_surface);
}

void SkiaOutputDevice::SetViewportSize(const gfx::Size& viewport_size) {}

void SkiaOutputDevice::Submit(bool sync_cpu, base::OnceClosure callback) {
  if (gr_context_) {
    gr_context_->submit(sync_cpu ? GrSyncCpu::kYes : GrSyncCpu::kNo);
  } else {
    CHECK(graphite_context_);
    graphite_context_->submit(sync_cpu ? skgpu::graphite::SyncToCpu::kYes
                                       : skgpu::graphite::SyncToCpu::kNo);
  }
  std::move(callback).Run();
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

void SkiaOutputDevice::SetDependencyTimings(base::TimeTicks task_ready) {
  gpu_task_ready_ = task_ready;
}

void SkiaOutputDevice::ReadbackForTesting(
    base::OnceCallback<void(SkBitmap)> callback) {
  NOTIMPLEMENTED();
}

void SkiaOutputDevice::StartSwapBuffers(BufferPresentedCallback feedback) {
  DCHECK_LT(static_cast<int>(pending_swaps_.size()),
            capabilities_.pending_swap_params.GetMax());

  pending_swaps_.emplace(++swap_id_, std::move(feedback), viz_scheduled_draw_,
                         gpu_started_draw_, gpu_task_ready_);
  viz_scheduled_draw_ = base::TimeTicks();
  gpu_started_draw_ = base::TimeTicks();
  gpu_task_ready_ = base::TimeTicks();
}

void SkiaOutputDevice::FinishSwapBuffers(
    gfx::SwapCompletionResult result,
    const gfx::Size& size,
    OutputSurfaceFrame frame,
    const std::optional<gfx::Rect>& damage_area,
    std::vector<gpu::Mailbox> released_overlays) {
  DCHECK(!pending_swaps_.empty());

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(frame.data.swap_trace_id),
      [swap_trace_id = frame.data.swap_trace_id](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_FINISH_BUFFER_SWAP);
        data->set_display_trace_id(swap_trace_id);
      });

  auto release_fence = std::move(result.release_fence);
  const gpu::SwapBuffersCompleteParams& params =
      pending_swaps_.front().Complete(std::move(result), damage_area,
                                      std::move(released_overlays),
                                      frame.data.swap_trace_id);

  did_swap_buffer_complete_callback_.Run(params, size,
                                         std::move(release_fence));

  pending_swaps_.front().CallFeedback();

  ReportLatency(params.swap_response.timings, std::move(frame.latency_info));

  pending_swaps_.pop();

  // If there are skipped swaps at the front of the queue, they are now ready
  // to be acknowledged without breaking ordering.
  if (!pending_swaps_.empty()) {
    auto iter = skipped_swap_info_.find(pending_swaps_.front().SwapId());
    if (iter != skipped_swap_info_.end()) {
      OutputSurfaceFrame output_frame = std::move(iter->second);
      gfx::Size frame_size = output_frame.size;
      skipped_swap_info_.erase(iter);
      // Recursively call into FinishSwapBuffers until the head of the queue is
      // no longer a skipped swap.
      FinishSwapBuffers(
          gfx::SwapCompletionResult(gfx::SwapResult::SWAP_SKIPPED), frame_size,
          std::move(output_frame));
    }
  }
}

void SkiaOutputDevice::SwapBuffersSkipped(BufferPresentedCallback feedback,
                                          OutputSurfaceFrame frame) {
  StartSwapBuffers(std::move(feedback));
  // If there are no other pending swaps, we can immediately close out the
  // "skipped" swap that was just enqueued. If there are outstanding pending
  // swaps, however, we need to wait for them to complete to avoid reordering
  // complete/presentation callbacks.
  if (pending_swaps_.size() == 1) {
    gfx::Size frame_size = frame.size;
    FinishSwapBuffers(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_SKIPPED),
                      frame_size, std::move(frame));
  } else {
    skipped_swap_info_[swap_id_] = std::move(frame);
  }
}

SkiaOutputDevice::SwapInfo::SwapInfo(
    uint64_t swap_id,
    SkiaOutputDevice::BufferPresentedCallback feedback,
    base::TimeTicks viz_scheduled_draw,
    base::TimeTicks gpu_started_draw,
    base::TimeTicks gpu_task_ready)
    : feedback_(std::move(feedback)) {
  params_.swap_response.swap_id = swap_id;
  params_.swap_response.timings.swap_start = base::TimeTicks::Now();
  params_.swap_response.timings.viz_scheduled_draw = viz_scheduled_draw;
  params_.swap_response.timings.gpu_started_draw = gpu_started_draw;
  params_.swap_response.timings.gpu_task_ready = gpu_task_ready;
}

SkiaOutputDevice::SwapInfo::SwapInfo(SwapInfo&& other) = default;

SkiaOutputDevice::SwapInfo::~SwapInfo() = default;

uint64_t SkiaOutputDevice::SwapInfo::SwapId() {
  return params_.swap_response.swap_id;
}

const gpu::SwapBuffersCompleteParams& SkiaOutputDevice::SwapInfo::Complete(
    gfx::SwapCompletionResult result,
    const std::optional<gfx::Rect>& damage_rect,
    std::vector<gpu::Mailbox> released_overlays,
    int64_t swap_trace_id) {
  params_.swap_response.result = result.swap_result;
  params_.swap_response.timings.swap_end = base::TimeTicks::Now();
  params_.frame_buffer_damage_area = damage_rect;
  if (result.ca_layer_params)
    params_.ca_layer_params = *result.ca_layer_params;

  params_.released_overlays = std::move(released_overlays);

  params_.swap_trace_id = swap_trace_id;
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

SkCanvas* SkiaOutputDevice::GetCanvas(SkSurface* sk_surface) {
  return sk_surface->getCanvas();
}

GrSemaphoresSubmitted SkiaOutputDevice::Flush(
    SkSurface* sk_surface,
    VulkanContextProvider* vulkan_context_provider,
    std::vector<GrBackendSemaphore> end_semaphores,
    base::OnceClosure on_finished) {
  CHECK(sk_surface);
  GrFlushInfo flush_info = {
      .fNumSemaphores = end_semaphores.size(),
      .fSignalSemaphores = end_semaphores.data(),
  };
  gpu::AddVulkanCleanupTaskForSkiaFlush(vulkan_context_provider, &flush_info);
  if (on_finished)
    gpu::AddCleanupTaskForSkiaFlush(std::move(on_finished), &flush_info);
  if (GrDirectContext* direct_context =
          GrAsDirectContext(sk_surface->recordingContext())) {
    return direct_context->flush(sk_surface, flush_info);
  }
  return {};
}

bool SkiaOutputDevice::Wait(SkSurface* sk_surface,
                            int num_semaphores,
                            const GrBackendSemaphore wait_semaphores[],
                            bool delete_semaphores_after_wait) {
  return sk_surface->wait(num_semaphores, wait_semaphores,
                          delete_semaphores_after_wait);
}

bool SkiaOutputDevice::Draw(SkSurface* sk_surface,
                            sk_sp<const GrDeferredDisplayList> ddl) {
#if DCHECK_IS_ON()
  const auto& characterization = ddl->characterization();
  if (!sk_surface->isCompatible(characterization)) {
    GrSurfaceCharacterization surface_characterization;
    DCHECK(sk_surface->characterize(&surface_characterization));
#define CHECK_PROPERTY(name) \
  DCHECK_EQ(characterization.name(), surface_characterization.name());
    CHECK_PROPERTY(cacheMaxResourceBytes);
    CHECK_PROPERTY(origin);
    CHECK_PROPERTY(width);
    CHECK_PROPERTY(height);
    CHECK_PROPERTY(colorType);
    CHECK_PROPERTY(sampleCount);
    CHECK_PROPERTY(isTextureable);
    CHECK_PROPERTY(isMipMapped);
    CHECK_PROPERTY(usesGLFBO0);
    CHECK_PROPERTY(vkRTSupportsInputAttachment);
    CHECK_PROPERTY(vulkanSecondaryCBCompatible);
    CHECK_PROPERTY(isProtected);
  }
#endif
  return skgpu::ganesh::DrawDDL(sk_surface, ddl);
}

bool SkiaOutputDevice::Draw(
    SkSurface* sk_surface,
    std::unique_ptr<skgpu::graphite::Recording> graphite_recording,
    base::OnceClosure on_finished) {
  CHECK(sk_surface);
  CHECK(graphite_recording);
  CHECK(graphite_context_);
  skgpu::graphite::InsertRecordingInfo info;
  info.fRecording = graphite_recording.get();
  info.fTargetSurface = sk_surface;
  if (on_finished) {
    gpu::AddCleanupTaskForGraphiteRecording(std::move(on_finished), &info);
  }
  return graphite_context_->insertRecording(info);
}

}  // namespace viz
