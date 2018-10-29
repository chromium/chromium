// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <stddef.h>
#include <limits>

#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/base/simple_enclosed_region.h"
#include "cc/benchmarks/benchmark_instrumentation.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/vulkan/buildflags.h"
#include "services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

namespace {

// Assign each Display instance a starting value for the the display-trace id,
// so that multiple Displays all don't start at 0, because that makes it
// difficult to associate the trace-events with the particular displays.
int64_t GetStartingTraceId() {
  static int64_t client = 0;
  return ((++client & 0xffffffff) << 32);
}

}  // namespace

Display::Display(
    SharedBitmapManager* bitmap_manager,
    const RendererSettings& settings,
    const FrameSinkId& frame_sink_id,
    std::unique_ptr<OutputSurface> output_surface,
    std::unique_ptr<DisplayScheduler> scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> current_task_runner,
    SkiaOutputSurface* skia_output_surface)
    : bitmap_manager_(bitmap_manager),
      settings_(settings),
      frame_sink_id_(frame_sink_id),
      skia_output_surface_(skia_output_surface),
      output_surface_(std::move(output_surface)),
      scheduler_(std::move(scheduler)),
      current_task_runner_(std::move(current_task_runner)),
      swapped_trace_id_(GetStartingTraceId()),
      last_acked_trace_id_(swapped_trace_id_) {
  DCHECK(output_surface_);
  DCHECK(frame_sink_id_.is_valid());
  if (scheduler_)
    scheduler_->SetClient(this);
}

Display::~Display() {
  for (auto& observer : observers_)
    observer.OnDisplayDestroyed();
  observers_.Clear();

  for (auto& callback_list : pending_presented_callbacks_) {
    for (auto& callback : callback_list.second)
      std::move(callback).Run(gfx::PresentationFeedback::Failure());
  }

  // Only do this if Initialize() happened.
  if (client_) {
    if (auto* context = output_surface_->context_provider())
      context->RemoveObserver(this);
    if (scheduler_)
      surface_manager_->RemoveObserver(scheduler_.get());
  }
  if (aggregator_) {
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      if (surface)
        surface->RunDrawCallback();
    }
  }
}

void Display::Initialize(DisplayClient* client,
                         SurfaceManager* surface_manager) {
  DCHECK(client);
  DCHECK(surface_manager);
  client_ = client;
  surface_manager_ = surface_manager;
  if (scheduler_)
    surface_manager_->AddObserver(scheduler_.get());

  output_surface_->BindToClient(this);
  if (output_surface_->software_device())
    output_surface_->software_device()->BindToClient(this);

  InitializeRenderer();

  // This depends on assumptions that Display::Initialize will happen on the
  // same callstack as the ContextProvider being created/initialized or else
  // it could miss a callback before setting this.
  if (auto* context = output_surface_->context_provider())
    context->AddObserver(this);
}

void Display::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void Display::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void Display::SetLocalSurfaceId(const LocalSurfaceId& id,
                                float device_scale_factor) {
  if (current_surface_id_.local_surface_id() == id &&
      device_scale_factor_ == device_scale_factor) {
    return;
  }

  TRACE_EVENT0("viz", "Display::SetSurfaceId");
  current_surface_id_ = SurfaceId(frame_sink_id_, id);
  device_scale_factor_ = device_scale_factor;

  UpdateRootFrameMissing();
  if (scheduler_)
    scheduler_->SetNewRootSurface(current_surface_id_);
}

void Display::SetVisible(bool visible) {
  TRACE_EVENT1("viz", "Display::SetVisible", "visible", visible);
  if (renderer_)
    renderer_->SetVisible(visible);
  if (scheduler_)
    scheduler_->SetVisible(visible);
  visible_ = visible;

  if (!visible) {
    // Damage tracker needs a full reset as renderer resources are dropped when
    // not visible.
    if (aggregator_ && current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::Resize(const gfx::Size& size) {
  if (size == current_surface_size_)
    return;

  TRACE_EVENT0("viz", "Display::Resize");

  // Need to ensure all pending swaps have executed before the window is
  // resized, or D3D11 will scale the swap output.
  if (settings_.finish_rendering_on_resize) {
    if (!swapped_since_resize_ && scheduler_)
      scheduler_->ForceImmediateSwapIfPossible();
    if (swapped_since_resize_ && output_surface_ &&
        output_surface_->context_provider())
      output_surface_->context_provider()->ContextGL()->ShallowFinishCHROMIUM();
  }
  swapped_since_resize_ = false;
  current_surface_size_ = size;
  if (scheduler_)
    scheduler_->DisplayResized();
}

void Display::SetColorMatrix(const SkMatrix44& matrix) {
  if (output_surface_)
    output_surface_->set_color_matrix(matrix);

  // Force a redraw.
  if (aggregator_) {
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }

  if (scheduler_) {
    BeginFrameAck ack;
    ack.has_damage = true;
    scheduler_->ProcessSurfaceDamage(current_surface_id_, ack, true);
  }
}

void Display::SetColorSpace(const gfx::ColorSpace& blending_color_space,
                            const gfx::ColorSpace& device_color_space) {
  blending_color_space_ = blending_color_space;
  device_color_space_ = device_color_space;
  if (aggregator_) {
    aggregator_->SetOutputColorSpace(blending_color_space, device_color_space_);
  }
}

void Display::SetOutputIsSecure(bool secure) {
  if (secure == output_is_secure_)
    return;
  output_is_secure_ = secure;

  if (aggregator_) {
    aggregator_->set_output_is_secure(secure);
    // Force a redraw.
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }
}

void Display::InitializeRenderer() {
  auto mode = output_surface_->context_provider() || skia_output_surface_
                  ? DisplayResourceProvider::kGpu
                  : DisplayResourceProvider::kSoftware;
  resource_provider_ = std::make_unique<DisplayResourceProvider>(
      mode, output_surface_->context_provider(), bitmap_manager_);

  if (settings_.use_skia_renderer && mode == DisplayResourceProvider::kGpu) {
    // Default to use DDL if skia_output_surface is not null.
    if (skia_output_surface_) {
      renderer_ = std::make_unique<SkiaRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get(),
          skia_output_surface_, SkiaRenderer::DrawMode::DDL);
    } else {
      // GPU compositing with GL.
      DCHECK(output_surface_);
      DCHECK(output_surface_->context_provider());
      SkiaRenderer::DrawMode mode = settings_.record_sk_picture
                                        ? SkiaRenderer::DrawMode::SKPRECORD
                                        : SkiaRenderer::DrawMode::GL;
      renderer_ = std::make_unique<SkiaRenderer>(
          &settings_, output_surface_.get(), resource_provider_.get(),
          nullptr /* skia_output_surface */, mode);
    }
  } else if (output_surface_->context_provider()) {
    renderer_ = std::make_unique<GLRenderer>(&settings_, output_surface_.get(),
                                             resource_provider_.get(),
                                             current_task_runner_);
#if BUILDFLAG(ENABLE_VULKAN)
  } else if (output_surface_->vulkan_context_provider()) {
    renderer_ = std::make_unique<SkiaRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get(),
        nullptr /* skia_output_surface */, SkiaRenderer::DrawMode::VULKAN);
#endif
  } else {
    auto renderer = std::make_unique<SoftwareRenderer>(
        &settings_, output_surface_.get(), resource_provider_.get());
    software_renderer_ = renderer.get();
    renderer_ = std::move(renderer);
  }

  renderer_->Initialize();
  renderer_->SetVisible(visible_);

  // TODO(jbauman): Outputting an incomplete quad list doesn't work when using
  // overlays.
  bool output_partial_list = renderer_->use_partial_swap() &&
                             !output_surface_->GetOverlayCandidateValidator();
  aggregator_.reset(new SurfaceAggregator(
      surface_manager_, resource_provider_.get(), output_partial_list));
  aggregator_->set_output_is_secure(output_is_secure_);
  aggregator_->SetOutputColorSpace(blending_color_space_, device_color_space_);
}

void Display::UpdateRootFrameMissing() {
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  bool root_frame_missing = !surface || !surface->HasActiveFrame();
  if (scheduler_)
    scheduler_->SetRootFrameMissing(root_frame_missing);
}

void Display::OnContextLost() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

bool Display::DrawAndSwap() {
  TRACE_EVENT0("viz", "Display::DrawAndSwap");

  if (!current_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0("viz", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!output_surface_) {
    TRACE_EVENT_INSTANT0("viz", "No output surface", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  // During aggregation, SurfaceAggregator marks all resources used for a draw
  // in the resource provider.  This has the side effect of deleting unused
  // resources and their textures, generating sync tokens, and returning the
  // resources to the client.  This involves GL work which is issued before
  // drawing commands, and gets prioritized by GPU scheduler because sync token
  // dependencies aren't issued until the draw.
  //
  // Batch and defer returning resources in resource provider.  This defers the
  // GL commands for deleting resources to after the draw, and prevents context
  // switching because the scheduler knows sync token dependencies at that time.
  DisplayResourceProvider::ScopedBatchReturnResources returner(
      resource_provider_.get());
  base::ElapsedTimer aggregate_timer;
  const base::TimeTicks now_time = aggregate_timer.Begin();
  CompositorFrame frame = aggregator_->Aggregate(
      current_surface_id_,
      scheduler_ ? scheduler_->current_frame_display_time() : now_time,
      ++swapped_trace_id_);
  UMA_HISTOGRAM_COUNTS_1M("Compositing.SurfaceAggregator.AggregateUs",
                          aggregate_timer.Elapsed().InMicroseconds());

  if (frame.render_pass_list.empty()) {
    TRACE_EVENT_INSTANT0("viz", "Empty aggregated frame.",
                         TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  TRACE_EVENT_ASYNC_BEGIN0("viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
                           swapped_trace_id_);

  // Run callbacks early to allow pipelining and collect presented callbacks.
  for (const auto& surface_id : surfaces_to_ack_on_next_draw_) {
    Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
    if (surface)
      surface->RunDrawCallback();
  }
  surfaces_to_ack_on_next_draw_.clear();

  frame.metadata.latency_info.insert(frame.metadata.latency_info.end(),
                                     stored_latency_info_.begin(),
                                     stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = false;
  size_t total_quad_count = 0;
  for (const auto& pass : frame.render_pass_list) {
    have_copy_requests |= !pass->copy_requests.empty();
    total_quad_count += pass->quad_list.size();
  }
  UMA_HISTOGRAM_COUNTS_1000("Compositing.Display.Draw.Quads", total_quad_count);

  gfx::Size surface_size;
  bool have_damage = false;
  auto& last_render_pass = *frame.render_pass_list.back();
  if (settings_.auto_resize_output_surface &&
      last_render_pass.output_rect.size() != current_surface_size_ &&
      last_render_pass.damage_rect == last_render_pass.output_rect &&
      !current_surface_size_.IsEmpty()) {
    // Resize the output rect to the current surface size so that we won't
    // skip the draw and so that the GL swap won't stretch the output.
    last_render_pass.output_rect.set_size(current_surface_size_);
    last_render_pass.damage_rect = last_render_pass.output_rect;
  }
  surface_size = last_render_pass.output_rect.size();
  have_damage = !last_render_pass.damage_rect.size().IsEmpty();

  bool size_matches = surface_size == current_surface_size_;
  if (!size_matches)
    TRACE_EVENT_INSTANT0("viz", "Size mismatch.", TRACE_EVENT_SCOPE_THREAD);

  bool should_draw = have_copy_requests || (have_damage && size_matches);
  client_->DisplayWillDrawAndSwap(should_draw, frame.render_pass_list);

  if (should_draw) {
    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "Draw");
    if (settings_.enable_draw_occlusion) {
      base::ElapsedTimer draw_occlusion_timer;
      RemoveOverdrawQuads(&frame);
      UMA_HISTOGRAM_COUNTS_1000(
          "Compositing.Display.Draw.Occlusion.Calculation.Time",
          draw_occlusion_timer.Elapsed().InMicroseconds());
    }

    bool disable_image_filtering =
        frame.metadata.is_resourceless_software_draw_with_scroll_or_animation;
    if (software_renderer_) {
      software_renderer_->SetDisablePictureQuadImageFiltering(
          disable_image_filtering);
    } else {
      // This should only be set for software draws in synchronous compositor.
      DCHECK(!disable_image_filtering);
    }

    base::ElapsedTimer draw_timer;
    renderer_->DecideRenderPassAllocationsForFrame(frame.render_pass_list);
    renderer_->DrawFrame(&frame.render_pass_list, device_scale_factor_,
                         current_surface_size_);
    if (software_renderer_) {
      UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.Software.DrawFrameUs",
                              draw_timer.Elapsed().InMicroseconds());
    } else {
      UMA_HISTOGRAM_COUNTS_1M("Compositing.DirectRenderer.GL.DrawFrameUs",
                              draw_timer.Elapsed().InMicroseconds());
    }
  } else {
    TRACE_EVENT_INSTANT0("viz", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = should_draw && size_matches;
  if (should_swap) {
    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "Swap");
    swapped_since_resize_ = true;

    if (scheduler_) {
      frame.metadata.latency_info.emplace_back(ui::SourceEventType::FRAME);
      frame.metadata.latency_info.back().AddLatencyNumberWithTimestamp(
          ui::LATENCY_BEGIN_FRAME_DISPLAY_COMPOSITOR_COMPONENT,
          scheduler_->current_frame_time(), 1);
    }

    std::vector<Surface::PresentedCallback> callbacks;
    for (const auto& id_entry : aggregator_->previous_contained_surfaces()) {
      Surface* surface = surface_manager_->GetSurfaceForId(id_entry.first);
      Surface::PresentedCallback callback;
      if (surface && surface->TakePresentedCallback(&callback)) {
        callbacks.emplace_back(std::move(callback));
      }
    }
    pending_presented_callbacks_.emplace_back(
        std::make_pair(now_time, std::move(callbacks)));

    ui::LatencyInfo::TraceIntermediateFlowEvents(frame.metadata.latency_info,
                                                 "Display::DrawAndSwap");

    cc::benchmark_instrumentation::IssueDisplayRenderingStatsEvent();
    const bool need_presentation_feedback = true;
    renderer_->SwapBuffers(std::move(frame.metadata.latency_info),
                           need_presentation_feedback);
    if (scheduler_)
      scheduler_->DidSwapBuffers();
    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "WaitForAck");
  } else {
    TRACE_EVENT_INSTANT0("viz", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);

    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);

    if (have_damage) {
      // Do not store more than the allowed size.
      if (ui::LatencyInfo::Verify(frame.metadata.latency_info,
                                  "Display::DrawAndSwap")) {
        stored_latency_info_.swap(frame.metadata.latency_info);
      }
    } else {
      // There was no damage. Terminate the latency info objects.
      while (!frame.metadata.latency_info.empty()) {
        auto& latency = frame.metadata.latency_info.back();
        latency.Terminate();
        frame.metadata.latency_info.pop_back();
      }
    }

    TRACE_EVENT_ASYNC_END1("viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
                           swapped_trace_id_, "status", "canceled");
    --swapped_trace_id_;
    if (scheduler_) {
      scheduler_->DidSwapBuffers();
      scheduler_->DidReceiveSwapBuffersAck();
    }
  }

  client_->DisplayDidDrawAndSwap();

  // Garbage collection can lead to sync IPCs to the GPU service to verify sync
  // tokens. We defer garbage collection until the end of DrawAndSwap to avoid
  // stalling the critical path for compositing.
  surface_manager_->GarbageCollectSurfaces();

  return true;
}

void Display::DidReceiveSwapBuffersAck() {
  ++last_acked_trace_id_;
  TRACE_EVENT_ASYNC_END0("viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
                         last_acked_trace_id_);
  if (scheduler_)
    scheduler_->DidReceiveSwapBuffersAck();
  if (renderer_)
    renderer_->SwapBuffersComplete();
}

void Display::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  if (renderer_)
    renderer_->DidReceiveTextureInUseResponses(responses);
}

void Display::DidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  if (client_)
    client_->DisplayDidReceiveCALayerParams(ca_layer_params);
}

void Display::DidSwapWithSize(const gfx::Size& pixel_size) {
  if (client_)
    client_->DisplayDidCompleteSwapWithSize(pixel_size);
}

void Display::DidReceivePresentationFeedback(
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!pending_presented_callbacks_.empty());
  auto& callbacks = pending_presented_callbacks_.front().second;
#if defined(OS_ANDROID)
  // Temporary to investigate large presentation times.
  // https://crbug.com/894440
  const auto swap_time = pending_presented_callbacks_.front().first;
  DCHECK(!swap_time.is_null());
  if (!feedback.timestamp.is_null()) {
    const auto now = base::TimeTicks::Now();
    if (feedback.timestamp > now) {
      const auto diff = feedback.timestamp - now;
      // This collects the time-delta in buckets from 10ms up-to 3minutes. This
      // should provide sufficient information about the spread.
      // https://crbug.com/894440
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Graphics.PresentationTimestamp.InvalidFromFuture", diff);
      base::debug::DumpWithoutCrashing();
      // In debug builds, just crash immediately.
      DCHECK(false);
    }

    const auto difference = feedback.timestamp - swap_time;
    if (difference.InMinutes() > 3) {
      base::debug::DumpWithoutCrashing();
      // In debug builds, just crash immediately.
      DCHECK(false);
    }
  }
#endif
  for (auto& callback : callbacks) {
    std::move(callback).Run(feedback);
  }
  pending_presented_callbacks_.pop_front();
}

void Display::DidFinishLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
}

void Display::SetNeedsRedrawRect(const gfx::Rect& damage_rect) {
  aggregator_->SetFullDamageForSurface(current_surface_id_);
  if (scheduler_) {
    BeginFrameAck ack;
    ack.has_damage = true;
    scheduler_->ProcessSurfaceDamage(current_surface_id_, ack, true);
  }
}

bool Display::SurfaceDamaged(const SurfaceId& surface_id,
                             const BeginFrameAck& ack) {
  if (!ack.has_damage)
    return false;
  bool display_damaged = false;
  if (aggregator_) {
    display_damaged |=
        aggregator_->NotifySurfaceDamageAndCheckForDisplayDamage(surface_id);
  }
  if (surface_id == current_surface_id_) {
    display_damaged = true;
    UpdateRootFrameMissing();
  }
  if (display_damaged)
    surfaces_to_ack_on_next_draw_.push_back(surface_id);
  return display_damaged;
}

void Display::SurfaceDiscarded(const SurfaceId& surface_id) {
  TRACE_EVENT0("viz", "Display::SurfaceDiscarded");
  if (aggregator_)
    aggregator_->ReleaseResources(surface_id);
}

bool Display::SurfaceHasUndrawnFrame(const SurfaceId& surface_id) const {
  if (!surface_manager_)
    return false;

  Surface* surface = surface_manager_->GetSurfaceForId(surface_id);
  if (!surface)
    return false;

  return surface->HasUndrawnActiveFrame();
}

void Display::DidFinishFrame(const BeginFrameAck& ack) {
  for (auto& observer : observers_)
    observer.OnDisplayDidFinishFrame(ack);
}

const SurfaceId& Display::CurrentSurfaceId() {
  return current_surface_id_;
}

LocalSurfaceId Display::GetSurfaceAtAggregation(
    const FrameSinkId& frame_sink_id) const {
  if (!aggregator_)
    return LocalSurfaceId();
  auto it = aggregator_->previous_contained_frame_sinks().find(frame_sink_id);
  if (it == aggregator_->previous_contained_frame_sinks().end())
    return LocalSurfaceId();
  return it->second;
}

void Display::SoftwareDeviceUpdatedCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
  if (client_)
    client_->DisplayDidReceiveCALayerParams(ca_layer_params);
}

void Display::ForceImmediateDrawAndSwapIfPossible() {
  if (scheduler_)
    scheduler_->ForceImmediateSwapIfPossible();
}

void Display::SetNeedsOneBeginFrame() {
  if (scheduler_)
    scheduler_->SetNeedsOneBeginFrame();
}

void Display::RemoveOverdrawQuads(CompositorFrame* frame) {
  if (frame->render_pass_list.empty())
    return;

  const SharedQuadState* last_sqs = nullptr;
  cc::SimpleEnclosedRegion occlusion_in_target_space;
  bool current_sqs_intersects_occlusion = false;
  int minimum_draw_occlusion_height =
      settings_.kMinimumDrawOcclusionSize.height() * device_scale_factor_;
  int minimum_draw_occlusion_width =
      settings_.kMinimumDrawOcclusionSize.width() * device_scale_factor_;

  // Total quad area to be drawn on screen before applying draw occlusion.
  base::CheckedNumeric<uint64_t> total_quad_area_shown_wo_occlusion_px = 0;

  // Total area not draw skipped by draw occlusion.
  base::CheckedNumeric<uint64_t> total_area_saved_in_px = 0;

  for (const auto& pass : frame->render_pass_list) {
    // TODO(yiyix): Add filter effects to draw occlusion calculation and perform
    // draw occlusion on render pass.
    if (!pass->filters.IsEmpty() || !pass->backdrop_filters.IsEmpty()) {
      for (auto* const quad : pass->quad_list) {
        total_quad_area_shown_wo_occlusion_px +=
            quad->visible_rect.size().GetCheckedArea();
      }
      continue;
    }

    // TODO(yiyix): Perform draw occlusion inside the render pass with
    // transparent background.
    if (pass != frame->render_pass_list.back()) {
      for (auto* const quad : pass->quad_list) {
        total_quad_area_shown_wo_occlusion_px +=
            quad->visible_rect.size().GetCheckedArea();
      }
      continue;
    }

    auto quad_list_end = pass->quad_list.end();
    gfx::Rect occlusion_in_quad_content_space;
    for (auto quad = pass->quad_list.begin(); quad != quad_list_end;) {
      total_quad_area_shown_wo_occlusion_px +=
          quad->visible_rect.size().GetCheckedArea();

      // Skip quad if it is a RenderPassDrawQuad because RenderPassDrawQuad is a
      // special type of DrawQuad where the visible_rect of shared quad state is
      // not entirely covered by draw quads in it; or the DrawQuad size is
      // smaller than the kMinimumDrawOcclusionSize; or the DrawQuad is inside
      // a 3d objects.
      if (quad->material == ContentDrawQuadBase::Material::RENDER_PASS ||
          (quad->visible_rect.width() <= minimum_draw_occlusion_width &&
           quad->visible_rect.height() <= minimum_draw_occlusion_height) ||
          quad->shared_quad_state->sorting_context_id != 0) {
        ++quad;
        continue;
      }

      if (!last_sqs)
        last_sqs = quad->shared_quad_state;

      gfx::Transform transform =
          quad->shared_quad_state->quad_to_target_transform;

      // TODO(yiyix): Find a rect interior to each transformed quad.
      if (last_sqs != quad->shared_quad_state) {
        if (last_sqs->opacity == 1 && last_sqs->are_contents_opaque &&
            last_sqs->quad_to_target_transform.Preserves2dAxisAlignment()) {
          gfx::Rect sqs_rect_in_target =
              cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                  last_sqs->quad_to_target_transform,
                  last_sqs->visible_quad_layer_rect);

          if (last_sqs->is_clipped)
            sqs_rect_in_target.Intersect(last_sqs->clip_rect);

          occlusion_in_target_space.Union(sqs_rect_in_target);
        }
        // If the visible_rect of the current shared quad state does not
        // intersect with the occlusion rect, we can skip draw occlusion checks
        // for quads in the current SharedQuadState.
        last_sqs = quad->shared_quad_state;
        current_sqs_intersects_occlusion = occlusion_in_target_space.Intersects(
            cc::MathUtil::MapEnclosingClippedRect(
                transform, last_sqs->visible_quad_layer_rect));

        // Compute the occlusion region in the quad content space for scale and
        // translation transforms. Note that 0 scale transform will fail the
        // positive scale check.
        if (current_sqs_intersects_occlusion &&
            transform.IsPositiveScaleOrTranslation()) {
          gfx::Transform reverse_transform;
          bool is_invertible = transform.GetInverse(&reverse_transform);
          // Scale transform can be inverted by multiplying 1/scale (given
          // scale > 0) and translation transform can be inverted by applying
          // the reversed directional translation. Therefore, |transform| is
          // always invertible.
          DCHECK(is_invertible);

          // TODO(yiyix): Make |occlusion_coordinate_space| to work with
          // occlusion region consists multiple rect.
          DCHECK_EQ(occlusion_in_target_space.GetRegionComplexity(), 1u);

          // Since transform can only be a scale or a translation matrix, it is
          // safe to use function MapEnclosedRectWith2dAxisAlignedTransform to
          // define occluded region in the quad content space with inverted
          // transform.
          occlusion_in_quad_content_space =
              cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
                  reverse_transform, occlusion_in_target_space.bounds());
        } else {
          occlusion_in_quad_content_space = gfx::Rect();
        }
      }

      if (!current_sqs_intersects_occlusion) {
        ++quad;
        continue;
      }

      if (occlusion_in_quad_content_space.Contains(quad->visible_rect)) {
        // Case 1: for simple transforms (scale or translation), define the
        // occlusion region in the quad content space. If the |quad| is not
        // shown on the screen, then remove |quad| from the compositor frame.
        total_area_saved_in_px += quad->visible_rect.size().GetCheckedArea();
        quad = pass->quad_list.EraseAndInvalidateAllPointers(quad);

      } else if (occlusion_in_quad_content_space.Intersects(
                     quad->visible_rect)) {
        // Case 2: for simple transforms, if the quad is partially shown on
        // screen and the region formed by (occlusion region - visible_rect) is
        // a rect, then update visible_rect to the resulting rect.
        gfx::Rect origin_rect = quad->visible_rect;
        quad->visible_rect.Subtract(occlusion_in_quad_content_space);
        if (origin_rect != quad->visible_rect) {
          origin_rect.Subtract(quad->visible_rect);
          total_area_saved_in_px += origin_rect.size().GetCheckedArea();
        }
        ++quad;
      } else if (occlusion_in_quad_content_space.IsEmpty() &&
                 occlusion_in_target_space.Contains(
                     cc::MathUtil::MapEnclosingClippedRect(
                         transform, quad->visible_rect))) {
        // Case 3: for non simple transforms, define the occlusion region in
        // target space. If the |quad| is not shown on the screen, then remove
        // |quad| from the compositor frame.
        total_area_saved_in_px += quad->visible_rect.size().GetCheckedArea();
        quad = pass->quad_list.EraseAndInvalidateAllPointers(quad);
      } else {
        ++quad;
      }
    }
  }

  UMA_HISTOGRAM_PERCENTAGE(
      "Compositing.Display.Draw.Occlusion.Percentage.Saved",
      total_quad_area_shown_wo_occlusion_px.ValueOrDefault(0) == 0
          ? 0
          : static_cast<uint64_t>(total_area_saved_in_px.ValueOrDie()) * 100 /
                static_cast<uint64_t>(
                    total_quad_area_shown_wo_occlusion_px.ValueOrDie()));

  UMA_HISTOGRAM_COUNTS_1M(
      "Compositing.Display.Draw.Occlusion.Drawing.Area.Saved2",
      static_cast<uint64_t>(total_area_saved_in_px.ValueOrDefault(
          std::numeric_limits<uint64_t>::max())));
}

}  // namespace viz
