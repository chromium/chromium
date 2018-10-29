// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/skia_renderer.h"

#include "base/bits.h"
#include "base/command_line.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/math_util.h"
#include "cc/paint/render_surface_filters.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/renderer_utils.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "skia/ext/opacity_filter_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorSpaceXformCanvas.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkOverdrawCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "third_party/skia/include/effects/SkOverdrawColorFilter.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "ui/gfx/geometry/axis_transform2d.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"

namespace viz {
// Parameters needed to draw a RenderPassDrawQuad.
struct SkiaRenderer::DrawRenderPassDrawQuadParams {
  // The "in" parameters that will be used when apply filters.
  const cc::FilterOperations* filters = nullptr;

  // The "out" parameters returned by filters.
  // A Skia image that should be sampled from instead of the original
  // contents.
  sk_sp<SkImage> filter_image;
  gfx::Point src_offset;
  gfx::RectF dst_rect;
  gfx::RectF tex_coord_rect;
};

namespace {

bool IsTextureResource(DisplayResourceProvider* resource_provider,
                       ResourceId resource_id) {
  return !resource_provider->IsResourceSoftwareBacked(resource_id);
}

}  // namespace

// Scoped helper class for building SkImage from resource id.
class SkiaRenderer::ScopedSkImageBuilder {
 public:
  ScopedSkImageBuilder(SkiaRenderer* skia_renderer, ResourceId resource_id);
  ~ScopedSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }

 private:
  base::Optional<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  const SkImage* sk_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedSkImageBuilder);
};

SkiaRenderer::ScopedSkImageBuilder::ScopedSkImageBuilder(
    SkiaRenderer* skia_renderer,
    ResourceId resource_id) {
  if (!resource_id)
    return;
  auto* resource_provider = skia_renderer->resource_provider_;
  if (!skia_renderer->is_using_ddl() || skia_renderer->non_root_surface_ ||
      !IsTextureResource(resource_provider, resource_id)) {
    // TODO(penghuang): remove this code when DDL is used everywhere.
    lock_.emplace(resource_provider, resource_id);
    sk_image_ = lock_->sk_image();
  } else {
    // Look up the image from promise_images_by resource_id and return the
    // reference. If the resource_id doesn't exist, this statement will
    // allocate it and return reference of it, and the reference will be used
    // to store the new created image later.
    auto& image = skia_renderer->promise_images_[resource_id];
    if (!image) {
      auto metadata =
          skia_renderer->lock_set_for_external_use_.LockResource(resource_id);
      DCHECK(!metadata.mailbox.IsZero());
      image = skia_renderer->skia_output_surface_->MakePromiseSkImage(
          std::move(metadata));
      LOG_IF(ERROR, !image) << "Failed to create the promise sk image.";
    }
    sk_image_ = image.get();
  }
}

class SkiaRenderer::ScopedYUVSkImageBuilder {
 public:
  ScopedYUVSkImageBuilder(SkiaRenderer* skia_renderer,
                          const YUVVideoDrawQuad* quad) {
    DCHECK(skia_renderer->is_using_ddl());
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->y_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->u_plane_resource_id()));
    DCHECK(IsTextureResource(skia_renderer->resource_provider_,
                             quad->v_plane_resource_id()));
    DCHECK(quad->a_plane_resource_id() == kInvalidResourceId ||
           IsTextureResource(skia_renderer->resource_provider_,
                             quad->a_plane_resource_id()));

    YUVIds ids(quad->y_plane_resource_id(), quad->u_plane_resource_id(),
               quad->v_plane_resource_id(), quad->a_plane_resource_id());
    auto& image = skia_renderer->yuv_promise_images_[std::move(ids)];

    if (!image) {
      auto yuv_color_space = kRec601_SkYUVColorSpace;
      quad->video_color_space.ToSkYUVColorSpace(&yuv_color_space);

      const bool is_i420 =
          quad->u_plane_resource_id() != quad->v_plane_resource_id();
      const bool has_alpha = quad->a_plane_resource_id() != kInvalidResourceId;
      const size_t number_of_textures = (is_i420 ? 3 : 2) + (has_alpha ? 1 : 0);
      std::vector<ResourceMetadata> metadatas;
      metadatas.reserve(number_of_textures);
      auto y_metadata = skia_renderer->lock_set_for_external_use_.LockResource(
          quad->y_plane_resource_id());
      metadatas.push_back(std::move(y_metadata));
      auto u_metadata = skia_renderer->lock_set_for_external_use_.LockResource(
          quad->u_plane_resource_id());
      metadatas.push_back(std::move(u_metadata));
      if (is_i420) {
        auto v_metadata =
            skia_renderer->lock_set_for_external_use_.LockResource(
                quad->v_plane_resource_id());
        metadatas.push_back(std::move(v_metadata));
      }

      if (has_alpha) {
        auto a_metadata =
            skia_renderer->lock_set_for_external_use_.LockResource(
                quad->a_plane_resource_id());
        metadatas.push_back(std::move(a_metadata));
      }

      image = skia_renderer->skia_output_surface_->MakePromiseSkImageFromYUV(
          std::move(metadatas), yuv_color_space, has_alpha);
      LOG_IF(ERROR, !image) << "Failed to create the promise sk yuva image.";
    }
    sk_image_ = image.get();
  }

  ~ScopedYUVSkImageBuilder() = default;

  const SkImage* sk_image() const { return sk_image_; }

 private:
  std::unique_ptr<DisplayResourceProvider::ScopedReadLockSkImage> lock_;
  SkImage* sk_image_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ScopedYUVSkImageBuilder);
};

SkiaRenderer::SkiaRenderer(const RendererSettings* settings,
                           OutputSurface* output_surface,
                           DisplayResourceProvider* resource_provider,
                           SkiaOutputSurface* skia_output_surface,
                           DrawMode mode)
    : DirectRenderer(settings, output_surface, resource_provider),
      draw_mode_(mode),
      skia_output_surface_(skia_output_surface),
      lock_set_for_external_use_(resource_provider) {
  switch (draw_mode_) {
    case DrawMode::GL: {
      DCHECK(output_surface_);
      context_provider_ = output_surface_->context_provider();
      const auto& context_caps = context_provider_->ContextCapabilities();
      use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
      if (context_caps.sync_query) {
        sync_queries_ =
            base::Optional<SyncQueryCollection>(context_provider_->ContextGL());
      }
      break;
    }
    case DrawMode::VULKAN: {
      DCHECK(output_surface_);
#if BUILDFLAG(ENABLE_VULKAN)
      vulkan_context_provider_ = output_surface_->vulkan_context_provider();
#endif
      break;
    }
    case DrawMode::DDL: {
      DCHECK(skia_output_surface_);
      break;
    }
    case DrawMode::SKPRECORD: {
      DCHECK(output_surface_);
      context_provider_ = output_surface_->context_provider();
      const auto& context_caps = context_provider_->ContextCapabilities();
      use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
      if (context_caps.sync_query) {
        sync_queries_ =
            base::Optional<SyncQueryCollection>(context_provider_->ContextGL());
      }
    }
  }
}

SkiaRenderer::~SkiaRenderer() = default;

bool SkiaRenderer::CanPartialSwap() {
  if (draw_mode_ != DrawMode::GL && draw_mode_ != DrawMode::SKPRECORD)
    return false;

  DCHECK(context_provider_);
  if (use_swap_with_bounds_)
    return false;

  return context_provider_->ContextCapabilities().post_sub_buffer;
}

void SkiaRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::BeginDrawingFrame");
  if (draw_mode_ != DrawMode::GL && draw_mode_ != DrawMode::SKPRECORD)
    return;

  // Copied from GLRenderer.
  scoped_refptr<ResourceFence> read_lock_fence;
  if (sync_queries_) {
    read_lock_fence = sync_queries_->StartNewFrame();
  } else {
    read_lock_fence =
        base::MakeRefCounted<DisplayResourceProvider::SynchronousFence>(
            context_provider_->ContextGL());
  }
  resource_provider_->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the
  // frame, so that drawing can proceed without GL context switching
  // interruptions.
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider_->WaitSyncToken(resource_id);
    }
  }
}

void SkiaRenderer::FinishDrawingFrame() {
  TRACE_EVENT0("viz", "SkiaRenderer::FinishDrawingFrame");
  if (sync_queries_) {
    sync_queries_->EndCurrentFrame();
  }
  non_root_surface_ = nullptr;
  current_canvas_ = nullptr;
  current_surface_ = nullptr;

  swap_buffer_rect_ = current_frame()->root_damage_rect;

  if (use_swap_with_bounds_)
    swap_content_bounds_ = current_frame()->root_content_bounds;
}

void SkiaRenderer::SwapBuffers(std::vector<ui::LatencyInfo> latency_info,
                               bool need_presentation_feedback) {
  DCHECK(visible_);
  TRACE_EVENT0("viz,benchmark", "SkiaRenderer::SwapBuffers");
  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(latency_info);
  output_frame.size = surface_size_for_swap_buffers();
  output_frame.need_presentation_feedback = need_presentation_feedback;
  if (use_swap_with_bounds_) {
    output_frame.content_bounds = std::move(swap_content_bounds_);
  } else if (use_partial_swap_) {
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size_for_swap_buffers()));
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  } else if (swap_buffer_rect_.IsEmpty() && allow_empty_swap_) {
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  }

  switch (draw_mode_) {
    case DrawMode::DDL: {
      skia_output_surface_->SkiaSwapBuffers(std::move(output_frame));
      break;
    }
    case DrawMode::VULKAN: {
#if BUILDFLAG(ENABLE_VULKAN)
      // TODO(penghuang): remove it when SkiaRenderer and SkDDL are always used.
      auto backend = root_surface_->getBackendRenderTarget(
          SkSurface::kFlushRead_BackendHandleAccess);
      GrVkImageInfo vk_image_info;
      if (!backend.getVkImageInfo(&vk_image_info))
        NOTREACHED() << "Failed to get the image info.";
      auto* vulkan_surface = output_surface_->GetVulkanSurface();
      auto* swap_chain = vulkan_surface->GetSwapChain();
      swap_chain->SetCurrentImageLayout(vk_image_info.fImageLayout);
      output_surface_->SwapBuffers(std::move(output_frame));
#endif
      break;
    }
    case DrawMode::GL: {
      output_surface_->SwapBuffers(std::move(output_frame));
      break;
    }
    case DrawMode::SKPRECORD: {
      // write to skp files
      std::string file_name = "composited-frame.skp";
      SkFILEWStream file(file_name.c_str());
      DCHECK(file.isValid());

      auto data = root_picture_->serialize();
      file.write(data->data(), data->size());
      file.fsync();
      root_picture_ = nullptr;
      root_recorder_.reset();
    }
  }

  swap_buffer_rect_ = gfx::Rect();
}

bool SkiaRenderer::FlippedFramebuffer() const {
  // TODO(weiliangc): Make sure flipped correctly for Windows.
  // (crbug.com/644851)
  return false;
}

void SkiaRenderer::EnsureScissorTestEnabled() {
  is_scissor_enabled_ = true;
}

void SkiaRenderer::EnsureScissorTestDisabled() {
  is_scissor_enabled_ = false;
}

void SkiaRenderer::BindFramebufferToOutputSurface() {
  DCHECK(!output_surface_->HasExternalStencilTest());
  non_root_surface_ = nullptr;

  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  // TODO(weiliangc): Set up correct can_use_lcd_text for SkSurfaceProps flags.
  // How to setup is in ResourceProvider. (http://crbug.com/644851)
  switch (draw_mode_) {
    case DrawMode::DDL: {
      root_canvas_ = skia_output_surface_->BeginPaintCurrentFrame();
      DCHECK(root_canvas_);
      break;
    }
    case DrawMode::VULKAN: {
#if BUILDFLAG(ENABLE_VULKAN)
      auto* vulkan_surface = output_surface_->GetVulkanSurface();
      auto* swap_chain = vulkan_surface->GetSwapChain();
      VkImage image = swap_chain->GetCurrentImage();
      VkImageLayout image_layout = swap_chain->GetCurrentImageLayout();
      GrVkImageInfo vk_image_info;
      vk_image_info.fImage = image;
      vk_image_info.fAlloc = {VK_NULL_HANDLE, 0, 0, 0};
      vk_image_info.fImageLayout = image_layout;
      vk_image_info.fImageTiling = VK_IMAGE_TILING_OPTIMAL;
      vk_image_info.fFormat = VK_FORMAT_B8G8R8A8_UNORM;
      vk_image_info.fLevelCount = 1;
      GrBackendRenderTarget render_target(
          current_frame()->device_viewport_size.width(),
          current_frame()->device_viewport_size.height(), 0, 0, vk_image_info);
      root_surface_ = SkSurface::MakeFromBackendRenderTarget(
          GetGrContext(), render_target, kTopLeft_GrSurfaceOrigin,
          kBGRA_8888_SkColorType, nullptr, &surface_props);
      DCHECK(root_surface_);
      root_canvas_ = root_surface_->getCanvas();
#else
      NOTREACHED();
#endif
      break;
    }
    case DrawMode::GL: {
      auto* gr_context = GetGrContext();
      if (!root_canvas_ || root_canvas_->getGrContext() != gr_context ||
          gfx::SkISizeToSize(root_canvas_->getBaseLayerSize()) !=
              current_frame()->device_viewport_size) {
        // Either no SkSurface setup yet, or new GrContext, need to create new
        // surface.
        GrGLFramebufferInfo framebuffer_info;
        framebuffer_info.fFBOID = 0;
        framebuffer_info.fFormat = GL_RGB8_OES;
        GrBackendRenderTarget render_target(
            current_frame()->device_viewport_size.width(),
            current_frame()->device_viewport_size.height(), 0, 8,
            framebuffer_info);

        root_surface_ = SkSurface::MakeFromBackendRenderTarget(
            gr_context, render_target, kBottomLeft_GrSurfaceOrigin,
            kRGB_888x_SkColorType, nullptr, &surface_props);
        DCHECK(root_surface_);
        root_canvas_ = root_surface_->getCanvas();
      }
      break;
    }
    case DrawMode::SKPRECORD: {
      root_recorder_ = std::make_unique<SkPictureRecorder>();

      current_recorder_ = root_recorder_.get();
      current_picture_ = &root_picture_;
      root_canvas_ = root_recorder_->beginRecording(
          SkRect::MakeWH(current_frame()->device_viewport_size.width(),
                         current_frame()->device_viewport_size.height()));
      break;
    }
  }

  current_canvas_ = root_canvas_;
  current_surface_ = root_surface_.get();

  if (settings_->show_overdraw_feedback) {
    const auto& size = current_frame()->device_viewport_size;
    overdraw_surface_ = root_canvas_->makeSurface(
        SkImageInfo::MakeA8(size.width(), size.height()));
    nway_canvas_ = std::make_unique<SkNWayCanvas>(size.width(), size.height());
    overdraw_canvas_ =
        std::make_unique<SkOverdrawCanvas>(overdraw_surface_->getCanvas());
    nway_canvas_->addCanvas(overdraw_canvas_.get());
    nway_canvas_->addCanvas(root_canvas_);
    current_canvas_ = nway_canvas_.get();
  }
}

void SkiaRenderer::BindFramebufferToTexture(const RenderPassId render_pass_id) {
  auto iter = render_pass_backings_.find(render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& backing = iter->second;
  switch (draw_mode_) {
    case DrawMode::DDL: {
      non_root_surface_ = nullptr;
      current_canvas_ = skia_output_surface_->BeginPaintRenderPass(
          render_pass_id, backing.size, backing.format, backing.mipmap);
      break;
    }
    case DrawMode::GL:  // Fallthrough
    case DrawMode::VULKAN: {
      non_root_surface_ = backing.render_pass_surface;
      current_surface_ = non_root_surface_.get();
      current_canvas_ = non_root_surface_->getCanvas();
      break;
    }
    case DrawMode::SKPRECORD: {
      current_recorder_ = backing.recorder.get();
      current_picture_ = &backing.picture;
      current_canvas_ = current_recorder_->beginRecording(
          SkRect::MakeWH(backing.size.width(), backing.size.height()));
    }
  }
}

void SkiaRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  is_scissor_enabled_ = true;
  scissor_rect_ = scissor_rect;
}

void SkiaRenderer::ClearCanvas(SkColor color) {
  if (!current_canvas_)
    return;

  if (is_scissor_enabled_) {
    // Limit the clear with the scissor rect.
    SkAutoCanvasRestore autoRestore(current_canvas_, true /* do_save */);
    current_canvas_->clipRect(gfx::RectToSkRect(scissor_rect_));
    current_canvas_->clear(color);
  } else {
    current_canvas_->clear(color);
  }
}

void SkiaRenderer::ClearFramebuffer() {
  if (current_frame()->current_render_pass->has_transparent_background) {
    ClearCanvas(SkColorSetARGB(0, 0, 0, 0));
  } else {
#ifndef NDEBUG
    // On DEBUG builds, opaque render passes are cleared to blue
    // to easily see regions that were not drawn on the screen.
    ClearCanvas(SkColorSetARGB(255, 0, 0, 255));
#endif
  }
}

void SkiaRenderer::PrepareSurfaceForPass(
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  switch (initialization_mode) {
    case SURFACE_INITIALIZATION_MODE_PRESERVE:
      EnsureScissorTestDisabled();
      return;
    case SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR:
      EnsureScissorTestDisabled();
      ClearFramebuffer();
      break;
    case SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR:
      SetScissorTestRect(render_pass_scissor);
      ClearFramebuffer();
      break;
  }
}

void SkiaRenderer::DoDrawQuad(const DrawQuad* quad,
                              const gfx::QuadF* draw_region) {
  if (!current_canvas_)
    return;
  TRACE_EVENT0("viz", "SkiaRenderer::DoDrawQuad");
  if (MustDrawBatchedTileQuadsBeforeQuad(quad, draw_region))
    DrawBatchedTileQuads();
  if (quad->material == DrawQuad::TILED_CONTENT) {
    AddTileQuadToBatch(TileDrawQuad::MaterialCast(quad), draw_region);
    return;
  }

  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  const gfx::Rect* scissor_rect =
      is_scissor_enabled_ ? &scissor_rect_ : nullptr;
  PrepareCanvasForDrawQuads(quad->shared_quad_state, draw_region, scissor_rect,
                            &auto_canvas_restore);

  current_paint_.reset();
  if (settings_->force_antialiasing ||
      !IsScaleAndIntegerTranslate(current_canvas_->getTotalMatrix())) {
    // TODO(danakj): Until we can enable AA only on exterior edges of the
    // layer, disable AA if any interior edges are present. crbug.com/248175
    bool all_four_edges_are_exterior =
        quad->IsTopEdge() && quad->IsLeftEdge() && quad->IsBottomEdge() &&
        quad->IsRightEdge();
    if (settings_->allow_antialiasing &&
        (settings_->force_antialiasing || all_four_edges_are_exterior))
      current_paint_.setAntiAlias(true);
    current_paint_.setFilterQuality(kLow_SkFilterQuality);
  }

  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_paint_.setBlendMode(
      static_cast<SkBlendMode>(quad->shared_quad_state->blend_mode));


  switch (quad->material) {
    case DrawQuad::DEBUG_BORDER:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::PICTURE_CONTENT:
      DrawPictureQuad(PictureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::RENDER_PASS:
      DrawRenderPassQuad(RenderPassDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::SOLID_COLOR:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TEXTURE_CONTENT:
      DrawTextureQuad(TextureDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::TILED_CONTENT:
      NOTREACHED();
      break;
    case DrawQuad::SURFACE_CONTENT:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::YUV_VIDEO_CONTENT:
      DrawYUVVideoQuad(YUVVideoDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::INVALID:
    case DrawQuad::STREAM_VIDEO_CONTENT:
      DrawUnsupportedQuad(quad);
      NOTREACHED();
      break;
  }

  current_canvas_->resetMatrix();
}

bool SkiaRenderer::MustDrawBatchedTileQuadsBeforeQuad(
    const DrawQuad* new_quad,
    const gfx::QuadF* draw_region) {
  if (batched_tiles_.empty())
    return false;

  bool has_draw_region = draw_region != nullptr;
  if (batched_tile_state_.shared_quad_state != new_quad->shared_quad_state ||
      batched_tile_state_.has_scissor_rect != is_scissor_enabled_ ||
      (is_scissor_enabled_ &&
       batched_tile_state_.scissor_rect != scissor_rect_) ||
      batched_tile_state_.has_draw_region != has_draw_region ||
      (has_draw_region && batched_tile_state_.draw_region != *draw_region))
    return true;

  switch (new_quad->material) {
    case DrawQuad::TILED_CONTENT:
      // TODO(bsalomon): Check whether we can simply assume all tiles with the
      // same SharedQuadState agree about filtering.
      return TileDrawQuad::MaterialCast(new_quad)->nearest_neighbor !=
             batched_tile_state_.is_nearest_neighbor;
    case DrawQuad::SOLID_COLOR:
      // Solid tiles from the same layer should not overlap tile quads.
      return false;
    default:
      return true;
  }
}

void SkiaRenderer::PrepareCanvasForDrawQuads(
    const SharedQuadState* shared_quad_state,
    const gfx::QuadF* draw_region,
    const gfx::Rect* scissor_rect,
    base::Optional<SkAutoCanvasRestore>* auto_canvas_restore) {
  if (draw_region || scissor_rect) {
    auto_canvas_restore->emplace(current_canvas_, true /* do_save */);
    if (scissor_rect)
      current_canvas_->clipRect(gfx::RectToSkRect(*scissor_rect));
  }

  gfx::Transform contents_device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      shared_quad_state->quad_to_target_transform;
  contents_device_transform.FlattenTo2d();
  SkMatrix sk_device_matrix;
  gfx::TransformToFlattenedSkMatrix(contents_device_transform,
                                    &sk_device_matrix);
  current_canvas_->setMatrix(sk_device_matrix);

  if (draw_region) {
    SkPath draw_region_clip_path;
    SkPoint clip_points[4];
    QuadFToSkPoints(*draw_region, clip_points);
    draw_region_clip_path.addPoly(clip_points, 4, true);
    current_canvas_->clipPath(draw_region_clip_path);
  }
}

void SkiaRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad) {
  // We need to apply the matrix manually to have pixel-sized stroke width.
  SkPoint vertices[4];
  gfx::RectToSkRect(quad->rect).toQuad(vertices);
  SkPoint transformed_vertices[4];
  current_canvas_->getTotalMatrix().mapPoints(transformed_vertices, vertices,
                                              4);
  current_canvas_->resetMatrix();

  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_paint_.setStyle(SkPaint::kStroke_Style);
  current_paint_.setStrokeWidth(quad->width);
  current_canvas_->drawPoints(SkCanvas::kPolygon_PointMode, 4,
                              transformed_vertices, current_paint_);
}

void SkiaRenderer::DrawPictureQuad(const PictureDrawQuad* quad) {
  SkMatrix content_matrix;
  content_matrix.setRectToRect(gfx::RectFToSkRect(quad->tex_coord_rect),
                               gfx::RectToSkRect(quad->rect),
                               SkMatrix::kFill_ScaleToFit);
  current_canvas_->concat(content_matrix);

  const bool needs_transparency =
      SkScalarRoundToInt(quad->shared_quad_state->opacity * 255) < 255;
  const bool disable_image_filtering =
      disable_picture_quad_image_filtering_ || quad->nearest_neighbor;

  TRACE_EVENT0("viz", "SkiaRenderer::DrawPictureQuad");

  SkCanvas* raster_canvas = current_canvas_;

  std::unique_ptr<SkCanvas> color_transform_canvas;
  // TODO(enne): color transform needs to be replicated in gles2_cmd_decoder
  color_transform_canvas = SkCreateColorSpaceXformCanvas(
      current_canvas_, gfx::ColorSpace::CreateSRGB().ToSkColorSpace());
  raster_canvas = color_transform_canvas.get();

  base::Optional<skia::OpacityFilterCanvas> opacity_canvas;
  if (needs_transparency || disable_image_filtering) {
    // TODO(aelias): This isn't correct in all cases. We should detect these
    // cases and fall back to a persistent bitmap backing
    // (http://crbug.com/280374).
    // TODO(vmpstr): Fold this canvas into playback and have raster source
    // accept a set of settings on playback that will determine which canvas to
    // apply. (http://crbug.com/594679)
    opacity_canvas.emplace(raster_canvas, quad->shared_quad_state->opacity,
                           disable_image_filtering);
    raster_canvas = &*opacity_canvas;
  }

  // Treat all subnormal values as zero for performance.
  cc::ScopedSubnormalFloatDisabler disabler;

  SkAutoCanvasRestore auto_canvas_restore(raster_canvas, true /* do_save */);
  raster_canvas->translate(-quad->content_rect.x(), -quad->content_rect.y());
  raster_canvas->clipRect(gfx::RectToSkRect(quad->content_rect));
  raster_canvas->scale(quad->contents_scale, quad->contents_scale);
  quad->display_item_list->Raster(raster_canvas);
}

void SkiaRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad) {
  current_paint_.setColor(quad->color);
  current_paint_.setAlpha(quad->shared_quad_state->opacity *
                          SkColorGetA(quad->color));
  current_canvas_->drawRect(gfx::RectToSkRect(quad->visible_rect),
                            current_paint_);
}

void SkiaRenderer::DrawTextureQuad(const TextureDrawQuad* quad) {
  ScopedSkImageBuilder builder(this, quad->resource_id());
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF uv_rect = gfx::ScaleRect(
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right),
      image->width(), image->height());
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));
  SkRect sk_uv_rect = gfx::RectFToSkRect(visible_uv_rect);
  SkRect quad_rect = gfx::RectToSkRect(quad->visible_rect);

  if (quad->y_flipped)
    current_canvas_->scale(1, -1);

  bool blend_background =
      quad->background_color != SK_ColorTRANSPARENT && !image->isOpaque();
  bool needs_layer = blend_background && (current_paint_.getAlpha() != 0xFF);
  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  if (needs_layer) {
    auto_canvas_restore.emplace(current_canvas_, false /* do_save */);
    current_canvas_->saveLayerAlpha(&quad_rect, current_paint_.getAlpha());
    current_paint_.setAlpha(0xFF);
  }
  if (blend_background) {
    SkPaint background_paint;
    background_paint.setColor(quad->background_color);
    current_canvas_->drawRect(quad_rect, background_paint);
  }
  current_paint_.setFilterQuality(
      quad->nearest_neighbor ? kNone_SkFilterQuality : kLow_SkFilterQuality);
  current_canvas_->drawImageRect(image, sk_uv_rect, quad_rect, &current_paint_);
}

void SkiaRenderer::AddTileQuadToBatch(const TileDrawQuad* quad,
                                      const gfx::QuadF* draw_region) {
  DCHECK(!MustDrawBatchedTileQuadsBeforeQuad(quad, draw_region));
  if (batched_tiles_.empty()) {
    batched_tile_state_.shared_quad_state = quad->shared_quad_state;
    batched_tile_state_.scissor_rect = scissor_rect_;
    if (draw_region) {
      batched_tile_state_.draw_region = *draw_region;
    }
    batched_tile_state_.is_nearest_neighbor = quad->nearest_neighbor;
    batched_tile_state_.has_scissor_rect = is_scissor_enabled_;
    batched_tile_state_.has_draw_region = (draw_region != nullptr);
  }

  // |resource_provider_| can be NULL in resourceless software draws, which
  // should never produce tile quads in the first place.
  DCHECK(resource_provider_);
  ScopedSkImageBuilder builder(this, quad->resource_id());
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));
  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  unsigned aa_flags = SkCanvas::kNone_QuadAAFlags;
  if (settings_->allow_antialiasing || settings_->force_antialiasing) {
    if (quad->IsLeftEdge())
      aa_flags |= SkCanvas::kLeft_QuadAAFlag;
    if (quad->IsTopEdge())
      aa_flags |= SkCanvas::kTop_QuadAAFlag;
    if (quad->IsRightEdge())
      aa_flags |= SkCanvas::kRight_QuadAAFlag;
    if (quad->IsBottomEdge())
      aa_flags |= SkCanvas::kBottom_QuadAAFlag;
  }
  batched_tiles_.push_back(
      SkCanvas::ImageSetEntry{sk_ref_sp(image), uv_rect,
                              gfx::RectToSkRect(quad->visible_rect), aa_flags});
}

void SkiaRenderer::DrawBatchedTileQuads() {
  TRACE_EVENT0("viz", "SkiaRenderer::DrawBatchedTileQuads");
  const gfx::QuadF* draw_region = batched_tile_state_.has_draw_region
                                      ? &batched_tile_state_.draw_region
                                      : nullptr;
  const gfx::Rect* scissor_rect = batched_tile_state_.has_scissor_rect
                                      ? &batched_tile_state_.scissor_rect
                                      : nullptr;
  base::Optional<SkAutoCanvasRestore> auto_canvas_restore;
  PrepareCanvasForDrawQuads(batched_tile_state_.shared_quad_state, draw_region,
                            scissor_rect, &auto_canvas_restore);

  SkFilterQuality filter_quality = batched_tile_state_.is_nearest_neighbor
                                       ? kNone_SkFilterQuality
                                       : kLow_SkFilterQuality;
  current_canvas_->experimental_DrawImageSetV0(
      &batched_tiles_.front(), batched_tiles_.size(),
      batched_tile_state_.shared_quad_state->opacity, filter_quality,
      batched_tile_state_.shared_quad_state->blend_mode);
  current_canvas_->resetMatrix();
  batched_tiles_.clear();
}

void SkiaRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad) {
  if (draw_mode_ != DrawMode::DDL) {
    NOTIMPLEMENTED();
    return;
  }

  DCHECK(resource_provider_);
  ScopedYUVSkImageBuilder builder(this, quad);
  const SkImage* image = builder.sk_image();
  if (!image)
    return;
  gfx::RectF visible_tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->ya_tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));

  SkRect uv_rect = gfx::RectFToSkRect(visible_tex_coord_rect);
  // TODO(penghuang): figure out how to set correct filter quality.
  current_paint_.setFilterQuality(kLow_SkFilterQuality);
  current_canvas_->drawImageRect(
      image, uv_rect, gfx::RectToSkRect(quad->visible_rect), &current_paint_);
}

bool SkiaRenderer::CalculateRPDQParams(sk_sp<SkImage> content,
                                       const RenderPassDrawQuad* quad,
                                       DrawRenderPassDrawQuadParams* params) {
  auto iter = render_pass_backings_.find(quad->render_pass_id);
  DCHECK(render_pass_backings_.end() != iter);
  if (params->filters == nullptr) {
    return true;
  }

  // This function is called after AllocateRenderPassResourceIfNeeded, so there
  // should be backing ready.
  RenderPassBacking& content_texture = iter->second;
  DCHECK(!params->filters->IsEmpty());
  auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
      *params->filters, gfx::SizeF(content_texture.size));
  auto filter = paint_filter ? paint_filter->cached_sk_filter_ : nullptr;

  // Apply filters to the content texture.
  // TODO(xing.xu):  Support SkColorFilter here. (https://crbug.com/823182)

  if (filter) {
    gfx::Rect clip_rect = quad->shared_quad_state->clip_rect;
    if (clip_rect.IsEmpty()) {
      clip_rect = current_draw_rect_;
    }
    gfx::Transform transform =
        quad->shared_quad_state->quad_to_target_transform;
    gfx::QuadF clip_quad = gfx::QuadF(gfx::RectF(clip_rect));
    gfx::QuadF local_clip =
        cc::MathUtil::InverseMapQuadToLocalSpace(transform, clip_quad);

    SkMatrix local_matrix;
    local_matrix.setTranslate(quad->filters_origin.x(),
                              quad->filters_origin.y());
    local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
    gfx::RectF dst_rect(params->filters
                            ? params->filters->MapRect(quad->rect, local_matrix)
                            : quad->rect);

    dst_rect.Intersect(local_clip.BoundingBox());
    // If we've been fully clipped out (by crop rect or clipping), there's
    // nothing to draw.
    if (dst_rect.IsEmpty())
      return false;

    SkIPoint offset;
    SkIRect subset;
    gfx::RectF src_rect(quad->rect);
    // TODO(xing.xu): Support flip_texture. (https://crbug.com/822859)
    params->filter_image = SkiaHelper::ApplyImageFilter(
        content, src_rect, dst_rect, quad->filters_scale, std::move(filter),
        &offset, &subset, quad->filters_origin);
    if (!params->filter_image)
      return false;
    params->dst_rect =
        gfx::RectF(src_rect.x() + offset.fX, src_rect.y() + offset.fY,
                   subset.width(), subset.height());
    params->src_offset.SetPoint(subset.x(), subset.y());
    gfx::RectF tex_rect =
        gfx::RectF(gfx::PointF(params->src_offset), params->dst_rect.size());
    params->tex_coord_rect = tex_rect;
  }
  return true;
}

const TileDrawQuad* SkiaRenderer::CanPassBeDrawnDirectly(
    const RenderPass* pass) {
  return DirectRenderer::CanPassBeDrawnDirectly(pass, is_using_vulkan(),
                                                resource_provider_);
}

void SkiaRenderer::DrawRenderPassQuad(const RenderPassDrawQuad* quad) {
  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  // When Render Pass has a single quad inside we would draw that directly.
  if (bypass != render_pass_bypass_quads_.end()) {
    TileDrawQuad* tile_quad = &bypass->second;
    ScopedSkImageBuilder builder(this, tile_quad->resource_id());
    sk_sp<SkImage> content_image = sk_ref_sp(builder.sk_image());
    DrawRenderPassQuadInternal(quad, content_image);
  } else {
    auto iter = render_pass_backings_.find(quad->render_pass_id);
    DCHECK(render_pass_backings_.end() != iter);
    // This function is called after AllocateRenderPassResourceIfNeeded, so
    // there should be backing ready.
    RenderPassBacking& backing = iter->second;

    sk_sp<SkImage> content_image;
    switch (draw_mode_) {
      case DrawMode::DDL: {
        content_image = skia_output_surface_->MakePromiseSkImageFromRenderPass(
            quad->render_pass_id, backing.size, backing.format, backing.mipmap);
        break;
      }
      case DrawMode::GL:  // Fallthrough
      case DrawMode::VULKAN: {
        content_image = backing.render_pass_surface->makeImageSnapshot();
        break;
      }
      case DrawMode::SKPRECORD: {
        content_image = SkImage::MakeFromPicture(
            backing.picture,
            SkISize::Make(backing.size.width(), backing.size.height()), nullptr,
            nullptr, SkImage::BitDepth::kU8,
            backing.color_space.ToSkColorSpace());
        return;
      }
    }

    DrawRenderPassQuadInternal(quad, content_image);
  }
}

void SkiaRenderer::DrawRenderPassQuadInternal(const RenderPassDrawQuad* quad,
                                              sk_sp<SkImage> content_image) {
  DrawRenderPassDrawQuadParams params;
  params.filters = FiltersForPass(quad->render_pass_id);
  bool can_draw = CalculateRPDQParams(content_image, quad, &params);

  if (!can_draw)
    return;

  SkRect content_rect;
  SkRect dest_visible_rect;
  if (params.filter_image) {
    content_rect = RectFToSkRect(params.tex_coord_rect);
    dest_visible_rect = gfx::RectFToSkRect(params.dst_rect);
    content_image = params.filter_image;
  } else {
    content_rect = RectFToSkRect(quad->tex_coord_rect);
    dest_visible_rect = gfx::RectToSkRect(quad->visible_rect);
  }

  // Prepare mask.
  ScopedSkImageBuilder mask_image_builder(this, quad->mask_resource_id());
  const SkImage* mask_image = mask_image_builder.sk_image();
  DCHECK_EQ(!!quad->mask_resource_id(), !!mask_image);
  SkMatrix mask_to_dest_matrix;
  sk_sp<SkMaskFilter> mask_filter;
  if (mask_image) {
    // Scale normalized uv rect into absolute texel coordinates.
    SkRect mask_rect = gfx::RectFToSkRect(
        gfx::ScaleRect(quad->mask_uv_rect, quad->mask_texture_size.width(),
                       quad->mask_texture_size.height()));
    mask_to_dest_matrix.setRectToRect(mask_rect, gfx::RectToSkRect(quad->rect),
                                      SkMatrix::kFill_ScaleToFit);
    mask_filter =
        SkShaderMaskFilter::Make(mask_image->makeShader(&mask_to_dest_matrix));
    DCHECK(mask_filter);
  }

  const cc::FilterOperations* backdrop_filters =
      BackgroundFiltersForPass(quad->render_pass_id);
  // Without backdrop effect.
  if (!ShouldApplyBackgroundFilters(quad, backdrop_filters)) {
    if (!mask_filter) {
      // Not mask, so we just draw the context_image directly.
      current_canvas_->drawImageRect(content_image, content_rect,
                                     dest_visible_rect, &current_paint_);
      return;
    }

    // With mask, we need convert the content_image to a shader, and use
    // drawRect() with the shader and the mask.
    current_paint_.setMaskFilter(mask_filter);
    // Convert the content_image to a shader, and use drawRect() with the
    // shader.
    SkMatrix content_to_dest_matrix;
    content_to_dest_matrix.setRectToRect(content_rect,
                                         gfx::RectToSkRect(quad->rect),
                                         SkMatrix::kFill_ScaleToFit);
    auto shader = content_image->makeShader(&content_to_dest_matrix);
    current_paint_.setShader(std::move(shader));
    current_canvas_->drawRect(dest_visible_rect, current_paint_);
    return;
  }

  // Draw render pass with backdrop effects.
  auto background_paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
      *backdrop_filters,
      gfx::SizeF(content_image->width(), content_image->height()));
  auto background_image_filter =
      background_paint_filter ? background_paint_filter->cached_sk_filter_
                              : nullptr;
  DCHECK(background_image_filter);
  SkMatrix content_to_dest_matrix;
  content_to_dest_matrix.setRectToRect(
      content_rect, gfx::RectToSkRect(quad->rect), SkMatrix::kFill_ScaleToFit);
  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  local_matrix.postConcat(content_to_dest_matrix);
  background_image_filter =
      background_image_filter->makeWithLocalMatrix(local_matrix);

  SkAutoCanvasRestore auto_canvas_restore(current_canvas_, true /* do_save */);
  current_canvas_->clipRect(gfx::RectToSkRect(quad->rect));

  SkPaint paint;
  paint.setMaskFilter(mask_filter);
  SkCanvas::SaveLayerRec rec(&dest_visible_rect, &paint,
                             background_image_filter.get(),
                             SkCanvas::kInitWithPrevious_SaveLayerFlag);
  // Lift content in the current_canvas_ into a new layer with
  // background_image_filter, and then paint content_image in the layer,
  // and then the current_canvas_->restore() will drop the layer into the
  // canvas.
  SkAutoCanvasRestore auto_canvas_restore_for_save_layer(current_canvas_,
                                                         false /* do_save */);
  current_canvas_->saveLayer(rec);
  current_canvas_->drawImageRect(content_image, content_rect, dest_visible_rect,
                                 &current_paint_);
}

void SkiaRenderer::DrawUnsupportedQuad(const DrawQuad* quad) {
  // TODO(weiliangc): Make sure unsupported quads work. (crbug.com/644851)
  NOTIMPLEMENTED();
#ifdef NDEBUG
  current_paint_.setColor(SK_ColorWHITE);
#else
  current_paint_.setColor(SK_ColorMAGENTA);
#endif
  current_paint_.setAlpha(quad->shared_quad_state->opacity * 255);
  current_canvas_->drawRect(gfx::RectToSkRect(quad->rect), current_paint_);
}

void SkiaRenderer::CopyDrawnRenderPass(
    std::unique_ptr<CopyOutputRequest> request) {
  // TODO(weiliangc): Make copy request work. (crbug.com/644851)
  TRACE_EVENT0("viz", "SkiaRenderer::CopyDrawnRenderPass");

  gfx::Rect copy_rect = current_frame()->current_render_pass->output_rect;
  if (request->has_area())
    copy_rect.Intersect(request->area());

  if (copy_rect.IsEmpty())
    return;

  gfx::Rect window_copy_rect = MoveFromDrawToWindowSpace(copy_rect);

  if (request->result_format() != CopyOutputResult::Format::RGBA_BITMAP ||
      request->is_scaled() ||
      (request->has_result_selection() &&
       request->result_selection() == gfx::Rect(copy_rect.size()))) {
    // TODO(crbug.com/644851): Complete the implementation for all request
    // types, scaling, etc.
    NOTIMPLEMENTED();
    return;
  }

  switch (draw_mode_) {
    case DrawMode::DDL: {
      if (settings_->show_overdraw_feedback) {
        // TODO(crbug.com/889122): Overdraw currently requires calling flush on
        // canvas on SkiaRenderer's thread.
        return;
      }
      // Root framebuffer uses id 0 in SkiaOutputSurface.
      RenderPassId render_pass_id = 0;
      // If we are in child render pass and we don't have overdraw, copy the
      // current render pass.
      if (root_canvas_ != current_canvas_)
        render_pass_id = current_frame()->current_render_pass->id;
      skia_output_surface_->CopyOutput(render_pass_id, window_copy_rect,
                                       std::move(request));
      break;
    }
    case DrawMode::GL:  // Fallthrough
    case DrawMode::VULKAN: {
      sk_sp<SkImage> copy_image =
          current_surface_->makeImageSnapshot()->makeSubset(
              RectToSkIRect(window_copy_rect));

      // Send copy request by copying into a bitmap.
      SkBitmap bitmap;
      copy_image->asLegacyBitmap(&bitmap);
      request->SendResult(
          std::make_unique<CopyOutputSkBitmapResult>(copy_rect, bitmap));
      break;
    }
    case DrawMode::SKPRECORD: {
      NOTIMPLEMENTED();
      break;
    }
  }
}

void SkiaRenderer::SetEnableDCLayers(bool enable) {
  // TODO(crbug.com/678800): Part of surport overlay on Windows.
  NOTIMPLEMENTED();
}

void SkiaRenderer::DidChangeVisibility() {
  if (visible_)
    output_surface_->EnsureBackbuffer();
  else
    output_surface_->DiscardBackbuffer();
}

void SkiaRenderer::FinishDrawingQuadList() {
  if (!batched_tiles_.empty())
    DrawBatchedTileQuads();
  switch (draw_mode_) {
    case DrawMode::DDL: {
      gpu::SyncToken sync_token = skia_output_surface_->SubmitPaint();
      promise_images_.clear();
      yuv_promise_images_.clear();
      lock_set_for_external_use_.UnlockResources(sync_token);
      break;
    }
    case DrawMode::GL:  // Fallthrough
    case DrawMode::VULKAN: {
      // For SkiaRendererPixelTestWithOverdrawFeedback, CopyDrawnRenderPass
      // happens before FinishDrawingFrame which results in an empty image. So
      // force a draw here.
      if (settings_->show_overdraw_feedback &&
          (current_frame()->current_render_pass ==
           current_frame()->root_render_pass)) {
        sk_sp<SkImage> image = overdraw_surface_->makeImageSnapshot();
        SkPaint paint;
        // TODO(xing.xu) : handle this in CPU mode, the R and B should be
        // switched in CPU mode. (http://crbug.com/896969)
        static const SkPMColor colors[SkOverdrawColorFilter::kNumColors] = {
            0x00000000, 0x00000000, 0x2fff0000,
            0x2f00ff00, 0x3f0000ff, 0x7f0000ff,
        };
        sk_sp<SkColorFilter> color_filter = SkOverdrawColorFilter::Make(colors);
        paint.setColorFilter(color_filter);
        current_surface_->getCanvas()->drawImage(image.get(), 0, 0, &paint);
      }
      current_canvas_->flush();
      break;
    }
    case DrawMode::SKPRECORD: {
      current_canvas_->flush();
      sk_sp<SkPicture> picture = current_recorder_->finishRecordingAsPicture();
      *current_picture_ = picture;
    }
  }
}

void SkiaRenderer::GenerateMipmap() {
  // TODO(reveman): Generates mipmaps for current canvas. (crbug.com/763664)
  NOTIMPLEMENTED();
}

bool SkiaRenderer::ShouldApplyBackgroundFilters(
    const RenderPassDrawQuad* quad,
    const cc::FilterOperations* backdrop_filters) const {
  if (!backdrop_filters)
    return false;
  DCHECK(!backdrop_filters->IsEmpty());

  // TODO(hendrikw): Look into allowing background filters to see pixels from
  // other render targets.  See crbug.com/314867.

  return true;
}

GrContext* SkiaRenderer::GetGrContext() {
  switch (draw_mode_) {
    case DrawMode::DDL:
      return nullptr;
    case DrawMode::VULKAN:
#if BUILDFLAG(ENABLE_VULKAN)
      return vulkan_context_provider_->GetGrContext();
#else
      NOTREACHED();
      return nullptr;
#endif
    case DrawMode::GL:
      return context_provider_->GrContext();
    case DrawMode::SKPRECORD:
      return nullptr;
  }
}

void SkiaRenderer::UpdateRenderPassTextures(
    const RenderPassList& render_passes_in_draw_order,
    const base::flat_map<RenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  std::vector<RenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_backings_) {
    auto render_pass_it = render_passes_in_frame.find(pair.first);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }

    const RenderPassRequirements& requirements = render_pass_it->second;
    const RenderPassBacking& backing = pair.second;
    bool size_appropriate = backing.size.width() >= requirements.size.width() &&
                            backing.size.height() >= requirements.size.height();
    bool mipmap_appropriate = !requirements.mipmap || backing.mipmap;
    if (!size_appropriate || !mipmap_appropriate)
      passes_to_delete.push_back(pair.first);
  }

  // Delete RenderPass backings from the previous frame that will not be used
  // again.
  for (size_t i = 0; i < passes_to_delete.size(); ++i) {
    auto it = render_pass_backings_.find(passes_to_delete[i]);
    render_pass_backings_.erase(it);
  }

  if (is_using_ddl() && !passes_to_delete.empty()) {
    skia_output_surface_->RemoveRenderPassResource(std::move(passes_to_delete));
  }
}

void SkiaRenderer::AllocateRenderPassResourceIfNeeded(
    const RenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto it = render_pass_backings_.find(render_pass_id);
  if (it != render_pass_backings_.end())
    return;

  // TODO(penghuang): check supported format correctly.
  gpu::Capabilities caps;
  caps.texture_format_bgra8888 = true;
  GrContext* gr_context = GetGrContext();
  switch (draw_mode_) {
    case DrawMode::DDL:
      break;
    case DrawMode::VULKAN: {
      // TODO(penghuang): check supported format correctly.
      caps.texture_format_bgra8888 = true;
      break;
    }
    case DrawMode::GL: {
      caps.texture_format_bgra8888 =
          context_provider_->ContextCapabilities().texture_format_bgra8888;
      break;
    }
    case DrawMode::SKPRECORD: {
      render_pass_backings_.emplace(
          std::move(render_pass_id),
          RenderPassBacking(requirements.size, requirements.mipmap,
                            current_frame()->current_render_pass->color_space));
      return;
    }
  }

  render_pass_backings_.emplace(
      std::move(render_pass_id),
      RenderPassBacking(gr_context, caps, requirements.size,
                        requirements.mipmap,
                        current_frame()->current_render_pass->color_space));
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    GrContext* gr_context,
    const gpu::Capabilities& caps,
    const gfx::Size& size,
    bool mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), mipmap(mipmap), color_space(color_space) {
  if (color_space.IsHDR()) {
    // If a platform does not support half-float renderbuffers then it should
    // not should request HDR rendering.
    // DCHECK(caps.texture_half_float_linear);
    // DCHECK(caps.color_buffer_half_float_rgba);
    format = RGBA_F16;
  } else {
    format = PlatformColor::BestSupportedTextureFormat(caps);
  }

  // For DDL, we don't need create teh render_pass_surface here, and we will
  // create the SkSurface by SkiaOutputSurface on Gpu thread.
  if (!gr_context)
    return;

  constexpr uint32_t flags = 0;
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(flags, SkSurfaceProps::kLegacyFontHost_InitType);
  int msaa_sample_count = 0;
  SkColorType color_type =
      ResourceFormatToClosestSkColorType(true /* gpu_compositing*/, format);
  SkImageInfo image_info = SkImageInfo::Make(
      size.width(), size.height(), color_type, kPremul_SkAlphaType, nullptr);
  render_pass_surface = SkSurface::MakeRenderTarget(
      gr_context, SkBudgeted::kNo, image_info, msaa_sample_count,
      kTopLeft_GrSurfaceOrigin, &surface_props, mipmap);
}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    const gfx::Size& size,
    bool mipmap,
    const gfx::ColorSpace& color_space)
    : size(size), mipmap(mipmap), color_space(color_space) {
  recorder = std::make_unique<SkPictureRecorder>();
}

SkiaRenderer::RenderPassBacking::~RenderPassBacking() {}

SkiaRenderer::RenderPassBacking::RenderPassBacking(
    SkiaRenderer::RenderPassBacking&& other)
    : size(other.size),
      mipmap(other.mipmap),
      color_space(other.color_space),
      format(other.format) {
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
}

SkiaRenderer::RenderPassBacking& SkiaRenderer::RenderPassBacking::operator=(
    SkiaRenderer::RenderPassBacking&& other) {
  size = other.size;
  mipmap = other.mipmap;
  color_space = other.color_space;
  format = other.format;
  render_pass_surface = other.render_pass_surface;
  other.render_pass_surface = nullptr;
  recorder = std::move(other.recorder);
  return *this;
}

bool SkiaRenderer::IsRenderPassResourceAllocated(
    const RenderPassId& render_pass_id) const {
  auto it = render_pass_backings_.find(render_pass_id);
  return it != render_pass_backings_.end();
}

gfx::Size SkiaRenderer::GetRenderPassBackingPixelSize(
    const RenderPassId& render_pass_id) {
  auto it = render_pass_backings_.find(render_pass_id);
  DCHECK(it != render_pass_backings_.end());
  return it->second.size;
}

}  // namespace viz
