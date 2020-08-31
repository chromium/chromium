// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/direct_renderer.h"

#include <limits.h>
#include <stddef.h>

#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/containers/circular_deque.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/service/display/bsp_tree.h"
#include "components/viz/service/display/bsp_walk_action.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace {

static gfx::Transform OrthoProjectionMatrix(float left,
                                            float right,
                                            float bottom,
                                            float top) {
  // Use the standard formula to map the clipping frustum to the cube from
  // [-1, -1, -1] to [1, 1, 1].
  float delta_x = right - left;
  float delta_y = top - bottom;
  gfx::Transform proj;
  if (!delta_x || !delta_y)
    return proj;
  proj.matrix().set(0, 0, 2.0f / delta_x);
  proj.matrix().set(0, 3, -(right + left) / delta_x);
  proj.matrix().set(1, 1, 2.0f / delta_y);
  proj.matrix().set(1, 3, -(top + bottom) / delta_y);

  // Z component of vertices is always set to zero as we don't use the depth
  // buffer while drawing.
  proj.matrix().set(2, 2, 0);

  return proj;
}

static gfx::Transform window_matrix(int x, int y, int width, int height) {
  gfx::Transform canvas;

  // Map to window position and scale up to pixel coordinates.
  canvas.Translate3d(x, y, 0);
  canvas.Scale3d(width, height, 0);

  // Map from ([-1, -1] to [1, 1]) -> ([0, 0] to [1, 1])
  canvas.Translate3d(0.5, 0.5, 0.5);
  canvas.Scale3d(0.5, 0.5, 0.5);

  return canvas;
}

// Returns the bounding box that contains the specified rounded corner.
gfx::RectF ComputeRoundedCornerBoundingBox(const gfx::RRectF& rrect,
                                           const gfx::RRectF::Corner corner) {
  auto radii = rrect.GetCornerRadii(corner);
  gfx::RectF bounding_box(radii.x(), radii.y());
  switch (corner) {
    case gfx::RRectF::Corner::kUpperLeft:
      bounding_box.Offset(rrect.rect().x(), rrect.rect().y());
      break;
    case gfx::RRectF::Corner::kUpperRight:
      bounding_box.Offset(rrect.rect().right() - radii.x(), rrect.rect().y());
      break;
    case gfx::RRectF::Corner::kLowerRight:
      bounding_box.Offset(rrect.rect().right() - radii.x(),
                          rrect.rect().bottom() - radii.y());
      break;
    case gfx::RRectF::Corner::kLowerLeft:
      bounding_box.Offset(rrect.rect().x(), rrect.rect().bottom() - radii.y());
      break;
  }
  return bounding_box;
}

}  // namespace

namespace viz {

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
      overlay_processor_(overlay_processor) {
  DCHECK(output_surface_);
}

DirectRenderer::~DirectRenderer() = default;

void DirectRenderer::Initialize() {
  auto* context_provider = output_surface_->context_provider();

  use_partial_swap_ = settings_->partial_swap_enabled && CanPartialSwap();
  allow_empty_swap_ = use_partial_swap_;
  if (context_provider) {
    if (context_provider->ContextCapabilities().commit_overlay_planes)
      allow_empty_swap_ = true;
#if DCHECK_IS_ON()
    supports_occlusion_query_ =
        context_provider->ContextCapabilities().occlusion_query;
#endif
  } else {
    allow_empty_swap_ |=
        output_surface_->capabilities().supports_commit_overlay_planes;
  }

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

void DirectRenderer::InitializeViewport(DrawingFrame* frame,
                                        const gfx::Rect& draw_rect,
                                        const gfx::Rect& viewport_rect,
                                        const gfx::Size& surface_size) {
  DCHECK_GE(viewport_rect.x(), 0);
  DCHECK_GE(viewport_rect.y(), 0);
  DCHECK_LE(viewport_rect.right(), surface_size.width());
  DCHECK_LE(viewport_rect.bottom(), surface_size.height());
  bool flip_y = FlippedFramebuffer();
  if (flip_y) {
    frame->projection_matrix = OrthoProjectionMatrix(
        draw_rect.x(), draw_rect.right(), draw_rect.bottom(), draw_rect.y());
  } else {
    frame->projection_matrix = OrthoProjectionMatrix(
        draw_rect.x(), draw_rect.right(), draw_rect.y(), draw_rect.bottom());
  }

  gfx::Rect window_rect = viewport_rect;
  if (flip_y)
    window_rect.set_y(surface_size.height() - viewport_rect.bottom());
  frame->window_matrix =
      window_matrix(window_rect.x(), window_rect.y(), window_rect.width(),
                    window_rect.height());
  current_draw_rect_ = draw_rect;
  current_viewport_rect_ = viewport_rect;
  current_surface_size_ = surface_size;
  current_window_space_viewport_ = window_rect;
}

gfx::Rect DirectRenderer::MoveFromDrawToWindowSpace(
    const gfx::Rect& draw_rect) const {
  gfx::Rect window_rect = draw_rect;
  window_rect -= current_draw_rect_.OffsetFromOrigin();
  window_rect += current_viewport_rect_.OffsetFromOrigin();
  if (FlippedFramebuffer())
    window_rect.set_y(current_surface_size_.height() - window_rect.bottom());
  return window_rect;
}

const DrawQuad* DirectRenderer::CanPassBeDrawnDirectly(
    const AggregatedRenderPass* pass) {
  return nullptr;
}

void DirectRenderer::SetVisible(bool visible) {
  DCHECK(initialized_);
  if (visible_ == visible)
    return;
  visible_ = visible;
  DidChangeVisibility();
}

void DirectRenderer::DecideRenderPassAllocationsForFrame(
    const AggregatedRenderPassList& render_passes_in_draw_order) {
  DCHECK(render_pass_bypass_quads_.empty());

  auto& root_render_pass = render_passes_in_draw_order.back();

  base::flat_map<AggregatedRenderPassId, RenderPassRequirements>
      render_passes_in_frame;
  for (const auto& pass : render_passes_in_draw_order) {
    // If there's a copy request, we need an explicit renderpass backing so
    // only try to draw directly if there are no copy requests.
    if (pass != root_render_pass && pass->copy_requests.empty()) {
      if (const DrawQuad* quad = CanPassBeDrawnDirectly(pass.get())) {
        // If the render pass is drawn directly, it will not be drawn from as
        // a render pass so it's not added to the map.
        render_pass_bypass_quads_[pass->id] = quad;
        continue;
      }
    }
    render_passes_in_frame[pass->id] = {
        CalculateTextureSizeForRenderPass(pass.get()), pass->generate_mipmap};
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
    const gfx::DisplayColorSpaces& display_color_spaces) {
  DCHECK(visible_);
  TRACE_EVENT0("viz,benchmark", "DirectRenderer::DrawFrame");
  UMA_HISTOGRAM_COUNTS_1M(
      "Renderer4.renderPassCount",
      base::saturated_cast<int>(render_passes_in_draw_order->size()));

  auto* root_render_pass = render_passes_in_draw_order->back().get();
  DCHECK(root_render_pass);

#if DCHECK_IS_ON()
  bool overdraw_tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.overdraw"),
                                     &overdraw_tracing_enabled);
  DLOG_IF(WARNING, !overdraw_tracing_support_missing_logged_once_ &&
                       overdraw_tracing_enabled && !supports_occlusion_query_)
      << "Overdraw tracing enabled on platform without support.";
  overdraw_tracing_support_missing_logged_once_ = true;
#endif

  bool overdraw_feedback = debug_settings_->show_overdraw_feedback;
  if (overdraw_feedback && !output_surface_->capabilities().supports_stencil) {
#if DCHECK_IS_ON()
    DLOG_IF(WARNING, !overdraw_feedback_support_missing_logged_once_)
        << "Overdraw feedback enabled on platform without support.";
    overdraw_feedback_support_missing_logged_once_ = true;
#endif
    overdraw_feedback = false;
  }
  base::AutoReset<bool> auto_reset_overdraw_feedback(&overdraw_feedback_,
                                                     overdraw_feedback);

  current_frame_valid_ = true;
  current_frame_ = DrawingFrame();
  current_frame()->render_passes_in_draw_order = render_passes_in_draw_order;
  current_frame()->root_render_pass = root_render_pass;
  current_frame()->root_damage_rect = root_render_pass->damage_rect;
  if (overlay_processor_) {
    current_frame()->root_damage_rect.Union(
        overlay_processor_->GetAndResetOverlayDamage());
  }
  current_frame()->root_damage_rect.Intersect(gfx::Rect(device_viewport_size));
  current_frame()->device_viewport_size = device_viewport_size;
  current_frame()->display_color_spaces = display_color_spaces;

  output_surface_->SetNeedsMeasureNextDrawLatency();
  BeginDrawingFrame();

  // RenderPass owns filters, backdrop_filters, etc., and will outlive this
  // function call. So it is safe to store pointers in these maps.
  for (const auto& pass : *render_passes_in_draw_order) {
    if (!pass->filters.IsEmpty())
      render_pass_filters_[pass->id] = &pass->filters;
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
  gfx::ColorSpace frame_color_space = RootRenderPassColorSpace();
  gfx::BufferFormat frame_buffer_format =
      current_frame()->display_color_spaces.GetOutputBufferFormat(
          current_frame()->root_render_pass->content_color_usage,
          frame_has_alpha);
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
    if (output_surface_->IsDisplayedAsOverlayPlane()) {
      // OutputSurface::GetOverlayMailbox() returns the mailbox for the last
      // used buffer, which is most likely different from the one being used
      // this frame. However, for the purpose of testing the overlay
      // configuration, the mailbox for ANY buffer from BufferQueue is good
      // enough because they're all created with identical properties.
      current_frame()->output_surface_plane =
          overlay_processor_->ProcessOutputSurfaceAsOverlay(
              device_viewport_size, frame_buffer_format, frame_color_space,
              frame_has_alpha, output_surface_->GetOverlayMailbox());
      primary_plane = &(current_frame()->output_surface_plane.value());
    }

    // Attempt to replace some or all of the quads of the root render pass with
    // overlays.
    overlay_processor_->ProcessForOverlays(
        resource_provider_, render_passes_in_draw_order,
        output_surface_->color_matrix(), render_pass_filters_,
        render_pass_backdrop_filters_, primary_plane,
        &current_frame()->overlay_list, &current_frame()->root_damage_rect,
        &current_frame()->root_content_bounds);

    // If we promote any quad to an underlay then the main plane must support
    // alpha.
    // TODO(ccameron): We should update
    // |root_render_pass->has_transparent_background|, |frame_color_space|, and
    // |frame_buffer_format| based on the change in |frame_has_alpha|.
    if (current_frame()->output_surface_plane)
      frame_has_alpha |= current_frame()->output_surface_plane->enable_blending;

    overlay_processor_->AdjustOutputSurfaceOverlay(
        &(current_frame()->output_surface_plane));
  }

  // Only reshape when we know we are going to draw. Otherwise, the reshape
  // can leave the window at the wrong size if we never draw and the proper
  // viewport size is never set.
  bool use_stencil = overdraw_feedback_;
  bool needs_full_frame_redraw = false;
  if (device_viewport_size != reshape_surface_size_ ||
      device_scale_factor != reshape_device_scale_factor_ ||
      frame_color_space != reshape_color_space_ ||
      frame_buffer_format != reshape_buffer_format_ ||
      use_stencil != reshape_use_stencil_) {
    reshape_surface_size_ = device_viewport_size;
    reshape_device_scale_factor_ = device_scale_factor;
    reshape_color_space_ = frame_color_space;
    reshape_buffer_format_ = frame_buffer_format;
    reshape_use_stencil_ = overdraw_feedback_;
    output_surface_->Reshape(reshape_surface_size_,
                             reshape_device_scale_factor_, reshape_color_space_,
                             *reshape_buffer_format_, reshape_use_stencil_);
#if defined(OS_APPLE)
    // For Mac, all render passes will be promoted to CALayer, the redraw full
    // frame is for the main surface only.
    // TODO(penghuang): verify this logic with SkiaRenderer.
    if (!output_surface_->capabilities().supports_surfaceless)
      needs_full_frame_redraw = true;
#else
    // The entire surface has to be redrawn if reshape is requested.
    needs_full_frame_redraw = true;
#endif
  }

  // Draw all non-root render passes except for the root render pass.
  for (const auto& pass : *render_passes_in_draw_order) {
    if (pass.get() == root_render_pass)
      break;
    DrawRenderPassAndExecuteCopyRequests(pass.get());
  }

  bool skip_drawing_root_render_pass =
      current_frame()->root_damage_rect.IsEmpty() && allow_empty_swap_ &&
      !needs_full_frame_redraw;

  // If partial swap is not used, and the frame can not be skipped, the whole
  // frame has to be redrawn.
  if (!use_partial_swap_ && !skip_drawing_root_render_pass)
    needs_full_frame_redraw = true;

  // If we need to redraw the frame, the whole output should be considered
  // damaged.
  if (needs_full_frame_redraw)
    current_frame()->root_damage_rect = gfx::Rect(device_viewport_size);

  if (!skip_drawing_root_render_pass)
    DrawRenderPassAndExecuteCopyRequests(root_render_pass);

  // Use a fence to synchronize display of the main fb used by the output
  // surface. Note that gpu_fence_id may have the special value 0 ("no fence")
  // if fences are not supported. In that case synchronization will happen
  // through other means on the service side.
  // TODO(afrantzis): Consider using per-overlay fences instead of the one
  // associated with the output surface when possible.
  if (current_frame()->output_surface_plane)
    current_frame()->output_surface_plane->gpu_fence_id =
        output_surface_->UpdateGpuFence();

  if (overlay_processor_)
    overlay_processor_->TakeOverlayCandidates(&current_frame()->overlay_list);

  FinishDrawingFrame();

  if (overlay_processor_)
    overlay_processor_->ScheduleOverlays(resource_provider_);

  render_passes_in_draw_order->clear();
  render_pass_filters_.clear();
  render_pass_backdrop_filters_.clear();
  render_pass_backdrop_filter_bounds_.clear();
  render_pass_bypass_quads_.clear();
  backdrop_filter_output_rects_.clear();

  current_frame_valid_ = false;
}

gfx::Rect DirectRenderer::GetTargetDamageBoundingRect() const {
  gfx::Rect bounding_rect = output_surface_->GetCurrentFramebufferDamage();
  if (overlay_processor_) {
    bounding_rect.Union(
        overlay_processor_->GetPreviousFrameOverlaysBoundingRect());
  }
  return bounding_rect;
}

gfx::Rect DirectRenderer::DeviceViewportRectInDrawSpace() const {
  gfx::Rect device_viewport_rect(current_frame()->device_viewport_size);
  device_viewport_rect -= current_viewport_rect_.OffsetFromOrigin();
  device_viewport_rect += current_draw_rect_.OffsetFromOrigin();
  return device_viewport_rect;
}

gfx::Rect DirectRenderer::OutputSurfaceRectInDrawSpace() const {
  if (current_frame()->current_render_pass ==
      current_frame()->root_render_pass) {
    gfx::Rect output_surface_rect(current_frame()->device_viewport_size);
    output_surface_rect -= current_viewport_rect_.OffsetFromOrigin();
    output_surface_rect += current_draw_rect_.OffsetFromOrigin();
    return output_surface_rect;
  } else {
    return current_frame()->current_render_pass->output_rect;
  }
}

bool DirectRenderer::ShouldSkipQuad(const DrawQuad& quad,
                                    const gfx::Rect& render_pass_scissor) {
  if (render_pass_scissor.IsEmpty())
    return true;

  gfx::Rect target_rect = cc::MathUtil::MapEnclosingClippedRect(
      quad.shared_quad_state->quad_to_target_transform, quad.visible_rect);
  if (quad.shared_quad_state->is_clipped)
    target_rect.Intersect(quad.shared_quad_state->clip_rect);

  target_rect.Intersect(render_pass_scissor);
  return target_rect.IsEmpty();
}

void DirectRenderer::SetScissorStateForQuad(
    const DrawQuad& quad,
    const gfx::Rect& render_pass_scissor,
    bool use_render_pass_scissor) {
  if (use_render_pass_scissor) {
    gfx::Rect quad_scissor_rect = render_pass_scissor;
    if (quad.shared_quad_state->is_clipped)
      quad_scissor_rect.Intersect(quad.shared_quad_state->clip_rect);
    SetScissorTestRectInDrawSpace(quad_scissor_rect);
    return;
  } else if (quad.shared_quad_state->is_clipped) {
    SetScissorTestRectInDrawSpace(quad.shared_quad_state->clip_rect);
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

const base::Optional<gfx::RRectF> DirectRenderer::BackdropFilterBoundsForPass(
    AggregatedRenderPassId render_pass_id) const {
  auto it = render_pass_backdrop_filter_bounds_.find(render_pass_id);
  return it == render_pass_backdrop_filter_bounds_.end()
             ? base::Optional<gfx::RRectF>()
             : it->second;
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

    geometry.result_selection = geometry.result_bounds;
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

void DirectRenderer::DrawRenderPass(const AggregatedRenderPass* render_pass) {
  TRACE_EVENT0("viz", "DirectRenderer::DrawRenderPass");
  if (CanSkipRenderPass(render_pass))
    return;
  UseRenderPass(render_pass);

  // TODO(crbug.com/582554): This change applies only when Vulkan is enabled and
  // it will be removed once SkiaRenderer has complete support for Vulkan.
  if (current_frame()->current_render_pass !=
          current_frame()->root_render_pass &&
      !IsRenderPassResourceAllocated(render_pass->id))
    return;

  const gfx::Rect surface_rect_in_draw_space = OutputSurfaceRectInDrawSpace();
  gfx::Rect render_pass_scissor_in_draw_space = surface_rect_in_draw_space;

  bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;
  if (is_root_render_pass) {
    render_pass_scissor_in_draw_space.Intersect(
        DeviceViewportRectInDrawSpace());
  }

  if (use_partial_swap_) {
    render_pass_scissor_in_draw_space.Intersect(
        ComputeScissorRectForRenderPass(current_frame()->current_render_pass));
  }

  const bool render_pass_is_clipped =
      !render_pass_scissor_in_draw_space.Contains(surface_rect_in_draw_space);

  // The SetDrawRectangleCHROMIUM spec requires that the scissor bit is always
  // set on the root framebuffer or else the rendering may modify something
  // outside the damage rectangle, even if the damage rectangle is the size of
  // the full backbuffer.
  const bool supports_dc_layers =
      output_surface_->capabilities().supports_dc_layers;
  const bool render_pass_requires_scissor =
      render_pass_is_clipped || (supports_dc_layers && is_root_render_pass);

  const bool has_external_stencil_test =
      is_root_render_pass && output_surface_->HasExternalStencilTest();
  const bool should_clear_surface =
      !has_external_stencil_test &&
      (!is_root_render_pass || settings_->should_clear_root_render_pass);

  // If |has_external_stencil_test| we can't discard or clear. Make sure we
  // don't need to.
  DCHECK(!has_external_stencil_test ||
         !current_frame()->current_render_pass->has_transparent_background);

  SurfaceInitializationMode mode;
  if (should_clear_surface && render_pass_requires_scissor) {
    mode = SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR;
  } else if (should_clear_surface) {
    mode = SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR;
  } else {
    mode = SURFACE_INITIALIZATION_MODE_PRESERVE;
  }

  PrepareSurfaceForPass(
      mode, MoveFromDrawToWindowSpace(render_pass_scissor_in_draw_space));

  if (is_root_render_pass)
    last_root_render_pass_scissor_rect_ = render_pass_scissor_in_draw_space;

  const QuadList& quad_list = render_pass->quad_list;
  base::circular_deque<std::unique_ptr<DrawPolygon>> poly_list;

  int next_polygon_id = 0;
  int last_sorting_context_id = 0;
  for (auto it = quad_list.BackToFrontBegin(); it != quad_list.BackToFrontEnd();
       ++it) {
    const DrawQuad& quad = **it;

    if (render_pass_is_clipped &&
        ShouldSkipQuad(quad, render_pass_scissor_in_draw_space)) {
      continue;
    }

    if (last_sorting_context_id != quad.shared_quad_state->sorting_context_id) {
      last_sorting_context_id = quad.shared_quad_state->sorting_context_id;
      FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                    render_pass_requires_scissor);
    }

    // This layer is in a 3D sorting context so we add it to the list of
    // polygons to go into the BSP tree.
    if (quad.shared_quad_state->sorting_context_id != 0) {
      // TODO(danakj): It's sad to do a malloc here to compare. Maybe construct
      // this on the stack and move it into the list.
      auto new_polygon = std::make_unique<DrawPolygon>(
          *it, gfx::RectF(quad.visible_rect),
          quad.shared_quad_state->quad_to_target_transform, next_polygon_id++);
      if (new_polygon->points().size() > 2u) {
        poly_list.push_back(std::move(new_polygon));
      }
      continue;
    }

    // We are not in a 3d sorting context, so we should draw the quad normally.
    SetScissorStateForQuad(quad, render_pass_scissor_in_draw_space,
                           render_pass_requires_scissor);

    DoDrawQuad(&quad, nullptr);
  }
  if (is_root_render_pass && delegated_ink_point_renderer_)
    delegated_ink_point_renderer_->DrawDelegatedInkTrail();

  FlushPolygons(&poly_list, render_pass_scissor_in_draw_space,
                render_pass_requires_scissor);
  FinishDrawingQuadList();

  if (is_root_render_pass && overdraw_feedback_)
    FlushOverdrawFeedback(render_pass_scissor_in_draw_space);

  if (render_pass->generate_mipmap)
    GenerateMipmap();
}

bool DirectRenderer::CanSkipRenderPass(
    const AggregatedRenderPass* render_pass) const {
  if (render_pass == current_frame()->root_render_pass)
    return false;

  // TODO(crbug.com/783275): It's possible to skip a child RenderPass if damage
  // does not overlap it, since that means nothing has changed:
  //   ComputeScissorRectForRenderPass(render_pass).IsEmpty()
  // However that caused crashes where the RenderPass' texture was not present
  // (never seen the RenderPass before, or the texture was deleted when not used
  // for a frame). It could avoid skipping if there is no texture present, which
  // is what was done for a while, but this seems to papering over a missing
  // damage problem, or we're failing to understand the system wholey.
  // If attempted again this should probably CHECK() that the texture exists,
  // and attempt to figure out where the new RenderPass texture without damage
  // is coming from.

  // If the RenderPass wants to be cached, then we only draw it if we need to.
  // When damage is present, then we can't skip the RenderPass. Or if the
  // texture does not exist (first frame, or was deleted) then we can't skip
  // the RenderPass.
  if (render_pass->cache_render_pass) {
    if (render_pass->has_damage_from_contributing_content)
      return false;
    return IsRenderPassResourceAllocated(render_pass->id);
  }

  return false;
}

void DirectRenderer::UseRenderPass(const AggregatedRenderPass* render_pass) {
  current_frame()->current_render_pass = render_pass;
  if (render_pass == current_frame()->root_render_pass) {
    BindFramebufferToOutputSurface();
    if (output_surface_->capabilities().supports_dc_layers)
      output_surface_->SetDrawRectangle(current_frame()->root_damage_rect);
    InitializeViewport(current_frame(), render_pass->output_rect,
                       gfx::Rect(current_frame()->device_viewport_size),
                       current_frame()->device_viewport_size);
    return;
  }

  gfx::Size enlarged_size = CalculateTextureSizeForRenderPass(render_pass);
  enlarged_size.Enlarge(enlarge_pass_texture_amount_.width(),
                        enlarge_pass_texture_amount_.height());

  AllocateRenderPassResourceIfNeeded(
      render_pass->id, {enlarged_size, render_pass->generate_mipmap});

  // TODO(crbug.com/582554): This change applies only when Vulkan is enabled and
  // it will be removed once SkiaRenderer has complete support for Vulkan.
  if (!IsRenderPassResourceAllocated(render_pass->id))
    return;

  BindFramebufferToTexture(render_pass->id);
  InitializeViewport(current_frame(), render_pass->output_rect,
                     gfx::Rect(render_pass->output_rect.size()),
                     // If the render pass backing is cached, we might have
                     // bigger size comparing to the size that was generated.
                     GetRenderPassBackingPixelSize(render_pass->id));
}

gfx::Rect DirectRenderer::ComputeScissorRectForRenderPass(
    const AggregatedRenderPass* render_pass) const {
  const AggregatedRenderPass* root_render_pass =
      current_frame()->root_render_pass;
  gfx::Rect root_damage_rect = current_frame()->root_damage_rect;

  if (render_pass == root_render_pass) {
    base::CheckedNumeric<int> display_area =
        current_frame()->device_viewport_size.GetCheckedArea();
    gfx::Rect frame_buffer_damage =
        output_surface_->GetCurrentFramebufferDamage();
    base::CheckedNumeric<int> root_damage_area =
        root_damage_rect.size().GetCheckedArea();
    if (display_area.IsValid() && root_damage_area.IsValid()) {
      DCHECK_GT(static_cast<int>(display_area.ValueOrDie()), 0);
      {
        base::CheckedNumeric<int> frame_buffer_damage_area =
            frame_buffer_damage.size().GetCheckedArea();
        int ratio =
            (frame_buffer_damage_area / display_area).ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.FrameBufferDamage",
            100ull * ratio);
      }
      {
        int ratio = (root_damage_area / display_area).ValueOrDie();
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.RootDamage",
            100ull * ratio);
      }

      root_damage_rect.Union(frame_buffer_damage);

      // If the root damage rect intersects any child render pass that has a
      // pixel-moving backdrop-filter, expand the damage to include the entire
      // child pass. See crbug.com/986206 for context.
      if (!backdrop_filter_output_rects_.empty() &&
          !root_damage_rect.IsEmpty()) {
        for (auto* quad : render_pass->quad_list) {
          // Sanity check: we should not have a Compositor
          // CompositorRenderPassDrawQuad here.
          DCHECK_NE(quad->material, DrawQuad::Material::kCompositorRenderPass);
          if (quad->material == DrawQuad::Material::kAggregatedRenderPass) {
            auto iter = backdrop_filter_output_rects_.find(
                AggregatedRenderPassDrawQuad::MaterialCast(quad)
                    ->render_pass_id);
            if (iter != backdrop_filter_output_rects_.end()) {
              gfx::Rect this_output_rect = iter->second;
              if (root_damage_rect.Intersects(this_output_rect))
                root_damage_rect.Union(this_output_rect);
            }
          }
        }
      }

      // Total damage after all adjustments.
      base::CheckedNumeric<int> total_damage_area =
          root_damage_rect.size().GetCheckedArea();
      {
        int ratio = (total_damage_area / display_area).ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.TotalDamage",
            100ull * ratio);
      }
      {
        int ratio = ((total_damage_area - root_damage_area) / display_area)
                        .ValueOrDefault(INT_MAX);
        UMA_HISTOGRAM_PERCENTAGE(
            "Compositing.DirectRenderer.PartialSwap.ExtraDamage",
            100ull * ratio);
      }
    }

    return root_damage_rect;
  }

  // If the root damage rect has been expanded due to overlays, all the other
  // damage rect calculations are incorrect.
  if (!root_render_pass->damage_rect.Contains(root_damage_rect))
    return render_pass->output_rect;

  DCHECK(render_pass->copy_requests.empty() ||
         (render_pass->damage_rect == render_pass->output_rect));
  return render_pass->damage_rect;
}

gfx::Size DirectRenderer::CalculateTextureSizeForRenderPass(
    const AggregatedRenderPass* render_pass) {
  // Round the size of the render pass backings to a multiple of 64 pixels. This
  // reduces memory fragmentation. https://crbug.com/146070. This also allows
  // backings to be more easily reused during a resize operation.
  int width = render_pass->output_rect.width();
  int height = render_pass->output_rect.height();
  if (!settings_->dont_round_texture_sizes_for_pixel_tests) {
    int multiple = 64;
    width = cc::MathUtil::CheckedRoundUp(width, multiple);
    height = cc::MathUtil::CheckedRoundUp(height, multiple);
  }
  return gfx::Size(width, height);
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
  const SharedQuadState* sqs = quad->shared_quad_state;
  const gfx::RRectF& rounded_corner_bounds = sqs->rounded_corner_bounds;

  // There is no rounded corner set.
  if (rounded_corner_bounds.IsEmpty())
    return false;

  const gfx::RectF target_quad = cc::MathUtil::MapClippedRect(
      sqs->quad_to_target_transform, gfx::RectF(quad->visible_rect));

  const gfx::RRectF::Corner corners[] = {
      gfx::RRectF::Corner::kUpperLeft, gfx::RRectF::Corner::kUpperRight,
      gfx::RRectF::Corner::kLowerRight, gfx::RRectF::Corner::kLowerLeft};
  for (auto c : corners) {
    if (ComputeRoundedCornerBoundingBox(rounded_corner_bounds, c)
            .Intersects(target_quad)) {
      return true;
    }
  }
  return false;
}

gfx::ColorSpace DirectRenderer::RootRenderPassColorSpace() const {
  return current_frame()->display_color_spaces.GetOutputColorSpace(
      current_frame()->root_render_pass->content_color_usage,
      current_frame()->root_render_pass->has_transparent_background);
}

gfx::ColorSpace DirectRenderer::CurrentRenderPassColorSpace() const {
  if (current_frame()->current_render_pass ==
      current_frame()->root_render_pass) {
    return RootRenderPassColorSpace();
  }
  return current_frame()->display_color_spaces.GetCompositingColorSpace(
      current_frame()->current_render_pass->has_transparent_background,
      current_frame()->current_render_pass->content_color_usage);
}

bool DirectRenderer::CreateDelegatedInkPointRenderer() {
  return false;
}

DelegatedInkPointRendererBase* DirectRenderer::GetDelegatedInkPointRenderer() {
  if (!delegated_ink_point_renderer_ && !CreateDelegatedInkPointRenderer())
    return nullptr;

  return delegated_ink_point_renderer_.get();
}

void DirectRenderer::SetDelegatedInkMetadata(
    std::unique_ptr<DelegatedInkMetadata> metadata) {
  if (!delegated_ink_point_renderer_ && !CreateDelegatedInkPointRenderer())
    return;

  delegated_ink_point_renderer_->SetDelegatedInkMetadata(std::move(metadata));
}

bool DirectRenderer::CompositeTimeTracingEnabled() {
  return false;
}

void DirectRenderer::AddCompositeTimeTraces(base::TimeTicks ready_timestamp) {}

}  // namespace viz
