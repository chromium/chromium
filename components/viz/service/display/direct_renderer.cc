// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/direct_renderer.h"

#include <limits.h>
#include <stddef.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/circular_deque.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/timer/elapsed_timer.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/color_space_utils.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/debugger/viz_debugger.h"
#include "components/viz/service/display/bsp_tree.h"
#include "components/viz/service/display/bsp_walk_action.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/render_pass_alpha_type.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "media/base/video_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/display_color_spaces.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"

namespace viz {

namespace {

// Allow skipping Begin/EndDraw on the shared image backing for non-root render
// passes if the computed update rect would mean nothing would be drawn.
// This is a kill switch in case something depends on an empty update.
BASE_FEATURE(kAllowSkipEmptyNonrootRenderPassDraws,
             "AllowSkipEmptyNonrootRenderPassDraws",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enum used for UMA histogram. These enum values must not be changed or
// reused.
enum class RenderPassDrawRectAssign {
  // New assignment. The output was empty before this point.
  kNewOutputRect = 0,
  // Assignment to a drawn rect that already has been set. This is likely an
  // expansion of the 'output_rect' of a render pass.
  kReassign = 1,
  //  Output rects match. No re-assignment was done.
  kNoAssign = 2,
  // Assigned 'output_rect' to drawn rect but it was full damage regardless.
  kFullDamage = 3,
  kMaxValue = kFullDamage,
};

}  // namespace

DirectRenderer::DrawingFrame::DrawingFrame() = default;
DirectRenderer::DrawingFrame::~DrawingFrame() = default;

DirectRenderer::SwapFrameData::SwapFrameData() = default;
DirectRenderer::SwapFrameData::~SwapFrameData() = default;
DirectRenderer::SwapFrameData::SwapFrameData(SwapFrameData&&) = default;
DirectRenderer::SwapFrameData& DirectRenderer::SwapFrameData::operator=(
    SwapFrameData&&) = default;

DirectRenderer::DirectRenderer(const RendererSettings* settings,
                               const DebugRendererSettings* debug_settings,
                               OutputSurface* output_surface,
                               DisplayResourceProvider* resource_provider,
                               OverlayProcessorInterface* overlay_processor)
    : settings_(settings),
      debug_settings_(debug_settings),
      output_surface_(output_surface),
      resource_provider_(resource_provider),
      overlay_processor_(overlay_processor),
      allow_undamaged_nonroot_render_pass_to_skip_(base::FeatureList::IsEnabled(
          features::kAllowUndamagedNonrootRenderPassToSkip)),
      use_render_pass_drawn_rect_(
          base::FeatureList::IsEnabled(features::kRenderPassDrawnRect)) {
  DCHECK(output_surface_);
}

DirectRenderer::~DirectRenderer() = default;

void DirectRenderer::Initialize() {
  use_partial_swap_ = settings_->partial_swap_enabled && CanPartialSwap();
  initialized_ = true;
}

// static
gfx::RectF DirectRenderer::QuadVertexRect() {
  return gfx::RectF(-0.5f, -0.5f, 1.f, 1.f);
}

// static
void DirectRenderer::QuadRectTransform(gfx::Transform* quad_rect_transform,
                                       const gfx::Transform& quad_transform,
                                       const gfx::RectF& quad_rect) {
  *quad_rect_transform = quad_transform;
  quad_rect_transform->Translate(0.5 * quad_rect.width() + quad_rect.x(),
                                 0.5 * quad_rect.height() + quad_rect.y());
  quad_rect_transform->Scale(quad_rect.width(), quad_rect.height());
}

gfx::AxisTransform2d DirectRenderer::CalculateTargetToDeviceTransform(
    const gfx::Rect& draw_rect,
    const gfx::Size& viewport_size) {
  gfx::AxisTransform2d target_to_device_transform =
      gfx::OrthoProjectionTransform(draw_rect.x(), draw_rect.right(),
                                    draw_rect.y(), draw_rect.bottom());
  target_to_device_transform.PostConcat(gfx::WindowTransform(
      0, 0, viewport_size.width(), viewport_size.height()));
  return target_to_device_transform;
}

gfx::Rect DirectRenderer::MoveFromDrawToWindowSpace(
    const gfx::Rect& draw_rect) const {
  gfx::Rect window_rect = draw_rect;
  window_rect -=
      current_frame()->current_render_pass->output_rect.OffsetFromOrigin();
  return window_rect;
}

const DrawQuad* DirectRenderer::CanPassBeDrawnDirectly(
    const AggregatedRenderPass* pass,
    const RenderPassRequirements& requirements) {
  return nullptr;
}

void DirectRenderer::SetOutputSurfaceClipRect(const gfx::Rect& clip_rect) {
  output_surface_clip_rect_ = clip_rect;
}

void DirectRenderer::SetVisible(bool visible) {
  DCHECK(initialized_);
  if (visible_ == visible)
    return;
  visible_ = visible;
  DidChangeVisibility();
}

void DirectRenderer::ReallocatedFrameBuffers() {
  next_frame_needs_full_frame_redraw_ = true;
}

void DirectRenderer::Reshape(
    const OutputSurface::ReshapeParams& reshape_params) {
  output_surface_->Reshape(reshape_params);
}

void DirectRenderer::DecideRenderPassAllocationsForFrame(
    const AggregatedRenderPassList& render_passes_in_draw_order) {
  DCHECK(render_pass_bypass_quads_.empty());

  auto& root_render_pass = render_passes_in_draw_order.back();

  base::flat_map<AggregatedRenderPassId, RenderPassRequirements>
      render_passes_in_frame;
  for (const auto& pass : render_passes_in_draw_order) {
    const bool is_root = pass == root_render_pass;

#if BUILDFLAG(IS_WIN)
    // For delegated compositing the root pass is preserved, but not rendered.
    // If a previous frame fell out of delegated compositing we want to make
    // sure that we deallocate its backing when switching back to delegated
    // compositing.
    if (is_root && output_surface_->capabilities().renderer_allocates_images &&
        !current_frame()->output_surface_plane) {
      // We expect to be in delegated compositing mode, which means the root
      // damage rect has been cleared.
      CHECK(current_frame()->root_damage_rect.IsEmpty());
      continue;
    }
#else
    // TODO(crbug.com/40224327): Consider deallocating the primary plane in this
    // case.
    // Non-Windows platforms use BufferQueue, which are not owned by the render
    // pass backing. ChromeOS must hold on to the root surface buffers to ensure
    // overlay-ability and macOS wants to just discard the underlying surfaces
    // for performance.
#endif

    const RenderPassRequirements requirements =
        CalculateRenderPassRequirements(pass.get());

    // If there's a copy request, we need an explicit renderpass backing so
    // only try to draw directly if there are no copy requests.
    if (!is_root && pass->copy_requests.empty()) {
      if (const DrawQuad* quad =
              CanPassBeDrawnDirectly(pass.get(), requirements)) {
        // If the render pass is drawn directly, it will not be drawn from as
        // a render pass so it's not added to the map.
        render_pass_bypass_quads_[pass->id] = quad;
        continue;
      }
    }

    render_passes_in_frame[pass->id] = requirements;
  }
  UMA_HISTOGRAM_COUNTS_1000(
      "Compositing.Display.FlattenedRenderPassCount",
      base::saturated_cast<int>(render_passes_in_draw_order.size() -
                                render_pass_bypass_quads_.size()));
  UpdateRenderPassTextures(render_passes_in_draw_order, render_passes_in_frame);
}

void DirectRenderer::DrawFrame(
    AggregatedRenderPassList* render_passes_in_draw_order,
    float device_scale_factor,
    const gfx::Size& device_viewport_size,
    const gfx::DisplayColorSpaces& display_color_spaces,
    SurfaceDamageRectList surface_damage_rect_list) {
  DCHECK(visible_);
  TRACE_EVENT0("viz,benchmark", "DirectRenderer::DrawFrame");

  auto* root_render_pass = render_passes_in_draw_order->back().get();
  DCHECK(root_render_pass);

  current_frame_valid_ = true;
  current_frame_ = DrawingFrame();
  current_frame()->render_passes_in_draw_order = render_passes_in_draw_order;
  current_frame()->root_render_pass = root_render_pass;
  current_frame()->root_damage_rect = root_render_pass->damage_rect;
  if (overlay_processor_) {
    current_frame()->root_damage_rect.Union(
        overlay_processor_->GetAndResetOverlayDamage());
  }

  if (auto* ink_renderer =
          GetDelegatedInkPointRenderer(/*create_if_necessary=*/false)) {
    // The path must be finalized before GetDamageRect() can return an
    // accurate rect that will allow the old trail to be removed and the new
    // trail to be drawn at the same time.
    ink_renderer->FinalizePathForDraw();
  }
  AddInkDamageToRenderPass(current_frame()->root_render_pass,
                           current_frame()->root_damage_rect);

  current_frame()->root_damage_rect.Intersect(gfx::Rect(device_viewport_size));
  current_frame()->device_viewport_size = device_viewport_size;
  current_frame()->display_color_spaces = display_color_spaces;

  output_surface_->SetNeedsMeasureNextDrawLatency();
  BeginDrawingFrame();

  // RenderPass owns filters, backdrop_filters, etc., and will outlive this
  // function call. So it is safe to store pointers in these maps.
  for (const auto& pass : *render_passes_in_draw_order) {
    if (!pass->filters.IsEmpty()) {
      render_pass_filters_[pass->id] = &pass->filters;
      if (pass->filters.HasFilterThatMovesPixels())
        has_pixel_moving_foreground_filters_ = true;
    }
    if (!pass->backdrop_filters.IsEmpty()) {
      render_pass_backdrop_filters_[pass->id] = &pass->backdrop_filters;
      render_pass_backdrop_filter_bounds_[pass->id] =
          pass->backdrop_filter_bounds;
      if (pass->backdrop_filters.HasFilterThatMovesPixels()) {
        backdrop_filter_output_rects_[pass->id] =
            cc::MathUtil::MapEnclosingClippedRect(
                pass->transform_to_root_target, pass->output_rect);
      }
    }
  }

  bool frame_has_alpha =
      current_frame()->root_render_pass->has_transparent_background;
  gfx::ColorSpace frame_color_space =
      RenderPassColorSpace(current_frame()->root_render_pass);
  SharedImageFormat frame_si_format = GetSharedImageFormat(
      current_frame()->display_color_spaces.GetOutputBufferFormat(
          current_frame()->root_render_pass->content_color_usage,
          frame_has_alpha));
  gfx::Size surface_resource_size =
      CalculateSizeForOutputSurface(device_viewport_size);
  if (overlay_processor_) {
    // Display transform and viewport size are needed for overlay validator on
    // Android SurfaceControl, and viewport size is need on Windows. These need
    // to be called before ProcessForOverlays.
    overlay_processor_->SetDisplayTransformHint(
        output_surface_->GetDisplayTransform());
    overlay_processor_->SetViewportSize(device_viewport_size);

    // Before ProcessForOverlay calls into the hardware to ask about whether the
    // overlay setup can be handled, we need to set up the primary plane.
    OverlayProcessorInterface::OutputSurfaceOverlayPlane* primary_plane =
        nullptr;
    if (output_surface_->capabilities().renderer_allocates_images) {
      // TODO(crbug.com/40224327): `output_surface_plane` can be changed to an
      // OverlayCandidate now.
      current_frame()->output_surface_plane =
          overlay_processor_->ProcessOutputSurfaceAsOverlay(
              device_viewport_size, surface_resource_size, frame_si_format,
              frame_color_space, frame_has_alpha, 1.0f /*opacity*/,
              GetPrimaryPlaneOverlayTestingMailbox());
      primary_plane = &(current_frame()->output_surface_plane.value());
    }

    // Attempt to replace some or all of the quads of the root render pass with
    // overlays.
    base::ElapsedTimer overlay_processing_timer;
    overlay_processor_->ProcessForOverlays(
        resource_provider_, render_passes_in_draw_order,
        output_surface_->color_matrix(), render_pass_filters_,
        render_pass_backdrop_filters_, std::move(surface_damage_rect_list),
        primary_plane, &current_frame()->overlay_list,
        &current_frame()->root_damage_rect,
        &current_frame()->root_content_bounds);
    auto overlay_processing_time = overlay_processing_timer.Elapsed();

    constexpr auto kMinTime = base::Microseconds(5);
    constexpr auto kMaxTime = base::Milliseconds(10);
    constexpr int kTimeBuckets = 50;
    UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
        "Compositing.DirectRenderer.OverlayProcessingUs",
        overlay_processing_time, kMinTime, kMaxTime, kTimeBuckets);

    // If we promote any quad to an underlay then the main plane must support
    // alpha.
    // TODO(ccameron): We should update |frame_color_space|, and
    // |frame_si_format| based on the change in |frame_has_alpha|.
    if (current_frame()->output_surface_plane) {
      frame_has_alpha |= current_frame()->output_surface_plane->enable_blending;
      root_render_pass->has_transparent_background = frame_has_alpha;
    }

    overlay_processor_->AdjustOutputSurfaceOverlay(
        &(current_frame()->output_surface_plane));
  }

  // Only reshape when we know we are going to draw. Otherwise, the reshape
  // can leave the window at the wrong size if we never draw and the proper
  // viewport size is never set.
  skipped_render_pass_ids_.clear();
  bool needs_full_frame_redraw = false;
  auto display_transform = output_surface_->GetDisplayTransform();
  OutputSurface::ReshapeParams reshape_params;
  reshape_params.size = surface_resource_size;
  reshape_params.device_scale_factor = device_scale_factor;
  reshape_params.color_space = frame_color_space;
  reshape_params.format = frame_si_format;
  reshape_params.alpha_type = frame_has_alpha ? RenderPassAlphaType::kPremul
                                              : RenderPassAlphaType::kOpaque;
  if (next_frame_needs_full_frame_redraw_ ||
      reshape_params != reshape_params_ ||
      display_transform != reshape_display_transform_) {
    next_frame_needs_full_frame_redraw_ = false;
    reshape_params_ = reshape_params;
    reshape_display_transform_ = display_transform;
    Reshape(reshape_params);
#if BUILDFLAG(IS_APPLE)
    // For Mac, all render passes will be promoted to CALayer, the redraw full
    // frame is for the main surface only.
    // TODO(penghuang): verify this logic with SkiaRenderer.
    if (!output_surface_->capabilities().supports_surfaceless)
      needs_full_frame_redraw = true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
    // If compositing is delegated, then there will be no output_surface_plane,
    // and we should not trigger a redraw of the root render pass.
    // Pixel tests will not be displayed as overlay planes, so they need redraw.
    if (current_frame()->output_surface_plane ||
        !output_surface_->capabilities().renderer_allocates_images) {
      needs_full_frame_redraw = true;
    }
#else
    // The entire surface has to be redrawn if reshape is requested.
    needs_full_frame_redraw = true;
#endif
  }

  // DecideRenderPassAllocationsForFrame needs
  // current_frame()->display_color_spaces to decide the color space
  // of each render pass. Overlay processing is also allowed to modify the
  // render pass backing requirements due to e.g. a underlay promotion. On
  // Windows, the root render pass' size is based on the |reshape_params_|.
  DecideRenderPassAllocationsForFrame(*render_passes_in_draw_order);

  // Draw all non-root render passes except for the root render pass.
  total_pixels_rendered_this_frame_ = 0;
  for (const auto& pass : *render_passes_in_draw_order) {
    if (pass.get() == root_render_pass)
      break;
    DrawRenderPassAndExecuteCopyRequests(pass.get());
  }

  bool skip_drawing_root_render_pass =
      current_frame()->root_damage_rect.IsEmpty() && use_partial_swap_ &&
      !needs_full_frame_redraw;

  // If partial swap is not used, and the frame can not be skipped, the whole
  // frame has to be redrawn.
  if (!use_partial_swap_ && !skip_drawing_root_render_pass)
    needs_full_frame_redraw = true;

  // If we need to redraw the frame, the whole output should be considered
  // damaged.
  if (needs_full_frame_redraw)
    current_frame()->root_damage_rect = gfx::Rect(device_viewport_size);

  if (!skip_drawing_root_render_pass) {
    DrawRenderPassAndExecuteCopyRequests(root_render_pass);
  }

  // Displays 4k in size or greater are relatively common.
  constexpr uint64_t kMaxPixelCount = 30'000'000;
  constexpr uint64_t kMinPixelCount = 1;
  constexpr uint64_t kNumBucketsPixelCount = 50;
  UMA_HISTOGRAM_CUSTOM_COUNTS("Compositing.DirectRenderer.TotalPixelsRendered",
                              total_pixels_rendered_this_frame_, kMinPixelCount,
                              kMaxPixelCount, kNumBucketsPixelCount);

  // Data focused on pixel counts closer to screen resolution sizes.
  constexpr base::Histogram::Sample kHistogramScale = 100 * 1024;
  constexpr uint64_t kNumberOfBucketsLinear = 100;
  UMA_HISTOGRAM_SCALED_EXACT_LINEAR(
      "Compositing.DirectRenderer.TotalPixelsRenderedNarrow",
      total_pixels_rendered_this_frame_, 1, kNumberOfBucketsLinear + 1,
      kHistogramScale);

  DBG_LOG("direct.renderer.total_pixels", "Total Pixels: %" PRIu64,
          total_pixels_rendered_this_frame_);

  if (overlay_processor_)
    overlay_processor_->TakeOverlayCandidates(&current_frame()->overlay_list);

  FinishDrawingFrame();

  if (overlay_processor_)
    overlay_processor_->ScheduleOverlays(resource_provider_);

  // Total non-root render pass count, excluding root render pass and bypassed
  // render passes.
  auto nonroot_render_pass_count = render_passes_in_draw_order->size() - 1 -
                                   render_pass_bypass_quads_.size();
  if (nonroot_render_pass_count > 0) {
    UMA_HISTOGRAM_BOOLEAN(
        "Compositing.DirectRenderer.SkipAllNonRootRenderPassesPerFrame",
        skipped_render_pass_ids_.size() == nonroot_render_pass_count);
  }

  // The current drawing frame is valid only during the duration of this
  // function. Clear the pointers held inside to avoid holding dangling
  // pointers.
  current_frame()->render_passes_in_draw_order = nullptr;
  current_frame()->root_render_pass = nullptr;

  render_passes_in_draw_order->clear();
  render_pass_filters_.clear();
  render_pass_backdrop_filters_.clear();
  render_pass_backdrop_filter_bounds_.clear();
  render_pass_bypass_quads_.clear();
  backdrop_filter_output_rects_.clear();
  has_pixel_moving_foreground_filters_ = false;

  current_frame_valid_ = false;
}

gfx::Rect DirectRenderer::GetCurrentFramebufferDamage() const {
  return output_surface_->GetCurrentFramebufferDamage();
}

gfx::Rect DirectRenderer::GetTargetDamageBoundingRect() const {
  if (use_render_pass_drawn_rect_) {
    return gfx::Rect();
  }

  gfx::Rect bounding_rect = GetCurrentFramebufferDamage();
  if (overlay_processor_) {
    bounding_rect.Union(
        overlay_processor_->GetPreviousFrameOverlaysBoundingRect());
  }
  return bounding_rect;
}

gfx::Rect DirectRenderer::DeviceViewportRectInDrawSpace() const {
  gfx::Rect device_viewport_size(current_frame()->device_viewport_size);
  device_viewport_size +=
      current_frame()->root_render_pass->output_rect.OffsetFromOrigin();
  return device_viewport_size;
}

gfx::Rect DirectRenderer::OutputSurfaceRectInDrawSpace() const {
  if (current_frame()->current_render_pass ==
      current_frame()->root_render_pass) {
    return DeviceViewportRectInDrawSpace();
  } else {
    return current_frame()->current_render_pass->output_rect;
  }
}

bool DirectRenderer::ShouldSkipQuad(const DrawQuad& quad,
                                    const gfx::Rect& render_pass_scissor) {
  if (render_pass_scissor.IsEmpty())
    return true;

  gfx::Rect target_rect = quad.visible_rect;

  auto* rpdq = quad.DynamicCast<AggregatedRenderPassDrawQuad>();
  if (rpdq) {
    // Render pass draw quads can have pixel-moving filters that expand their
    // visible bounds.
    auto filter_it = render_pass_filters_.find(rpdq->render_pass_id);
    if (filter_it != render_pass_filters_.end()) {
      target_rect = filter_it->second->ExpandRectForPixelMovement(target_rect);
    }
  }

  target_rect = cc::MathUtil::MapEnclosingClippedRect(
      quad.shared_quad_state->quad_to_target_transform, target_rect);

  if (quad.shared_quad_state->clip_rect) {
    target_rect.Intersect(*quad.shared_quad_state->clip_rect);
  }

  target_rect.Intersect(render_pass_scissor);
  return target_rect.IsEmpty();
}

void DirectRenderer::SetScissorStateForQuad(
    const DrawQuad& quad,
    const gfx::Rect& render_pass_scissor,
    bool use_render_pass_scissor) {
  if (use_render_pass_scissor) {
    gfx::Rect quad_scissor_rect = render_pass_scissor;
    if (quad.shared_quad_state->clip_rect)
      quad_scissor_rect.Intersect(*quad.shared_quad_state->clip_rect);
    SetScissorTestRectInDrawSpace(quad_scissor_rect);
    return;
  } else if (quad.shared_quad_state->clip_rect) {
    SetScissorTestRectInDrawSpace(*quad.shared_quad_state->clip_rect);
    return;
  }

  EnsureScissorTestDisabled();
}

void DirectRenderer::SetScissorTestRectInDrawSpace(
    const gfx::Rect& draw_space_rect) {
  gfx::Rect window_space_rect = MoveFromDrawToWindowSpace(draw_space_rect);
  SetScissorTestRect(window_space_rect);
}

void DirectRenderer::DoDrawPolygon(const DrawPolygon& poly,
                                   const gfx::Rect& render_pass_scissor,
                                   bool use_render_pass_scissor) {
  SetScissorStateForQuad(*poly.original_ref(), render_pass_scissor,
                         use_render_pass_scissor);

  // If the poly has not been split, then it is just a normal DrawQuad,
  // and we should save any extra processing that would have to be done.
  if (!poly.is_split()) {
    DoDrawQuad(poly.original_ref(), nullptr);
    return;
  }

  std::vector<gfx::QuadF> quads;
  poly.ToQuads2D(&quads);
  for (size_t i = 0; i < quads.size(); ++i) {
    DoDrawQuad(poly.original_ref(), &quads[i]);
  }
}

const cc::FilterOperations* DirectRenderer::FiltersForPass(
    AggregatedRenderPassId render_pass_id) const {
  auto it = render_pass_filters_.find(render_pass_id);
  return it == render_pass_filters_.end() ? nullptr : it->second;
}

const cc::FilterOperations* DirectRenderer::BackdropFiltersForPass(
    AggregatedRenderPassId render_pass_id) const {
  auto it = render_pass_backdrop_filters_.find(render_pass_id);
  return it == render_pass_backdrop_filters_.end() ? nullptr : it->second;
}

const std::optional<gfx::RRectF> DirectRenderer::BackdropFilterBoundsForPass(
    AggregatedRenderPassId render_pass_id) const {
  auto it = render_pass_backdrop_filter_bounds_.find(render_pass_id);
  return it == render_pass_backdrop_filter_bounds_.end()
             ? std::optional<gfx::RRectF>()
             : it->second;
}

bool DirectRenderer::SupportsBGRA() const {
  // TODO(penghuang): check supported format correctly.
  return true;
}

void DirectRenderer::FlushPolygons(
    base::circular_deque<std::unique_ptr<DrawPolygon>>* poly_list,
    const gfx::Rect& render_pass_scissor,
    bool use_render_pass_scissor) {
  if (poly_list->empty()) {
    return;
  }

  BspTree bsp_tree(poly_list);
  BspWalkActionDrawPolygon action_handler(this, render_pass_scissor,
                                          use_render_pass_scissor);
  bsp_tree.TraverseWithActionHandler(&action_handler);
  DCHECK(poly_list->empty());
}

void DirectRenderer::DrawRenderPassAndExecuteCopyRequests(
    AggregatedRenderPass* render_pass) {
  base::AutoReset<raw_ptr<const AggregatedRenderPass>> current_render_pass(
      &current_frame()->current_render_pass, render_pass);

  if (render_pass_bypass_quads_.find(render_pass->id) !=
      render_pass_bypass_quads_.end()) {
    return;
  }

  // Repeated draw to simulate a slower device for the evaluation of performance
  // improvements in UI effects.
  for (int i = 0; i < settings_->slow_down_compositing_scale_factor; ++i)
    DrawRenderPass(render_pass);

  for (auto& request : render_pass->copy_requests) {
    // Finalize the source subrect (output_rect, result_bounds,
    // sampling_bounds), as the entirety of the RenderPass's output optionally
    // clamped to the requested copy area. Then, compute the result rect
    // (result_selection), which is the selection clamped to the maximum
    // possible result bounds. If there will be zero pixels of output or the
    // scaling ratio was not reasonable, do not proceed.
    gfx::Rect output_rect = render_pass->output_rect;
    if (request->has_area())
      output_rect.Intersect(request->area());

    copy_output::RenderPassGeometry geometry;
    geometry.result_bounds =
        request->is_scaled() ? copy_output::ComputeResultRect(
                                   gfx::Rect(output_rect.size()),
                                   request->scale_from(), request->scale_to())
                             : gfx::Rect(output_rect.size());

    // Result bounds may not satisfy the pixel format requirements for the
    // CopyOutputRequest - we need to adjust them to something that will be
    // compatible. Formats other than RGBA have this restriction.
    geometry.result_selection =
        request->result_format() == CopyOutputRequest::ResultFormat::RGBA
            ? geometry.result_bounds
            : media::MinimallyShrinkRectForI420(geometry.result_bounds);
    if (request->has_result_selection())
      geometry.result_selection.Intersect(request->result_selection());
    if (geometry.result_selection.IsEmpty())
      continue;

    geometry.sampling_bounds = MoveFromDrawToWindowSpace(output_rect);

    geometry.readback_offset =
        MoveFromDrawToWindowSpace(geometry.result_selection +
                                  output_rect.OffsetFromOrigin())
            .OffsetFromOrigin();

    CopyDrawnRenderPass(geometry, std::move(request));
  }
}

void DirectRenderer::AddInkDamageToRenderPass(
    const AggregatedRenderPass* render_pass,
    gfx::Rect& output_damage_rect) {
  if (auto* ink_renderer =
          GetDelegatedInkPointRenderer(/*create_if_necessary=*/false)) {
    auto pass_id = ink_renderer->GetLatestMetadataRenderPassId();
    // Apply damage rect to target render pass.
    // If the targeted render pass changes or there's no target, it is still
    // important to apply the new damage rect to the old render pass with
    // delegated ink, so that the region with ink can be invalidated and the ink
    // be cleared.
    if (render_pass->id == pass_id ||
        render_pass->id == last_pass_with_delegated_ink_) {
      // Ink damage rect is in root target space, and will need to be
      // transformed to the current render pass space.
      gfx::Transform root_target_to_render_pass_draw_transform;
      if (render_pass->transform_to_root_target.GetInverse(
              &root_target_to_render_pass_draw_transform)) {
        // Since we're potentially expanding damage, we need
        // |use_render_pass_drawn_rect_| to ensure that dependant render
        // passes always have valid pixels.
        DCHECK((render_pass == current_frame()->root_render_pass) ||
               use_render_pass_drawn_rect_);

        const gfx::Rect delegated_ink_damage_rect =
            ink_renderer->GetDamageRect();
        // Damage rect is initially in root space. Transform to render pass
        // space, even for a root render pass.
        gfx::Rect delegated_ink_damage_rect_in_draw_space =
            root_target_to_render_pass_draw_transform.MapRect(
                delegated_ink_damage_rect);
        // Make sure the damage rect is not larger than the render pass output
        // rect.
        delegated_ink_damage_rect_in_draw_space.Intersect(
            gfx::Rect(render_pass->output_rect));
        output_damage_rect.Union(delegated_ink_damage_rect_in_draw_space);
      }
    }
    last_pass_with_delegated_ink_ = pass_id;
  }
}

void DirectRenderer::DrawRenderPass(const AggregatedRenderPass* render_pass) {
  TRACE_EVENT1("viz", "DirectRenderer::DrawRenderPass", "NumberOfQuads",
               render_pass->quad_list.size());

  bool can_skip_rp = CanSkipRenderPass(render_pass);
  if (render_pass != current_frame()->root_render_pass) {
    UMA_HISTOGRAM_BOOLEAN("Compositing.DirectRenderer.SkipNonRootRenderPass",
                          can_skip_rp);
  }

  if (can_skip_rp) {
    skipped_render_pass_ids_.insert(render_pass->id);
    return;
  }

  const gfx::Rect surface_rect_in_draw_space = OutputSurfaceRectInDrawSpace();
  gfx::Rect render_pass_scissor_in_draw_space = surface_rect_in_draw_space;

  const bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;

  if (use_partial_swap_) {
    render_pass_scissor_in_draw_space.Intersect(
        ComputeScissorRectForRenderPass(current_frame()->current_render_pass));
  }

  AddInkDamageToRenderPass(current_frame()->current_render_pass,
                           render_pass_scissor_in_draw_space);

  if (is_root_render_pass && output_surface_clip_rect_) {
    render_pass_scissor_in_draw_space.Intersect(*output_surface_clip_rect_);
  }

  if (!is_root_render_pass && render_pass_scissor_in_draw_space.IsEmpty() &&
      base::FeatureList::IsEnabled(kAllowSkipEmptyNonrootRenderPassDraws)) {
    // If the scissor rect is empty, we will end up skipping all the draw quads,
    // so there is no work to do.
    return;
  }

  EnsureRenderPassAllocated(render_pass);

  // TODO(crbug.com/40454563): This change applies only when Vulkan is enabled
  // and it will be removed once SkiaRenderer has complete support for Vulkan.
  if (!is_root_render_pass && !IsRenderPassResourceAllocated(render_pass->id)) {
    return;
  }

  total_pixels_rendered_this_frame_ +=
      render_pass_scissor_in_draw_space.size().Area64();

  const bool render_pass_is_clipped =
      !render_pass_scissor_in_draw_space.Contains(surface_rect_in_draw_space);
  const bool should_clear_surface =
      !is_root_render_pass || settings_->should_clear_root_render_pass;
  const gfx::Rect render_pass_update_rect = MoveFromDrawToWindowSpace(
      render_pass_is_clipped ? render_pass_scissor_in_draw_space
                             : surface_rect_in_draw_space);

  const gfx::Size viewport_size = is_root_render_pass
                                      ? current_frame()->device_viewport_size
                                      : render_pass->output_rect.size();
  current_frame()->target_to_device_transform =
      CalculateTargetToDeviceTransform(
          /*draw_rect=*/render_pass->output_rect, viewport_size);
  BeginDrawingRenderPass(render_pass, should_clear_surface,
                         render_pass_update_rect, viewport_size);

  if (is_root_render_pass)
    last_root_render_pass_scissor_rect_ = render_pass_scissor_in_draw_space;

  const QuadList& quad_list = render_pass->quad_list;
  base::circular_deque<std::unique_ptr<DrawPolygon>> poly_list;

  int next_polygon_id = 0;
  int last_sorting_context_id = 0;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad& quad = **it;

    if (ShouldSkipQuad(quad, render_pass_is_clipped
                                 ? render_pass_scissor_in_draw_space
                                 : surface_rect_in_draw_space)) {
      continue;
    }

    if (last_sorting_context_id != quad.shared_quad_state->sorting_context_id) {
      last_sorting_context_id = quad.shared_quad_state->sorting_context_id;
      FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                    render_pass_is_clipped);
    }

    // This layer is in a 3D sorting context so we add it to the list of
    // polygons to go into the BSP tree.
    if (quad.shared_quad_state->sorting_context_id != 0) {
      // TODO(danakj): It's sad to do a malloc here to compare. Maybe construct
      // this on the stack and move it into the list.
      auto new_polygon = std::make_unique<DrawPolygon>(
          *it, gfx::RectF(quad.visible_rect),
          quad.shared_quad_state->quad_to_target_transform, next_polygon_id++);
      if (new_polygon->normal().LengthSquared() > 0.0) {
        poly_list.push_back(std::move(new_polygon));
      }
      continue;
    }

    // We are not in a 3d sorting context, so we should draw the quad normally.
    SetScissorStateForQuad(quad, render_pass_scissor_in_draw_space,
                           render_pass_is_clipped);

    DoDrawQuad(&quad, nullptr);
  }
  FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                render_pass_is_clipped);
  FinishDrawingRenderPass();

  if (use_render_pass_drawn_rect_ && !is_root_render_pass) {
    const gfx::Rect drawn_rect = GetRenderPassBackingDrawnRect(render_pass->id);
    constexpr char kDrawnRectAssignmentType[] =
        "Compositing.DirectRenderer.DrawnRectAssignmentType";
    if (drawn_rect != render_pass->output_rect) {
      CHECK_EQ(render_pass->output_rect, render_pass_scissor_in_draw_space);
      CHECK_EQ(surface_rect_in_draw_space, render_pass_scissor_in_draw_space);
      CHECK(!render_pass_is_clipped);
      if (render_pass->output_rect == render_pass->damage_rect) {
        UMA_HISTOGRAM_ENUMERATION(kDrawnRectAssignmentType,
                                  RenderPassDrawRectAssign::kFullDamage);
      } else {
        UMA_HISTOGRAM_ENUMERATION(kDrawnRectAssignmentType,
                                  drawn_rect.IsEmpty()
                                      ? RenderPassDrawRectAssign::kNewOutputRect
                                      : RenderPassDrawRectAssign::kReassign);
      }

      SetRenderPassBackingDrawnRect(render_pass->id, render_pass->output_rect);
    } else {
      UMA_HISTOGRAM_ENUMERATION(kDrawnRectAssignmentType,
                                RenderPassDrawRectAssign::kNoAssign);
    }
  }
}

bool DirectRenderer::CanSkipRenderPass(
    const AggregatedRenderPass* render_pass) const {
  if (render_pass == current_frame()->root_render_pass)
    return false;

  // If the RenderPass wants to be cached, then we only draw it if we need to.
  // When damage is present, then we can't skip the RenderPass. Or if the
  // texture does not exist (first frame, or was deleted) then we can't skip
  // the RenderPass.
  if (render_pass->cache_render_pass ||
      allow_undamaged_nonroot_render_pass_to_skip_) {
    // TODO(crbug.com/40232521): Fix CopyOutputRequest and allow the render pass
    // with copy request to skip.
    if (render_pass->has_damage_from_contributing_content ||
        !render_pass->copy_requests.empty()) {
      return false;
    }
    return IsRenderPassResourceAllocated(render_pass->id);
  }

  return false;
}

DirectRenderer::RenderPassRequirements
DirectRenderer::CalculateRenderPassRequirements(
    const AggregatedRenderPass* render_pass) const {
  const bool is_root = render_pass == current_frame()->root_render_pass;

  RenderPassRequirements requirements;

#if BUILDFLAG(IS_WIN)
  // All root render pass backings allocated by the renderer needs to eventually
  // go into some composition tree. Other things that own/allocate the root pass
  // backing include the output device and buffer queue.
  // Windows also can support scanout backings for non-root passes to optimize
  // partially delegated compositing iff they will not be read in Viz.
  requirements.is_scanout =
      is_root || (features::IsDelegatedCompositingEnabled() &&
                  render_pass->is_from_surface_root_pass &&
                  !render_pass->will_backing_be_read_by_viz);

  requirements.scanout_dcomp_surface =
      requirements.is_scanout && render_pass->needs_synchronous_dcomp_commit;
#else
  // On macOS and Lacros, the root render pass is handled by |BufferQueue| and
  // RPDQ overlays are handled by |PrepareRenderPassOverlay|.
  requirements.is_scanout = is_root;
#endif

  if (is_root) {
    requirements.size = surface_size_for_swap_buffers();
  } else {
    requirements.size = CalculateTextureSizeForRenderPass(render_pass);
  }

  if (requirements.is_scanout) {
    CHECK(!render_pass->generate_mipmap);
    requirements.generate_mipmap = false;
    requirements.color_space = reshape_color_space();
    requirements.format = reshape_si_format();
    if (is_root) {
      requirements.alpha_type = reshape_alpha_type();
    } else {
      requirements.alpha_type = render_pass->has_transparent_background
                                    ? RenderPassAlphaType::kPremul
                                    : RenderPassAlphaType::kOpaque;
    }
  } else {
    requirements.generate_mipmap = render_pass->generate_mipmap;
    requirements.color_space = RenderPassColorSpace(render_pass);
    requirements.format =
        GetColorSpaceSharedImageFormat(requirements.color_space);
    requirements.alpha_type = RenderPassAlphaType::kPremul;
  }

  if (render_pass->has_transparent_background) {
    CHECK(requirements.format.HasAlpha());
    CHECK_EQ(requirements.alpha_type, RenderPassAlphaType::kPremul);
  }

  return requirements;
}

gfx::ColorSpace DirectRenderer::RenderPassColorSpace(
    const AggregatedRenderPass* render_pass) const {
  const auto& display_color_spaces = current_frame()->display_color_spaces;
  auto content_color_usage = render_pass->content_color_usage;
  bool has_transparent_background = render_pass->has_transparent_background;
  return render_pass == current_frame()->root_render_pass
             ? ColorSpaceUtils::OutputColorSpace(display_color_spaces,
                                                 content_color_usage,
                                                 has_transparent_background)
             : ColorSpaceUtils::CompositingColorSpace(
                   display_color_spaces, content_color_usage,
                   has_transparent_background);
}

void DirectRenderer::EnsureRenderPassAllocated(
    const AggregatedRenderPass* render_pass) {
  const bool is_root = render_pass == current_frame()->root_render_pass;
  if (is_root && !output_surface_->capabilities().renderer_allocates_images) {
    return;
  }

  DirectRenderer::RenderPassRequirements requirements =
      CalculateRenderPassRequirements(render_pass);
  // We should not change the buffer size for the root render pass.
  if (!is_root) {
    requirements.size.Enlarge(enlarge_pass_texture_amount_.width(),
                              enlarge_pass_texture_amount_.height());
  }
  AllocateRenderPassResourceIfNeeded(render_pass->id, requirements);
}

gfx::Rect DirectRenderer::ComputeScissorRectForRenderPass(
    const AggregatedRenderPass* render_pass) const {
  const AggregatedRenderPass* root_render_pass =
      current_frame()->root_render_pass;
  gfx::Rect root_damage_rect = current_frame()->root_damage_rect;
  // If |frame_buffer_damage|, which is carried over from the previous frame
  // when we want to preserve buffer content, is not empty, we should add it
  // to both root and non-root render passes.
  gfx::Rect frame_buffer_damage = GetCurrentFramebufferDamage();

  if (render_pass == root_render_pass) {
    base::CheckedNumeric<int64_t> display_area =
        current_frame()->device_viewport_size.GetCheckedArea();
    base::CheckedNumeric<int64_t> root_damage_area =
        root_damage_rect.size().GetCheckedArea();
    if (display_area.IsValid() && root_damage_area.IsValid()) {
      DCHECK_GT(static_cast<int64_t>(display_area.ValueOrDie()), 0);
      {
        base::CheckedNumeric<int64_t> frame_buffer_damage_area =
            frame_buffer_damage.size().GetCheckedArea();
        int64_t percentage = ((frame_buffer_damage_area * 100ll) / display_area)
                                 .ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.FrameBufferDamage",
            percentage);
      }
      {
        int64_t percentage =
            ((root_damage_area * 100ll) / display_area).ValueOrDie();
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.RootDamage", percentage);
      }

      root_damage_rect.Union(frame_buffer_damage);

      // If the root damage rect intersects any child render pass that has a
      // pixel-moving backdrop filter, expand the damage to include the entire
      // child pass. See crbug.com/986206 for context.
      if ((!backdrop_filter_output_rects_.empty() ||
           has_pixel_moving_foreground_filters_) &&
          !root_damage_rect.IsEmpty()) {
        for (auto* quad : root_render_pass->quad_list) {
          // Sanity check: we should not have a Compositor
          // CompositorRenderPassDrawQuad here.
          DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
          if (auto* rpdq = quad->DynamicCast<AggregatedRenderPassDrawQuad>()) {
            // For render pass with pixel moving backdrop filters.
            if (auto iter =
                    backdrop_filter_output_rects_.find(rpdq->render_pass_id);
                iter != backdrop_filter_output_rects_.end()) {
              gfx::Rect this_output_rect = iter->second;
              if (root_damage_rect.Intersects(this_output_rect))
                root_damage_rect.Union(this_output_rect);
            }

            // For render pass with pixel moving foreground filters.
            const cc::FilterOperations* foreground_filters =
                FiltersForPass(rpdq->render_pass_id);
            if (foreground_filters &&
                foreground_filters->HasFilterThatMovesPixels()) {
              gfx::Rect expanded_rect =
                  GetExpandedRectWithPixelMovingForegroundFilter(
                      *rpdq, *foreground_filters);

              // Expanding damage outside of the 'clip_rect' can cause parts of
              // the root to be rendered that may never have been included due
              // to 'aggregate_only_damaged_' in SurfaceAggregator. See
              // crbug.com/1492891
              if (rpdq->shared_quad_state->clip_rect) {
                expanded_rect.Intersect(*rpdq->shared_quad_state->clip_rect);
              }

              if (root_damage_rect.Intersects(expanded_rect))
                root_damage_rect.Union(expanded_rect);
            }
          }
        }
      }
      // Total damage after all adjustments.
      base::CheckedNumeric<int64_t> total_damage_area =
          root_damage_rect.size().GetCheckedArea();
      {
        int64_t percentage = ((total_damage_area * 100ll) / display_area)
                                 .ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.TotalDamage", percentage);
      }
      {
        int64_t percentage =
            (((total_damage_area - root_damage_area) * 100ll) / display_area)
                .ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.ExtraDamage", percentage);
      }
    }

    return root_damage_rect;
  }

  DCHECK(render_pass->copy_requests.empty() ||
         (render_pass->damage_rect == render_pass->output_rect));

  if (use_render_pass_drawn_rect_) {
    if (GetRenderPassBackingDrawnRect(render_pass->id) ==
        render_pass->output_rect) {
      UMA_HISTOGRAM_BOOLEAN(
          "Compositing.DirectRenderer.RenderPassDrawnRectMatch", true);
      return render_pass->damage_rect;
    } else {
      // This is the first time we are drawing to this backing but it might not
      // be the first time we are drawing this render pass. If the render pass
      // backing has been deallocated we must conservatively redraw the entire
      // 'output_rect' as we have lost the accumulated damaged for this pass.
      // TODO(crbug.com/332562242): We should move to better tracking of
      // the drawn area by only fully drawing the visible portion of this render
      // pass and not the entire output rect. This information is available in
      // surface aggregator as root parent clip for render passes.
      UMA_HISTOGRAM_BOOLEAN(
          "Compositing.DirectRenderer.RenderPassDrawnRectMatch", false);
      return render_pass->output_rect;
    }
  }
  // If the root damage rect has been expanded due to overlays, all the other
  // damage rect calculations are incorrect. If the root damage rect was shrunk
  // to an empty rect (i.e. during overlay processing for delegated compositing)
  // then |Contains()| no longer works as expected so it must be checked
  // separately.
  if (!root_damage_rect.IsEmpty() &&
      !root_render_pass->damage_rect.Contains(root_damage_rect)) {
    return render_pass->output_rect;
  }

  // For the non-root render pass.
  // This is a repeated computation of target damage to render pass damage that
  // already occurs in surface aggregator.
  gfx::Rect damage_rect = render_pass->damage_rect;
  if (!frame_buffer_damage.IsEmpty()) {
    gfx::Transform inverse_transform;
    if (render_pass->transform_to_root_target.GetInverse(&inverse_transform)) {
      // |frame_buffer_damage| is in the root target space. Transform the damage
      // from the root to the non-root space before it's added.
      gfx::Rect frame_buffer_damage_in_render_pass_space =
          cc::MathUtil::MapEnclosingClippedRect(inverse_transform,
                                                frame_buffer_damage);
      damage_rect.Union(frame_buffer_damage_in_render_pass_space);
    }
  }

  return damage_rect;
}

gfx::Size DirectRenderer::CalculateTextureSizeForRenderPass(
    const AggregatedRenderPass* render_pass) const {
  // Round the size of the render pass backings to a multiple of 64 pixels. This
  // reduces memory fragmentation. https://crbug.com/146070. This also allows
  // backings to be more easily reused during a resize operation.
  int width = render_pass->output_rect.width();
  int height = render_pass->output_rect.height();
  if (!settings_->dont_round_texture_sizes_for_pixel_tests) {
    constexpr int multiple = 64;
    width = cc::MathUtil::CheckedRoundUp(width, multiple);
    height = cc::MathUtil::CheckedRoundUp(height, multiple);

    // There are no guarantees that max texture size is a multiple of 64 so
    // clamp the rounded up dimensions to avoid ending up with dimensions
    // larger than max texture size. Note: Software surfaces and some
    // test only surfaces does not have max_texture_size set so assume that
    // we don't have to clamp the dimensions when max texture size is zero.
    const int max_texture_size =
        output_surface_->capabilities().max_texture_size;
    if (max_texture_size > 0) {
      width = std::min(max_texture_size, width);
      height = std::min(max_texture_size, height);
    }
  }

  return gfx::Size(width, height);
}

// TODO(fangzhoug): There should be metrics recording the amount of unused
// buffer area and number of reallocations to quantify the trade-off.
gfx::Size DirectRenderer::CalculateSizeForOutputSurface(
    const gfx::Size& requested_viewport_size) {
  // We're not able to clip back buffers if output surface does not support
  // clipping.
  if (requested_viewport_size == surface_size_for_swap_buffers() ||
      !output_surface_->capabilities().supports_viewporter ||
      settings_->dont_round_texture_sizes_for_pixel_tests) {
    device_viewport_size_ = requested_viewport_size;
    return requested_viewport_size;
  }

  // If 1 second has passed since last |device_viewport_size_| change, shrink
  // OutputSurface size to |device_viewport_size_|.
  if (device_viewport_size_ == requested_viewport_size &&
      (base::TimeTicks::Now() - last_viewport_resize_time_) >=
          base::Seconds(1)) {
    return requested_viewport_size;
  }

  // Round the size of the output surface to a multiple of 256 pixels. This
  // allows backings to be more easily reused during a resize operation.
  const int request_width = requested_viewport_size.width();
  const int request_height = requested_viewport_size.height();
  int surface_width = surface_size_for_swap_buffers().width();
  int surface_height = surface_size_for_swap_buffers().height();
  constexpr int multiple = 256;

  // If |request_width| or |request_height| is already a multiple of |multiple|,
  // round up extra |multiple| pixels s.t. we always have some amount of
  // padding.
  if (request_width > surface_width)
    surface_width =
        cc::MathUtil::CheckedRoundUp(request_width + multiple - 1, multiple);
  if (request_height > surface_height)
    surface_height =
        cc::MathUtil::CheckedRoundUp(request_height + multiple - 1, multiple);

  if (requested_viewport_size != device_viewport_size_)
    last_viewport_resize_time_ = base::TimeTicks::Now();

  // Width & height mustn't be more than max texture size.
  if (surface_width > output_surface_->capabilities().max_texture_size) {
    auto old_width = surface_width;
    surface_width = output_surface_->capabilities().max_texture_size;
    LOG_IF(ERROR, surface_width < request_width)
        << "Reduced surface width from " << old_width << " to "
        << surface_width;
  }
  if (surface_height > output_surface_->capabilities().max_texture_size) {
    auto old_height = surface_height;
    surface_height = output_surface_->capabilities().max_texture_size;
    LOG_IF(ERROR, surface_height < request_height)
        << "Reduced surface height from " << old_height << " to "
        << surface_height;
  }

  device_viewport_size_ = requested_viewport_size;
  return gfx::Size(surface_width, surface_height);
}

void DirectRenderer::SetCurrentFrameForTesting(const DrawingFrame& frame) {
  current_frame_valid_ = true;
  current_frame_ = frame;
}

bool DirectRenderer::HasAllocatedResourcesForTesting(
    const AggregatedRenderPassId& render_pass_id) const {
  return IsRenderPassResourceAllocated(render_pass_id);
}

bool DirectRenderer::ShouldApplyRoundedCorner(const DrawQuad* quad) const {
  const gfx::RectF target_quad = cc::MathUtil::MapClippedRect(
      quad->shared_quad_state->quad_to_target_transform,
      gfx::RectF(quad->visible_rect));

  return QuadRoundedCornersBoundsIntersects(quad, target_quad);
}

float DirectRenderer::CurrentFrameSDRWhiteLevel() const {
  return current_frame()->display_color_spaces.GetSDRMaxLuminanceNits();
}

bool DirectRenderer::ShouldApplyGradientMask(const DrawQuad* quad) const {
  if (!quad->shared_quad_state->mask_filter_info.HasGradientMask())
    return false;

  return true;
}

SharedImageFormat DirectRenderer::GetColorSpaceSharedImageFormat(
    gfx::ColorSpace color_space) const {
  gpu::Capabilities caps;
  caps.texture_format_bgra8888 = SupportsBGRA();
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  auto gfx_hdr_format =
      current_frame()->display_color_spaces.GetOutputBufferFormat(
          gfx::ContentColorUsage::kHDR, /*needs_alpha=*/true);
  auto viz_hdr_format = gfx_hdr_format == gfx::BufferFormat::RGBA_F16
                            ? SinglePlaneFormat::kRGBA_F16
                            : SinglePlaneFormat::kRGBA_1010102;
  auto format = color_space.IsHDR()
                    ? viz_hdr_format
                    : PlatformColor::BestSupportedTextureFormat(caps);
#else
  auto format = color_space.IsHDR()
                    ? SinglePlaneFormat::kRGBA_F16
                    : PlatformColor::BestSupportedTextureFormat(caps);
#endif
  return format;
}

DelegatedInkPointRendererBase* DirectRenderer::GetDelegatedInkPointRenderer(
    bool create_if_necessary) {
  return nullptr;
}

bool DirectRenderer::CompositeTimeTracingEnabled() {
  return false;
}

void DirectRenderer::AddCompositeTimeTraces(base::TimeTicks ready_timestamp) {}

gfx::Rect DirectRenderer::GetDelegatedInkTrailDamageRect() {
  if (auto* ink_renderer =
          GetDelegatedInkPointRenderer(/*create_if_necessary=*/false)) {
    return ink_renderer->GetDamageRect();
  }

  return gfx::Rect();
}

gpu::Mailbox DirectRenderer::GetPrimaryPlaneOverlayTestingMailbox() {
  NOTREACHED_IN_MIGRATION();
  return gpu::Mailbox();
}

}  // namespace viz
