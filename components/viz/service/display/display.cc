// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/display.h"

#include <stddef.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/debug/dump_without_crashing.h"
#include "base/metrics/histogram_macros.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/base/simple_enclosed_region.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/switches.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/aggregated_frame.h"
#include "components/viz/service/display/damage_frame_annotator.h"
#include "components/viz/service/display/delegated_ink_point_renderer_base.h"
#include "components/viz/service/display/direct_renderer.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/display/display_resource_provider_null.h"
#include "components/viz/service/display/display_resource_provider_skia.h"
#include "components/viz/service/display/display_resource_provider_software.h"
#include "components/viz/service/display/display_scheduler.h"
#include "components/viz/service/display/display_utils.h"
#include "components/viz/service/display/frame_interval_decider.h"
#include "components/viz/service/display/frame_interval_matchers.h"
#include "components/viz/service/display/null_renderer.h"
#include "components/viz/service/display/occlusion_culler.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/overdraw_tracker.h"
#include "components/viz/service/display/overlay_candidate_factory.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/display/skia_renderer.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/surface_aggregator.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/scheduler_sequence.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_service_utils.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_latency_info.pbzero.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/overlay_transform_utils.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/gfx/android/android_surface_control_compat.h"
#endif
namespace viz {

namespace {

#if !BUILDFLAG(IS_MAC)
constexpr base::TimeDelta kAllowedDeltaFromFuture = base::Milliseconds(16);
#endif

// Assign each Display instance a starting value for the the display-trace id,
// so that multiple Displays all don't start at 0, because that makes it
// difficult to associate the trace-events with the particular displays.
int64_t GetStartingTraceId() {
  static int64_t client = 0;
  return ((++client & 0xffffffff) << 16);
}

gfx::PresentationFeedback SanitizePresentationFeedback(
    const gfx::PresentationFeedback& feedback,
    base::TimeTicks draw_time) {
  if (feedback.timestamp.is_null())
    return feedback;

  // If the presentation-timestamp is from the future, or from the past (i.e.
  // before swap-time), then invalidate the feedback. Also report how far into
  // the future (or from the past) the timestamps are.
  // https://crbug.com/894440
  //
  // The timestamp for the presentation feedback may have a different source and
  // therefore the timestamp can be slightly in the future in comparison with
  // base::TimeTicks::Now(). Such presentation feedbacks should not be rejected.
  // See https://crbug.com/1040178
  // Sometimes we snap the feedback's time stamp to the nearest vsync, and that
  // can be offset by one vsync-internal. These feedback has kVSync set.

  // If the the presentation is from before the swap-time, then invalidate
  // the feedback.
  if (feedback.timestamp < draw_time) {
    return gfx::PresentationFeedback::Failure();
  }

  // All |feedback.timestamp| on Mac are valid and should not be sanitized.
#if !BUILDFLAG(IS_MAC)
  const auto now = base::TimeTicks::Now();
  const auto allowed_delta_from_future =
      ((feedback.flags & (gfx::PresentationFeedback::kHWClock |
                          gfx::PresentationFeedback::kVSync)) != 0)
          ? kAllowedDeltaFromFuture
          : base::TimeDelta();
  if (feedback.timestamp > now + allowed_delta_from_future) {
    return gfx::PresentationFeedback::Failure();
  }
#endif

  return feedback;
}

void IssueDisplayRenderingStatsEvent() {
  std::unique_ptr<base::trace_event::TracedValue> record_data =
      std::make_unique<base::trace_event::TracedValue>();
  record_data->SetInteger("frame_count", 1);
  // Please don't rename this trace event as it's used by tools. The benchmarks
  // search for events and their arguments by name.
  TRACE_EVENT_INSTANT1(
      "benchmark", "BenchmarkInstrumentation::DisplayRenderingStats",
      TRACE_EVENT_SCOPE_THREAD, "data", std::move(record_data));
}

}  // namespace

constexpr base::TimeDelta Display::kDrawToSwapMin;
constexpr base::TimeDelta Display::kDrawToSwapMax;

Display::PresentationGroupTiming::PresentationGroupTiming() = default;

Display::PresentationGroupTiming::PresentationGroupTiming(
    Display::PresentationGroupTiming&& other) = default;

Display::PresentationGroupTiming::~PresentationGroupTiming() = default;

void Display::PresentationGroupTiming::AddPresentationHelper(
    std::unique_ptr<Surface::PresentationHelper> helper) {
  presentation_helpers_.push_back(std::move(helper));
}

void Display::PresentationGroupTiming::OnDraw(
    base::TimeTicks frame_time,
    base::TimeTicks draw_start_timestamp,
    base::flat_set<base::PlatformThreadId> thread_ids,
    HintSession::BoostType boost_type) {
  frame_time_ = frame_time;
  draw_start_timestamp_ = draw_start_timestamp;
  thread_ids_ = std::move(thread_ids);
  boost_type_ = boost_type;
}

void Display::PresentationGroupTiming::OnSwap(gfx::SwapTimings timings,
                                              DisplaySchedulerBase* scheduler) {
  swap_timings_ = timings;

  if (timings.swap_start.is_null() || frame_time_.is_inf())
    return;

  auto frame_latency = timings.swap_start - frame_time_;
  if (frame_latency < base::Seconds(0)) {
    LOG(ERROR) << "Frame latency is negative: "
               << frame_latency.InMillisecondsF() << " ms";
    return;
  }
  // Can be nullptr in unittests.
  if (scheduler) {
    scheduler->ReportFrameTime(frame_latency, std::move(thread_ids_),
                               draw_start_timestamp_, boost_type_);
  }
}

void Display::PresentationGroupTiming::OnPresent(
    const gfx::PresentationFeedback& feedback) {
  for (auto& presentation_helper : presentation_helpers_) {
    presentation_helper->DidPresent(draw_start_timestamp_, swap_timings_,
                                    feedback);
  }
}

Display::Display(
    SharedBitmapManager* bitmap_manager,
    gpu::SharedImageManager* shared_image_manager,
    gpu::SyncPointManager* sync_point_manager,
    gpu::Scheduler* gpu_scheduler,
    const RendererSettings& settings,
    const DebugRendererSettings* debug_settings,
    const FrameSinkId& frame_sink_id,
    std::unique_ptr<DisplayCompositorMemoryAndTaskController> gpu_dependency,
    std::unique_ptr<OutputSurface> output_surface,
    std::unique_ptr<OverlayProcessorInterface> overlay_processor,
    std::unique_ptr<DisplaySchedulerBase> scheduler,
    scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
    : bitmap_manager_(bitmap_manager),
      shared_image_manager_(shared_image_manager),
      sync_point_manager_(sync_point_manager),
      gpu_scheduler_(gpu_scheduler),
      settings_(settings),
      debug_settings_(debug_settings),
      frame_sink_id_(frame_sink_id),
      gpu_dependency_(std::move(gpu_dependency)),
      output_surface_(std::move(output_surface)),
      skia_output_surface_(output_surface_->AsSkiaOutputSurface()),
      scheduler_(std::move(scheduler)),
      current_task_runner_(std::move(current_task_runner)),
      overlay_processor_(std::move(overlay_processor)),
      swapped_trace_id_(GetStartingTraceId()),
      last_swap_ack_trace_id_(swapped_trace_id_),
      last_presented_trace_id_(swapped_trace_id_) {
  DCHECK(output_surface_);
  DCHECK(frame_sink_id_.is_valid());

#if BUILDFLAG(IS_CHROMEOS_ASH)
  static bool logged = false;
  // TODO(b/329688656): Remove this after the issue is resolved.
  if (!logged && output_surface_->capabilities().max_texture_size > 0) {
    logged = true;
    LOG(ERROR) << "Max Texture Size="
               << output_surface_->capabilities().max_texture_size;
  }
#endif

  occlusion_culler_ = std::make_unique<OcclusionCuller>(
      overlay_processor_.get(), settings_.occlusion_culler_settings);

  if (scheduler_)
    scheduler_->SetClient(this);
}

Display::~Display() {
#if DCHECK_IS_ON()
  allow_schedule_gpu_task_during_destruction_.reset(
      new gpu::ScopedAllowScheduleGpuTask);
#endif
  if (resource_provider_) {
    resource_provider_->SetAllowAccessToGPUThread(true);
  }

  if (no_pending_swaps_callback_)
    std::move(no_pending_swaps_callback_).Run();

  for (auto& observer : observers_)
    observer.OnDisplayDestroyed();
  observers_.Clear();

  // Send gfx::PresentationFeedback::Failure() to any surfaces expecting
  // feedback.
  pending_presentation_group_timings_.clear();

  // Only do this if Initialize() happened.
  if (client_) {
    if (skia_output_surface_)
      skia_output_surface_->RemoveContextLostObserver(this);
  }

  // Un-register as DisplaySchedulerClient to prevent us from being called in a
  // partially destructed state.
  if (scheduler_)
    scheduler_->SetClient(nullptr);

  if (damage_tracker_)
    damage_tracker_->RunDrawCallbacks();
}

void Display::Initialize(DisplayClient* client,
                         SurfaceManager* surface_manager,
                         bool hw_support_for_multiple_refresh_rates) {
  DCHECK(client);
  DCHECK(surface_manager);
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;
  client_ = client;
  surface_manager_ = surface_manager;

  output_surface_->BindToClient(this);
  if (output_surface_->software_device())
    output_surface_->software_device()->BindToClient(this);

  if (features::IsUsingFrameIntervalDecider()) {
    frame_interval_decider_ = std::make_unique<FrameIntervalDecider>();
  } else {
    bool output_surface_supports_set_frame_rate = false;
#if BUILDFLAG(IS_ANDROID)
    output_surface_supports_set_frame_rate =
        OutputSurfaceSupportsSetFrameRate();
#endif
    frame_rate_decider_ = std::make_unique<FrameRateDecider>(
        surface_manager_, this, hw_support_for_multiple_refresh_rates,
        output_surface_supports_set_frame_rate);
  }

  InitializeRenderer();

  damage_tracker_ = std::make_unique<DisplayDamageTracker>(surface_manager_,
                                                           aggregator_.get());
  if (scheduler_)
    scheduler_->SetDamageTracker(damage_tracker_.get());

  // This depends on assumptions that Display::Initialize will happen on the
  // same callstack as the ContextProvider being created/initialized or else
  // it could miss a callback before setting this.
  if (skia_output_surface_)
    skia_output_surface_->AddContextLostObserver(this);
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

  damage_tracker_->SetNewRootSurface(current_surface_id_);
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
  disable_swap_until_resize_ = false;

  if (size == current_surface_size_)
    return;

  // This DCHECK should probably go at the top of the function, but mac
  // sometimes calls Resize() with 0x0 before it sets a real size. This will
  // early out before the DCHECK fails.
  DCHECK(!size.IsEmpty());
  TRACE_EVENT0("viz", "Display::Resize");

  swapped_since_resize_ = false;
  current_surface_size_ = size;

  damage_tracker_->DisplayResized();
}

void Display::SetOutputSurfaceClipRect(const gfx::Rect& clip_rect) {
  renderer_->SetOutputSurfaceClipRect(clip_rect);
}

void Display::InvalidateCurrentSurfaceId() {
  current_surface_id_ = SurfaceId();
  // Force a gc as the display may not be visible (gc occurs after drawing,
  // which won't happen when display is hidden).
  surface_manager_->GarbageCollectSurfaces();
}

void Display::DisableSwapUntilResize(
    base::OnceClosure no_pending_swaps_callback) {
  TRACE_EVENT0("viz", "Display::DisableSwapUntilResize");
  DCHECK(no_pending_swaps_callback_.is_null());

  if (!disable_swap_until_resize_) {
    DCHECK(scheduler_);

    if (!swapped_since_resize_)
      scheduler_->ForceImmediateSwapIfPossible();

    if (no_pending_swaps_callback && pending_swaps_ > 0 &&
        output_surface_->AsSkiaOutputSurface()) {
      no_pending_swaps_callback_ = std::move(no_pending_swaps_callback);
    }

    disable_swap_until_resize_ = true;
  }

  // There are no pending swaps for current size so immediately run callback.
  if (no_pending_swaps_callback)
    std::move(no_pending_swaps_callback).Run();
}

void Display::SetColorMatrix(const SkM44& matrix) {
  if (output_surface_)
    output_surface_->set_color_matrix(matrix);

  // Force a redraw.
  if (aggregator_) {
    if (current_surface_id_.is_valid())
      aggregator_->SetFullDamageForSurface(current_surface_id_);
  }

  damage_tracker_->SetRootSurfaceDamaged();
}

void Display::SetDisplayColorSpaces(
    const gfx::DisplayColorSpaces& display_color_spaces) {
  display_color_spaces_ = display_color_spaces;
  if (aggregator_)
    aggregator_->SetDisplayColorSpaces(display_color_spaces_);
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
  if (skia_output_surface_) {
    auto resource_provider = std::make_unique<DisplayResourceProviderSkia>();
    renderer_ = std::make_unique<SkiaRenderer>(
        &settings_, debug_settings_, output_surface_.get(),
        resource_provider.get(), overlay_processor_.get(),
        skia_output_surface_);
    resource_provider_ = std::move(resource_provider);
  } else if (output_surface_->capabilities().skips_draw) {
    auto resource_provider = std::make_unique<DisplayResourceProviderNull>();
    renderer_ = std::make_unique<NullRenderer>(
        &settings_, debug_settings_, output_surface_.get(),
        resource_provider.get(), overlay_processor_.get());
    resource_provider_ = std::move(resource_provider);
  } else {
    auto resource_provider = std::make_unique<DisplayResourceProviderSoftware>(
        bitmap_manager_, shared_image_manager_, sync_point_manager_,
        gpu_scheduler_);
    DCHECK(!overlay_processor_->IsOverlaySupported());
    auto renderer = std::make_unique<SoftwareRenderer>(
        &settings_, debug_settings_, output_surface_.get(),
        resource_provider.get(), overlay_processor_.get());
    software_renderer_ = renderer.get();
    renderer_ = std::move(renderer);
    resource_provider_ = std::move(resource_provider);
  }

  renderer_->Initialize();
  renderer_->SetVisible(visible_);

  SurfaceAggregator::ExtraPassForReadbackOption extra_pass_option =
      SurfaceAggregator::ExtraPassForReadbackOption::kNone;
  if (output_surface_->capabilities().root_is_vulkan_secondary_command_buffer) {
    extra_pass_option =
        base::FeatureList::IsEnabled(features::kWebViewVulkanIntermediateBuffer)
            ? SurfaceAggregator::ExtraPassForReadbackOption::kAlwaysAddPass
            : SurfaceAggregator::ExtraPassForReadbackOption::
                  kAddPassForReadback;
  }
#if BUILDFLAG(IS_WIN)
  const bool prevent_merging_surfaces_to_root_pass =
      features::IsDelegatedCompositingEnabled() &&
      features::kDelegatedCompositingModeParam.Get() ==
          features::DelegatedCompositingMode::kLimitToUi &&
      output_surface_->capabilities().dc_support_level >=
          OutputSurface::DCSupportLevel::kDCompTexture;
#else
  const bool prevent_merging_surfaces_to_root_pass = false;
#endif
  aggregator_ = std::make_unique<SurfaceAggregator>(
      surface_manager_, resource_provider_.get(),
      overlay_processor_->NeedsSurfaceDamageRectList(), extra_pass_option,
      prevent_merging_surfaces_to_root_pass);

  aggregator_->set_output_is_secure(output_is_secure_);
  aggregator_->SetDisplayColorSpaces(display_color_spaces_);
  aggregator_->SetMaxRenderTargetSize(
      output_surface_->capabilities().max_render_target_size);
  // Do not move the |CopyOutputRequest| instances to the aggregated frame if
  // the frame won't be drawn (as that would drop the copy request).
  aggregator_->set_take_copy_requests(
      !output_surface_->capabilities().skips_draw);
}

bool Display::IsRootFrameMissing() const {
  return damage_tracker_->root_frame_missing();
}

bool Display::HasPendingSurfaces(const BeginFrameArgs& args) const {
  return damage_tracker_->HasPendingSurfaces(args);
}

void Display::OnContextLost() {
  if (scheduler_)
    scheduler_->OutputSurfaceLost();
  // WARNING: The client may delete the Display in this method call. Do not
  // make any additional references to members after this call.
  client_->DisplayOutputSurfaceLost();
}

namespace {

DBG_FLAG_FBOOL("frame.debug.non_root_passes", debug_non_root_passes)

DBG_FLAG_FBOOL("frame.render_pass.non_root_passes_in_root_space",
               non_root_passes_in_root_space)

void DebugDrawFrame(
    const AggregatedFrame& frame,
    const std::unique_ptr<DisplayResourceProvider>& resource_provider) {
  bool is_debugger_connected = false;
  DBG_CONNECTED_OR_TRACING(is_debugger_connected);
  if (!is_debugger_connected) {
    return;
  }

  for (auto& render_pass : frame.render_pass_list) {
    if (render_pass != frame.render_pass_list.back() &&
        !debug_non_root_passes()) {
      continue;
    }

    auto output_rect = render_pass->output_rect;
    auto damage_rect = render_pass->damage_rect;
    if (non_root_passes_in_root_space()) {
      output_rect = render_pass->transform_to_root_target.MapRect(output_rect);
      damage_rect = render_pass->transform_to_root_target.MapRect(damage_rect);
    }

    DBG_DRAW_RECT_OPT("frame.render_pass.output_rect", DBG_OPT_BLUE,
                      output_rect);
    DBG_DRAW_RECT_OPT("frame.render_pass.damage", DBG_OPT_RED, damage_rect);

    DBG_LOG_OPT("frame.render_pass.meta", DBG_OPT_BLUE,
                "Render pass id=%" PRIu64
                ", output_rect=(%s), damage_rect=(%s), "
                "quad_list.size=%zu",
                render_pass->id.value(),
                render_pass->output_rect.ToString().c_str(),
                render_pass->damage_rect.ToString().c_str(),
                render_pass->quad_list.size());
    DBG_LOG_OPT(
        "frame.render_pass.transform_to_root_target", DBG_OPT_BLUE,
        "Render pass transform=%s",
        render_pass->transform_to_root_target.ToDecomposedString().c_str());

    for (auto* quad : render_pass->quad_list) {
      auto* sqs = quad->shared_quad_state;
      auto quad_to_root_transform = sqs->quad_to_target_transform;
      if (non_root_passes_in_root_space()) {
        quad_to_root_transform.PostConcat(
            render_pass->transform_to_root_target);
      }
      auto display_rect =
          quad_to_root_transform.MapRect(gfx::RectF(quad->rect));
      DBG_DRAW_TEXT_OPT("frame.render_pass.material", DBG_OPT_GREEN,
                        display_rect.origin(),
                        base::NumberToString(static_cast<int>(quad->material)));
      DBG_DRAW_TEXT_OPT(
          "frame.render_pass.layer_id", DBG_OPT_BLUE, display_rect.origin(),
          base::StringPrintf("%u:%u", sqs->layer_namespace_id, sqs->layer_id));
      DBG_DRAW_TEXT_OPT("frame.render_pass.display_rect", DBG_OPT_GREEN,
                        display_rect.origin(), display_rect.ToString());
      DBG_DRAW_TEXT_OPT(
          "frame.render_pass.resource_id", DBG_OPT_RED, display_rect.origin(),
          base::NumberToString(quad->resources.ids[0].GetUnsafeValue()));

      if (quad->resources.ids[0] != kInvalidResourceId) {
        DBG_DRAW_TEXT_OPT(
            "frame.render_pass.buf_format", DBG_OPT_BLUE, display_rect.origin(),
            resource_provider->GetSharedImageFormat(quad->resources.ids[0])
                .ToString());
        DBG_DRAW_TEXT_OPT(
            "frame.render_pass.buf_color_space", DBG_OPT_GREEN,
            display_rect.origin(),
            resource_provider->GetColorSpace(quad->resources.ids[0])
                .ToString());
      }
      DBG_DRAW_RECT("frame.render_pass.quad", display_rect);
    }
  }
}

void DebugDrawFrameVisible(const AggregatedFrame& frame) {
  bool is_debugger_connected = false;
  DBG_CONNECTED_OR_TRACING(is_debugger_connected);
  if (!is_debugger_connected) {
    return;
  }

  auto& root_render_pass = *frame.render_pass_list.back();
  [[maybe_unused]] int num_quad_empty = 0;
  for (auto* quad : root_render_pass.quad_list) {
    auto& transform = quad->shared_quad_state->quad_to_target_transform;
    auto display_rect = transform.MapRect(gfx::RectF(quad->visible_rect));
    DBG_DRAW_TEXT_OPT("frame.root.display_rect_visible", DBG_OPT_GREEN,
                      display_rect.origin(), display_rect.ToString());
    DBG_DRAW_RECT("frame.root.visible", display_rect);

    if (quad->visible_rect.IsEmpty()) {
      num_quad_empty++;
    }
  }

  DBG_LOG_OPT("frame.root.num_empty_visible", DBG_OPT_BLUE,
              "Num quads that have empty visibility =%d", num_quad_empty);
}

void VisualDebuggerSync(gfx::OverlayTransform current_display_transform,
                        gfx::Size current_surface_size,
                        int64_t last_presented_trace_id) {
  bool is_debugger_connected = false;
  DBG_CONNECTED_OR_TRACING(is_debugger_connected);
  if (!is_debugger_connected) {
    return;
  }

  const gfx::Transform display_transform = gfx::OverlayTransformToTransform(
      current_display_transform, gfx::SizeF(current_surface_size));
  current_surface_size =
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          display_transform, gfx::Rect(current_surface_size))
          .size();

  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("viz.visual_debugger"),
               "visual_debugger_sync", "last_presented_trace_id",
               last_presented_trace_id, "display_size",
               current_surface_size.ToString());
  VizDebugger::GetInstance()->CompleteFrame(
      last_presented_trace_id, current_surface_size, base::TimeTicks::Now());
}

}  // namespace

void Display::MaybeLogQuadsProperties(
    AggregatedRenderPass& last_render_pass,
    const SurfaceDamageRectList* surface_damage_rect_list) {
  // A restraint on how frequently we log quad infos in number of frames.
  constexpr double kLogQuadInfoProbability = 1.0 / 20000;
  if (!metrics_subsampler_.ShouldSample(kLogQuadInfoProbability)) {
    return;
  }
  base::ElapsedTimer logging_timer;
  int num_nonopaque_quads = 0;
  int num_roundedcorners_quads = 0;
  int num_transformation_quads = 0;
  int num_nonaligned_quads = 0;
  int num_nonpixelaligned_quads = 0;
  int num_solid_quads = 0;
  int num_scaled_quads = 0;
  int num_failed_candidate = 0;

  OverlayCandidateFactory::OverlayContext context;
  context.is_delegated_context = true;
  context.supports_clip_rect = true;
  context.supports_out_of_window_clip_rect = true;
  context.supports_arbitrary_transform = true;
  context.supports_mask_filter = true;
  context.transform_and_clip_rpdq = true;
  context.supports_flip_rotate_transform = true;

  SkM44 color_matrix;
  // auto resource_provider = std::make_unique<DisplayResourceProviderSkia>();
  base::flat_map<AggregatedRenderPassId,
                 raw_ptr<cc::FilterOperations, CtnExperimental>>
      render_pass_filters;
  render_pass_filters[last_render_pass.id] = &(last_render_pass.filters);
  OverlayCandidateFactory candidate_factory = OverlayCandidateFactory(
      &last_render_pass, resource_provider_.get(), surface_damage_rect_list,
      &color_matrix, gfx::RectF(), &render_pass_filters, context);

  OverlayCandidate candidate;

  for (auto* quad : last_render_pass.quad_list) {
    auto result = candidate_factory.FromDrawQuad(quad, candidate);
    if (result == OverlayCandidate::CandidateStatus::kFailNotAxisAligned ||
        result ==
            OverlayCandidate::CandidateStatus::kFailNotAxisAligned3dTransform ||
        result ==
            OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dShear ||
        result ==
            OverlayCandidate::CandidateStatus::kFailNotAxisAligned2dRotation) {
      num_nonaligned_quads++;
    }

    if (result != OverlayCandidate::CandidateStatus::kSuccess) {
      num_failed_candidate++;
    }

    if (!candidate.rounded_corners.IsEmpty()) {
      num_roundedcorners_quads++;
    }
    if (!candidate.is_opaque) {
      num_nonopaque_quads++;
    }
    if (!absl::holds_alternative<gfx::OverlayTransform>(candidate.transform) ||
        absl::get<gfx::OverlayTransform>(candidate.transform) !=
             gfx::OVERLAY_TRANSFORM_NONE) {
      num_transformation_quads++;
    }
    if (candidate.is_solid_color) {
      num_solid_quads++;
    }
    auto rect = OverlayCandidate::DisplayRectInTargetSpace(candidate);
    if (IsNearestRectWithinDistance(rect,
                                    std::numeric_limits<float>::epsilon())) {
      num_nonpixelaligned_quads++;
    }
    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.Display.Draw.LastPass.Quads.ColorSpacePrimaryID",
        candidate.color_space.GetPrimaryID());
    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.Display.Draw.LastPass.Quads.ColorSpaceTransferID",
        candidate.color_space.GetTransferID());
    UMA_HISTOGRAM_ENUMERATION(
        "Compositing.Display.Draw.LastPass.Quads.BufferFormat",
        gpu::ToBufferFormat(candidate.format));
    gfx::RectF uv_rect = candidate.uv_rect;
    candidate_factory.HandleClipAndSubsampling(candidate);
    if (uv_rect != candidate.uv_rect) {
      num_scaled_quads++;
    }
  }

  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.LastPass.Quads",
                           last_render_pass.quad_list.size());
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.LastPass.Quads.NonOpaque",
                           num_nonopaque_quads);
  UMA_HISTOGRAM_COUNTS_100(
      "Compositing.Display.Draw.LastPass.Quads.RoundedCorners",
      num_roundedcorners_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.Transformations",
                           num_transformation_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.NonAligned",
                           num_nonaligned_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.NonPixelAligned",
                           num_nonpixelaligned_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.SolidColor",
                           num_solid_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.Scaled",
                           num_scaled_quads);
  UMA_HISTOGRAM_COUNTS_100("Compositing.Display.Draw.Quads.FailedCandidate",
                           num_failed_candidate);

  UMA_HISTOGRAM_COUNTS_1M("Compositing.Display.Draw.Quads.LoggingTimeUs",
                          logging_timer.Elapsed().InMicroseconds());
}

void Display::StartTrackingOverdraw(int interval_length_in_seconds) {
  CHECK(!overdraw_tracker_);

  OverdrawTracker::Settings settings;
  settings.interval_length_in_seconds = interval_length_in_seconds;

  overdraw_tracker_ = std::make_unique<OverdrawTracker>(settings);
}

OverdrawTracker::OverdrawTimeSeries Display::StopTrackingOverdraw() {
  // Returns empty time series if `overdraw_tracker_` has no value. This could
  // happen when gpu-process is restarted in middle of test and test scripts
  // still calls this at the end.
  if (!overdraw_tracker_) {
    return OverdrawTracker::OverdrawTimeSeries();
  }

  auto overdraw_data = overdraw_tracker_->TakeDataAsTimeSeries();
  overdraw_tracker_.reset();

  return overdraw_data;
}

bool Display::DrawAndSwap(const DrawAndSwapParams& params) {
  TRACE_EVENT0("viz", "Display::DrawAndSwap");
  if (debug_settings_->show_aggregated_damage !=
      aggregator_->HasFrameAnnotator()) {
    if (debug_settings_->show_aggregated_damage) {
      aggregator_->SetFrameAnnotator(std::make_unique<DamageFrameAnnotator>());
    } else {
      aggregator_->DestroyFrameAnnotator();
    }
  }
  gpu::ScopedAllowScheduleGpuTask allow_schedule_gpu_task;

  if (!current_surface_id_.is_valid()) {
    TRACE_EVENT_INSTANT0("viz", "No root surface.", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  if (!output_surface_) {
    TRACE_EVENT_INSTANT0("viz", "No output surface", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  ++swapped_trace_id_;
  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::Flow::Global(swapped_trace_id_),
      [this](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_DRAW_AND_SWAP);
        data->set_display_trace_id(swapped_trace_id_);
      });

  if (params.max_pending_swaps >= 0 && skia_output_surface_ &&
      skia_output_surface_->capabilities()
          .supports_dynamic_frame_buffer_allocation) {
    renderer_->EnsureMinNumberOfBuffers(params.max_pending_swaps + 1);
  }

  gfx::OverlayTransform current_display_transform = gfx::OVERLAY_TRANSFORM_NONE;
  Surface* surface = surface_manager_->GetSurfaceForId(current_surface_id_);
  if (surface->HasActiveFrame()) {
    current_display_transform =
        surface->GetActiveFrameMetadata().display_transform_hint;
    if (current_display_transform != output_surface_->GetDisplayTransform()) {
      output_surface_->SetDisplayTransformHint(current_display_transform);

      // Gets the transform from |output_surface_| back so that if it ignores
      // the hint, the rest of the code ignores the hint too.
      current_display_transform = output_surface_->GetDisplayTransform();
    }
  }

  absl::Cleanup visual_debugger_sync_scoped_exit =
      [current_display_transform, current_surface_size = current_surface_size_,
       last_presented_trace_id = last_presented_trace_id_] {
        VisualDebuggerSync(current_display_transform, current_surface_size,
                           last_presented_trace_id);
      };

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
      resource_provider_.get(), /*allow_access_to_gpu_thread=*/true);

  base::ElapsedTimer aggregate_timer;
  AggregatedFrame frame;
  {
    std::optional<FrameRateDecider::ScopedAggregate> scoped_aggregate;
    if (frame_rate_decider_) {
      scoped_aggregate.emplace(frame_rate_decider_.get());
    }
    std::unique_ptr<FrameIntervalDecider::ScopedAggregate>
        scoped_interval_decider;
    if (frame_interval_decider_) {
      scoped_interval_decider = frame_interval_decider_->WrapAggregate(
          *surface_manager_, params.frame_time);
    }
    gfx::Rect target_damage_bounding_rect;
    if (output_surface_->capabilities().supports_target_damage)
      target_damage_bounding_rect = renderer_->GetTargetDamageBoundingRect();

    // Ensure that the surfaces that were damaged by any delegated ink trail are
    // aggregated again so that the trail exists for a single frame.
    target_damage_bounding_rect.Union(
        renderer_->GetDelegatedInkTrailDamageRect());
    frame = aggregator_->Aggregate(
        current_surface_id_, params.expected_display_time,
        current_display_transform, target_damage_bounding_rect,
        swapped_trace_id_);
  }
  DebugDrawFrame(frame, resource_provider_);

  if (frame.delegated_ink_metadata) {
    TRACE_EVENT_INSTANT1(
        "delegated_ink_trails",
        "Delegated Ink Metadata was aggregated for DrawAndSwap.",
        TRACE_EVENT_SCOPE_THREAD, "ink metadata",
        frame.delegated_ink_metadata->ToString());
    renderer_->SetDelegatedInkMetadata(std::move(frame.delegated_ink_metadata));
  }

  UMA_HISTOGRAM_ENUMERATION("Compositing.ColorGamut",
                            frame.content_color_usage);

#if BUILDFLAG(IS_ANDROID)
  bool wide_color_enabled =
      display_color_spaces_.GetOutputColorSpace(
          frame.content_color_usage, true) != gfx::ColorSpace::CreateSRGB();
  if (wide_color_enabled != last_wide_color_enabled_) {
    client_->SetWideColorEnabled(wide_color_enabled);
    last_wide_color_enabled_ = wide_color_enabled;
  }
#endif

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
  damage_tracker_->RunDrawCallbacks();

  if (output_surface_->capabilities().skips_draw) {
    TRACE_EVENT_INSTANT0("viz", "Skip draw", TRACE_EVENT_SCOPE_THREAD);
    // Aggregation needs to happen before generating hit test for the unified
    // desktop display. After this point skip drawing anything for real.
    client_->DisplayWillDrawAndSwap(false, &frame.render_pass_list);
    return true;
  }

  frame.latency_info.insert(frame.latency_info.end(),
                            stored_latency_info_.begin(),
                            stored_latency_info_.end());
  stored_latency_info_.clear();
  bool have_copy_requests = frame.has_copy_requests;
  size_t total_quad_count = 0;
  for (const auto& pass : frame.render_pass_list) {
    total_quad_count += pass->quad_list.size();
  }
  UMA_HISTOGRAM_COUNTS_1000("Compositing.Display.Draw.Quads", total_quad_count);

  gfx::Size surface_size;
  bool have_damage = false;
  auto& last_render_pass = *frame.render_pass_list.back();

  // log quad types every so often if experiment and n-th frame
  if (features::ShouldLogFrameQuadInfo()) {
    MaybeLogQuadsProperties(last_render_pass,
                            &(frame.surface_damage_rect_list_));
  }

  // The CompositorFrame provided by the SurfaceAggregator includes the display
  // transform while |current_surface_size_| is the pre-transform size received
  // from the client.
  const gfx::Transform display_transform = gfx::OverlayTransformToTransform(
      current_display_transform, gfx::SizeF(current_surface_size_));
  const gfx::Size current_surface_size =
      cc::MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
          display_transform, gfx::Rect(current_surface_size_))
          .size();
  if (settings_.auto_resize_output_surface &&
      last_render_pass.output_rect.size() != current_surface_size &&
      last_render_pass.damage_rect == last_render_pass.output_rect &&
      !current_surface_size.IsEmpty()) {
    // Resize the |output_rect| to the |current_surface_size| so that we won't
    // skip the draw and so that the GL swap won't stretch the output.
    last_render_pass.output_rect.set_size(current_surface_size);
    last_render_pass.damage_rect = last_render_pass.output_rect;
    frame.surface_damage_rect_list_.push_back(last_render_pass.damage_rect);
  }
  surface_size = last_render_pass.output_rect.size();
  have_damage = !last_render_pass.damage_rect.size().IsEmpty();

  bool size_matches = surface_size == current_surface_size;
  if (!size_matches)
    TRACE_EVENT_INSTANT0("viz", "Size mismatch.", TRACE_EVENT_SCOPE_THREAD);

  bool should_draw = have_copy_requests || (have_damage && size_matches);
  client_->DisplayWillDrawAndSwap(should_draw, &frame.render_pass_list);

  std::optional<base::ElapsedTimer> draw_timer;
  if (should_draw) {
    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "Draw");
    base::ElapsedTimer draw_occlusion_timer;
    occlusion_culler_->RemoveOverdrawQuads(&frame, device_scale_factor_);
    DebugDrawFrameVisible(frame);
    UMA_HISTOGRAM_COUNTS_1000(
        "Compositing.Display.Draw.Occlusion.Calculation.Time",
        draw_occlusion_timer.Elapsed().InMicroseconds());

    DBG_LOG("renderer.ptr", "renderer = %p%s", this,
            renderer_.get() == software_renderer_ ? " (software)" : "");

    if (overdraw_tracker_) {
      overdraw_tracker_->EstimateAndRecordOverdraw(&frame,
                                                   base::TimeTicks::Now());
    }

    OverdrawTracker::EstimateAndRecordOverdrawAsUMAMetric(&frame);

    draw_timer.emplace();
    overlay_processor_->SetFrameSequenceNumber(frame_sequence_number_);
    overlay_processor_->SetIsPageFullscreen(frame.page_fullscreen_mode);
    renderer_->DrawFrame(&frame.render_pass_list, device_scale_factor_,
                         current_surface_size, display_color_spaces_,
                         std::move(frame.surface_damage_rect_list_));
  } else {
    TRACE_EVENT_INSTANT0("viz", "Draw skipped.", TRACE_EVENT_SCOPE_THREAD);
  }

  bool should_swap = !disable_swap_until_resize_ && should_draw && size_matches;
  if (should_swap) {
    PresentationGroupTiming& presentation_group_timing =
        pending_presentation_group_timings_.emplace_back();

    base::flat_set<base::PlatformThreadId> thread_ids;
    for (const auto& surface_id : aggregator_->previous_contained_surfaces()) {
      surface = surface_manager_->GetSurfaceForId(surface_id);
      if (surface) {
        base::flat_set<base::PlatformThreadId> surface_thread_ids =
            surface->GetThreadIds();
        thread_ids.insert(surface_thread_ids.begin(), surface_thread_ids.end());
      }
    }

    HintSession::BoostType boost_type = HintSession::BoostType::kDefault;
    if (IsScroll(frame.latency_info)) {
      boost_type = HintSession::BoostType::kScrollBoost;
    }
    presentation_group_timing.OnDraw(params.frame_time,
                                     draw_timer->start_time(),
                                     std::move(thread_ids), boost_type);

    for (const auto& surface_id : aggregator_->previous_contained_surfaces()) {
      surface = surface_manager_->GetSurfaceForId(surface_id);
      if (surface) {
        std::unique_ptr<Surface::PresentationHelper> helper =
            surface->TakePresentationHelperForPresentNotification();
        if (helper) {
          presentation_group_timing.AddPresentationHelper(std::move(helper));
        }
      }
    }

    TRACE_EVENT_ASYNC_STEP_INTO0("viz,benchmark",
                                 "Graphics.Pipeline.DrawAndSwap",
                                 swapped_trace_id_, "WaitForSwap");
    swapped_since_resize_ = true;

    IssueDisplayRenderingStatsEvent();
    DirectRenderer::SwapFrameData swap_frame_data;
    swap_frame_data.latency_info = std::move(frame.latency_info);
    swap_frame_data.seq =
        current_surface_id_.local_surface_id().parent_sequence_number();
    swap_frame_data.choreographer_vsync_id = params.choreographer_vsync_id;
    swap_frame_data.swap_trace_id = swapped_trace_id_;
    swap_frame_data.display_hdr_headroom =
        display_color_spaces_.GetHDRMaxLuminanceRelative();

    TRACE_EVENT(
        "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
        perfetto::Flow::Global(swap_frame_data.swap_trace_id),
        [swap_trace_id =
             swap_frame_data.swap_trace_id](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_chrome_graphics_pipeline();
          data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                             StepName::STEP_SEND_BUFFER_SWAP);
          data->set_display_trace_id(swap_trace_id);
        });

#if BUILDFLAG(IS_APPLE)
    swap_frame_data.ca_layer_error_code =
        overlay_processor_->GetCALayerErrorCode();
#endif

    // We must notify scheduler and increase |pending_swaps_| before calling
    // SwapBuffers() as it can call DidReceiveSwapBuffersAck synchronously.
    if (scheduler_)
      scheduler_->DidSwapBuffers();
    pending_swaps_++;

    UMA_HISTOGRAM_COUNTS_100("Compositing.Display.PendingSwaps",
                             pending_swaps_);

    renderer_->SwapBuffers(std::move(swap_frame_data));
  } else {
    TRACE_EVENT_INSTANT0("viz", "Swap skipped.", TRACE_EVENT_SCOPE_THREAD);

    if (have_damage && !size_matches)
      aggregator_->SetFullDamageForSurface(current_surface_id_);

    if (have_damage) {
      // Do not store more than the allowed size.
      if (ui::LatencyInfo::Verify(frame.latency_info, "Display::DrawAndSwap")) {
        stored_latency_info_.swap(frame.latency_info);
      }
    } else {
      // There was no damage. Terminate the latency info objects.
      while (!frame.latency_info.empty()) {
        auto& latency = frame.latency_info.back();
        latency.Terminate();
        frame.latency_info.pop_back();
      }
    }

    // If we did draw, but not going to swap we need notify DirectRenderer that
    // swap buffers will be skipped.
    if (should_draw)
      renderer_->SwapBuffersSkipped();

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

void Display::DidReceiveSwapBuffersAck(
    const gpu::SwapBuffersCompleteParams& params,
    gfx::GpuFenceHandle release_fence) {
  // Adding to |pending_presentation_group_timings_| must
  // have been done in DrawAndSwap(), and should not be popped until
  // DidReceiveSwapBuffersAck.
  DCHECK(!pending_presentation_group_timings_.empty());

  TRACE_EVENT(
      "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
      perfetto::TerminatingFlow::Global(params.swap_trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_chrome_graphics_pipeline();
        data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                           StepName::STEP_SWAP_BUFFERS_ACK);
        data->set_display_trace_id(params.swap_trace_id);
      });

  // Both cases require full damage. That is, if buffers are recreated or
  // non-simple overlays failed, a frame is expected to be sent again.
  if (params.swap_response.result ==
          gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS ||
      params.swap_response.result ==
          gfx::SwapResult::SWAP_NON_SIMPLE_OVERLAYS_FAILED) {
    aggregator_->SetFullDamageForSurface(current_surface_id_);
    damage_tracker_->SetRootSurfaceDamaged();
  }

  const gfx::SwapTimings& timings = params.swap_response.timings;
  ++last_swap_ack_trace_id_;
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap", last_swap_ack_trace_id_,
      "Swap", timings.swap_start);
  TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap", last_swap_ack_trace_id_,
      "WaitForPresentation", timings.swap_end);

  if (overlay_processor_)
    overlay_processor_->OverlayPresentationComplete();
  if (renderer_) {
    renderer_->SwapBuffersComplete(params, std::move(release_fence));
  }

  DCHECK_GT(pending_swaps_, 0);
  pending_swaps_--;
  if (scheduler_) {
    scheduler_->DidReceiveSwapBuffersAck();
  }

  if (no_pending_swaps_callback_ && pending_swaps_ == 0)
    std::move(no_pending_swaps_callback_).Run();

  // It's possible to receive multiple calls to DidReceiveSwapBuffersAck()
  // before DidReceivePresentationFeedback(). Ensure that we're not setting
  // |swap_timings_| for the same PresentationGroupTiming multiple times.
  base::TimeTicks draw_start_timestamp;
  for (auto& group_timing : pending_presentation_group_timings_) {
    if (!group_timing.HasSwapped()) {
      group_timing.OnSwap(timings, scheduler_.get());
      draw_start_timestamp = group_timing.draw_start_timestamp();
      break;
    }
  }

  // We should have at least one group that hasn't received a SwapBuffersAck
  DCHECK(!draw_start_timestamp.is_null());

  // Check that the swap timings correspond with the timestamp from when
  // the swap was triggered. Note that not all output surfaces provide timing
  // information, hence the check for a valid swap_start.

  if (!timings.swap_start.is_null()) {
    DCHECK_LE(draw_start_timestamp, timings.swap_start);
    base::TimeDelta draw_start_to_swap_start =
        timings.swap_start - draw_start_timestamp;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.Display.DrawToSwapUs", draw_start_to_swap_start,
        kDrawToSwapMin, kDrawToSwapMax, kDrawToSwapUsBuckets);
  }

  if (!timings.viz_scheduled_draw.is_null()) {
    DCHECK(!timings.gpu_started_draw.is_null());
    DCHECK_LE(timings.viz_scheduled_draw, timings.gpu_started_draw);
    base::TimeDelta schedule_draw_to_gpu_start =
        timings.gpu_started_draw - timings.viz_scheduled_draw;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.Display.VizScheduledDrawToGpuStartedDrawUs",
        schedule_draw_to_gpu_start, kDrawToSwapMin, kDrawToSwapMax,
        kDrawToSwapUsBuckets);
  }

  if (!timings.gpu_task_ready.is_null()) {
    DCHECK(!timings.viz_scheduled_draw.is_null());
    DCHECK(!timings.gpu_started_draw.is_null());
    DCHECK_LE(timings.viz_scheduled_draw, timings.gpu_task_ready);
    DCHECK_LE(timings.gpu_task_ready, timings.gpu_started_draw);
    base::TimeDelta dependency_delta =
        timings.gpu_task_ready - timings.viz_scheduled_draw;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.Display.VizScheduledDrawToDependencyResolvedUs",
        dependency_delta, kDrawToSwapMin, kDrawToSwapMax, kDrawToSwapUsBuckets);
    base::TimeDelta scheduling_delta =
        timings.gpu_started_draw - timings.gpu_task_ready;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.Display.VizDependencyResolvedToGpuStartedDrawUs",
        scheduling_delta, kDrawToSwapMin, kDrawToSwapMax, kDrawToSwapUsBuckets);
  }
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
  if (renderer_)
    renderer_->BuffersPresented();

  if (pending_presentation_group_timings_.empty()) {
    DLOG(ERROR) << "Received unexpected PresentationFeedback";
    return;
  }
  auto& presentation_group_timing = pending_presentation_group_timings_.front();
  auto copy_feedback = SanitizePresentationFeedback(
      feedback, presentation_group_timing.draw_start_timestamp());
  ++last_presented_trace_id_;
  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
      "viz,benchmark", "Graphics.Pipeline.DrawAndSwap",
      last_presented_trace_id_, copy_feedback.timestamp);
  TRACE_EVENT_INSTANT_WITH_TIMESTAMP0(
      "benchmark,viz," TRACE_DISABLED_BY_DEFAULT("display.framedisplayed"),
      "Display::FrameDisplayed", TRACE_EVENT_SCOPE_THREAD,
      copy_feedback.timestamp);

  if (renderer_->CompositeTimeTracingEnabled()) {
    if (copy_feedback.ready_timestamp.is_null()) {
      LOG(WARNING) << "Ready Timestamp unavailable";
    } else {
      renderer_->AddCompositeTimeTraces(copy_feedback.ready_timestamp);
    }
  }

  presentation_group_timing.OnPresent(copy_feedback);
  pending_presentation_group_timings_.pop_front();
}

void Display::DidReceiveReleasedOverlays(
    const std::vector<gpu::Mailbox>& released_overlays) {
  if (renderer_)
    renderer_->DidReceiveReleasedOverlays(released_overlays);
}

void Display::AddChildWindowToBrowser(gpu::SurfaceHandle child_window) {
  if (client_) {
    client_->DisplayAddChildWindowToBrowser(child_window);
  }
}

void Display::DidFinishFrame(const BeginFrameAck& ack) {
  for (auto& observer : observers_)
    observer.OnDisplayDidFinishFrame(ack);

  // Prevent a delegated ink trail from staying on the screen
  // for more than one frame by forcing a new frame to be produced.
  if (!renderer_->GetDelegatedInkTrailDamageRect().IsEmpty()) {
    scheduler_->SetNeedsOneBeginFrame(true);
  }

  frame_sequence_number_ = ack.frame_id.sequence_number;
}

const SurfaceId& Display::CurrentSurfaceId() const {
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
    scheduler_->SetNeedsOneBeginFrame(false);
}

void Display::SetPreferredFrameInterval(base::TimeDelta interval) {
#if BUILDFLAG(IS_ANDROID)
  if (OutputSurfaceSupportsSetFrameRate()) {
    SetFrameIntervalOnOutputSurface(interval);
    return;
  }
#endif

  client_->SetPreferredFrameInterval(interval);
}

base::TimeDelta Display::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id,
    mojom::CompositorFrameSinkType* type) {
  return client_->GetPreferredFrameIntervalForFrameSinkId(id, type);
}

void Display::SetSupportedFrameIntervals(
    base::flat_set<base::TimeDelta> intervals) {
  if (frame_rate_decider_) {
    frame_rate_decider_->SetSupportedFrameIntervals(std::move(intervals));
  }
}

void Display::SetHwSupportForMultipleRefreshRates(bool support) {
  if (frame_rate_decider_) {
    frame_rate_decider_->SetHwSupportForMultipleRefreshRates(support);
  }
}

#if BUILDFLAG(IS_ANDROID)
bool Display::OutputSurfaceSupportsSetFrameRate() {
  return output_surface_ &&
         output_surface_->capabilities().supports_surfaceless &&
         gfx::SurfaceControl::SupportsSetFrameRate();
}

void Display::SetFrameIntervalOnOutputSurface(base::TimeDelta interval) {
  float interval_s = interval.InSecondsF();
  float frame_rate = interval_s == 0 ? 0 : (1 / interval_s);
  output_surface_->SetFrameRate(frame_rate);
}

base::ScopedClosureRunner Display::GetCacheBackBufferCb() {
  return output_surface_->GetCacheBackBufferCb();
}
#endif

void Display::DisableGPUAccessByDefault() {
  DCHECK(resource_provider_);
  resource_provider_->SetAllowAccessToGPUThread(false);
}

void Display::PreserveChildSurfaceControls() {
  if (skia_output_surface_) {
    skia_output_surface_->PreserveChildSurfaceControls();
  }
}

void Display::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  if (std::optional<switches::DelegatedInkRendererMode> mode =
          switches::GetDelegatedInkRendererMode()) {
    switch (mode.value()) {
      case switches::DelegatedInkRendererMode::kSkia:
        if (DelegatedInkPointRendererBase* ink_renderer =
                renderer_->GetDelegatedInkPointRenderer(
                    /*create_if_necessary=*/true)) {
          ink_renderer->InitMessagePipeline(std::move(pending_receiver));
        }
        break;
      case switches::DelegatedInkRendererMode::kSystem:
        if (DoesPlatformSupportDelegatedInk()) {
          output_surface_->InitDelegatedInkPointRendererReceiver(
              std::move(pending_receiver));
        }
        break;
      case switches::DelegatedInkRendererMode::kNone:
        // Do not initialize a receiver for `kNone` or any other values.
        return;
    }
    return;
  }

  if (DoesPlatformSupportDelegatedInk() && output_surface_) {
    output_surface_->InitDelegatedInkPointRendererReceiver(
        std::move(pending_receiver));
  } else if (DelegatedInkPointRendererBase* ink_renderer =
                 renderer_->GetDelegatedInkPointRenderer(
                     /*create_if_necessary=*/true)) {
    ink_renderer->InitMessagePipeline(std::move(pending_receiver));
  }
}

void Display::ResetDisplayClientForTesting(DisplayClient* old_client) {
  CHECK_EQ(client_, old_client);
  client_ = nullptr;
}

}  // namespace viz
