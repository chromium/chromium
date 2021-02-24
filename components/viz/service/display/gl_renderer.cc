// Copyright 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/gl_renderer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <numeric>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/stl_util.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/paint/render_surface_filters.h"
#include "cc/raster/scoped_gpu_raster.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/stream_video_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/platform_color.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/skia_helper.h"
#include "components/viz/common/viz_utils.h"
#include "components/viz/service/display/draw_polygon.h"
#include "components/viz/service/display/dynamic_geometry_binding.h"
#include "components/viz/service/display/layer_quad.h"
#include "components/viz/service/display/output_surface.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_fence.h"
#include "components/viz/service/display/scoped_render_pass_texture.h"
#include "components/viz/service/display/static_geometry_binding.h"
#include "components/viz/service/display/texture_deleter.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_allocation.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/effects/SkShaderMaskFilter.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/rrect_f.h"
#include "ui/gfx/skia_util.h"

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"
#endif

using gpu::gles2::GLES2Interface;

namespace viz {
namespace {

Float4 UVTransform(const TextureDrawQuad* quad) {
  gfx::RectF uv_rect =
      gfx::BoundingRect(quad->uv_top_left, quad->uv_bottom_right);
  gfx::RectF visible_uv_rect = cc::MathUtil::ScaleRectProportional(
      uv_rect, gfx::RectF(quad->rect), gfx::RectF(quad->visible_rect));

  gfx::PointF uv0 = visible_uv_rect.origin();
  gfx::PointF uv1 = visible_uv_rect.bottom_right();
  Float4 xform = {{uv0.x(), uv0.y(), uv1.x() - uv0.x(), uv1.y() - uv0.y()}};
  if (quad->y_flipped) {
    xform.data[1] = 1.0f - xform.data[1];
    xform.data[3] = -xform.data[3];
  }
  return xform;
}

// To prevent sampling outside the visible rect.
Float4 UVClampRect(gfx::RectF uv_visible_rect,
                   const gfx::Size& texture_size,
                   SamplerType sampler) {
  gfx::SizeF half_texel(0.5f, 0.5f);
  if (sampler != SAMPLER_TYPE_2D_RECT) {
    half_texel.Scale(1.f / texture_size.width(), 1.f / texture_size.height());
  } else {
    uv_visible_rect.Scale(texture_size.width(), texture_size.height());
  }
  uv_visible_rect.Inset(half_texel.width(), half_texel.height());
  return {{uv_visible_rect.x(), uv_visible_rect.y(), uv_visible_rect.right(),
           uv_visible_rect.bottom()}};
}

Float4 PremultipliedColor(SkColor color, float opacity) {
  const U8CPU alpha255 = SkColorGetA(color);
  const unsigned int alpha256 = alpha255 + 1;
  const unsigned int premultiplied_red = (SkColorGetR(color) * alpha256) >> 8;
  const unsigned int premultiplied_green = (SkColorGetG(color) * alpha256) >> 8;
  const unsigned int premultiplied_blue = (SkColorGetB(color) * alpha256) >> 8;
  const float factor = opacity / 255.0f;
  return {{premultiplied_red * factor, premultiplied_green * factor,
           premultiplied_blue * factor, alpha255 * factor}};
}

SamplerType SamplerTypeFromTextureTarget(GLenum target) {
  switch (target) {
    case GL_TEXTURE_2D:
      return SAMPLER_TYPE_2D;
    case GL_TEXTURE_RECTANGLE_ARB:
      return SAMPLER_TYPE_2D_RECT;
    case GL_TEXTURE_EXTERNAL_OES:
      return SAMPLER_TYPE_EXTERNAL_OES;
    default:
      NOTREACHED();
      return SAMPLER_TYPE_2D;
  }
}

BlendMode BlendModeFromSkXfermode(SkBlendMode mode) {
  switch (mode) {
    case SkBlendMode::kSrcOver:
      return BLEND_MODE_NORMAL;
    case SkBlendMode::kDstIn:
      return BLEND_MODE_DESTINATION_IN;
    case SkBlendMode::kScreen:
      return BLEND_MODE_SCREEN;
    case SkBlendMode::kOverlay:
      return BLEND_MODE_OVERLAY;
    case SkBlendMode::kDarken:
      return BLEND_MODE_DARKEN;
    case SkBlendMode::kLighten:
      return BLEND_MODE_LIGHTEN;
    case SkBlendMode::kColorDodge:
      return BLEND_MODE_COLOR_DODGE;
    case SkBlendMode::kColorBurn:
      return BLEND_MODE_COLOR_BURN;
    case SkBlendMode::kHardLight:
      return BLEND_MODE_HARD_LIGHT;
    case SkBlendMode::kSoftLight:
      return BLEND_MODE_SOFT_LIGHT;
    case SkBlendMode::kDifference:
      return BLEND_MODE_DIFFERENCE;
    case SkBlendMode::kExclusion:
      return BLEND_MODE_EXCLUSION;
    case SkBlendMode::kMultiply:
      return BLEND_MODE_MULTIPLY;
    case SkBlendMode::kHue:
      return BLEND_MODE_HUE;
    case SkBlendMode::kSaturation:
      return BLEND_MODE_SATURATION;
    case SkBlendMode::kColor:
      return BLEND_MODE_COLOR;
    case SkBlendMode::kLuminosity:
      return BLEND_MODE_LUMINOSITY;
    case SkBlendMode::kSrc:
      return BLEND_MODE_NONE;
    default:
      NOTREACHED();
      return BLEND_MODE_NONE;
  }
}

// Adds a timer query that spans all GL calls in its scope. |viz.composite_time|
// trace category must be enabled for this to work.
// Note:: Multiple timer queries cannot be nested.
class ScopedTimerQuery {
 public:
  ScopedTimerQuery(bool tracing_enabled,
                   gpu::gles2::GLES2Interface* gl,
                   base::queue<std::pair<unsigned, std::string>>* timer_queries,
                   const std::string& quad_type_str)
      : gl_(gl) {
    if (!tracing_enabled) {
      gl_ = nullptr;
      return;
    }
    unsigned timer_query;
    gl_->GenQueriesEXT(1, &timer_query);
    gl_->BeginQueryEXT(GL_TIME_ELAPSED_EXT, timer_query);
    timer_queries->emplace(timer_query, quad_type_str);
  }

  ~ScopedTimerQuery() {
    if (gl_)
      gl_->EndQueryEXT(GL_TIME_ELAPSED_EXT);
  }

 private:
  gpu::gles2::GLES2Interface* gl_;
};

void AccumulateDrawRects(const gfx::Rect& quad_rect,
                         const gfx::Transform& target_transform,
                         std::vector<gfx::Rect>* drawn_rects) {
  gfx::RectF quad_rect_f(quad_rect);

  // If the transform is not axis aligned then assume the largest possible
  // bounds the quad can take in the render target. In this case, we take the
  // sum of 2 sides.
  if (!target_transform.Preserves2dAxisAlignment()) {
    // Increase the length of each side to |width + height|.
    const int total_length = quad_rect.width() + quad_rect.height();
    quad_rect_f.set_height(total_length);
    quad_rect_f.set_width(total_length);

    // Ensure that the increase is equally distributed on either sides of the
    // quad such that the position of the center of the quad does not change.
    const float delta_x = -(quad_rect.height() / 2.f);
    const float delta_y = -(quad_rect.width() / 2.f);
    quad_rect_f.Offset(gfx::Vector2d(delta_x, delta_y));

    // Apply only the scale and translation component.
    const gfx::Vector2dF& translate = target_transform.To2dTranslation();
    const gfx::Vector2dF& scale = target_transform.Scale2d();
    quad_rect_f.Scale(scale.x(), scale.y());
    quad_rect_f.Offset(translate.x(), translate.y());
  } else {
    target_transform.TransformRect(&quad_rect_f);
  }
  drawn_rects->push_back(gfx::ToRoundedRect(quad_rect_f));
}

// Smallest unit that impact anti-aliasing output. We use this to
// determine when anti-aliasing is unnecessary.
const float kAntiAliasingEpsilon = 1.0f / 1024.0f;

// A dummy timer query ID used to identify the beginning of a frame in the queue
// of timer queries.
const unsigned kTimerQueryDummy = 0;

}  // anonymous namespace

static GLint GetActiveTextureUnit(GLES2Interface* gl) {
  GLint active_unit = 0;
  gl->GetIntegerv(GL_ACTIVE_TEXTURE, &active_unit);
  return active_unit;
}

// Parameters needed to draw a CompositorRenderPassDrawQuad.
struct GLRenderer::DrawRenderPassDrawQuadParams {
  DrawRenderPassDrawQuadParams() {}
  ~DrawRenderPassDrawQuadParams() {
    // Don't leak the texture.
    DCHECK(!background_texture);
  }

  // Required inputs below.
  const AggregatedRenderPassDrawQuad* quad = nullptr;

  // Either |contents_texture| or |bypass_quad_texture| is populated. The
  // |contents_texture| will be valid if non-null, and when null the
  // bypass_quad_texture will be valid instead.
  ScopedRenderPassTexture* contents_texture = nullptr;
  struct {
    ResourceId resource_id = kInvalidResourceId;
    gfx::Size size;
  } bypass_quad_texture;

  const gfx::QuadF* clip_region = nullptr;
  bool flip_texture = false;

  // |window_matrix| maps from [-1,-1]-[1,1] unit square coordinates to window
  // pixel coordinates.
  gfx::Transform window_matrix;
  // |projection_matrix| maps texture coordinates (in pixels) to the 2D plane in
  // [-1,-1]-[1,1] unit square coordinates. If FlippedFrameBuffer() is true,
  // |projection_matrix| includes this flip.
  gfx::Transform projection_matrix;
  // |quad_to_target_transform| transforms from local quad pixel coordinates to
  // target content space pixel coordinates, including scale, offset,
  // perspective, and rotation.
  gfx::Transform quad_to_target_transform;
  const cc::FilterOperations* filters = nullptr;
  const cc::FilterOperations* backdrop_filters = nullptr;
  base::Optional<gfx::RRectF> backdrop_filter_bounds;

  // Whether the texture to be sampled from needs to be flipped.
  bool source_needs_flip = false;

  float edge[24];
  SkScalar color_matrix[20];

  // Blending in the fragment shaders is used for modifications to the backdrop
  // and for supporting advanced blending equation when not available by the
  // underlying graphics API.
  bool use_shaders_for_blending = false;
  SkBlendMode blend_mode = SkBlendMode::kSrcOver;

  bool use_aa = false;

  // Some filters affect pixels outside the original contents bounds, in which
  // case ApplyImageFilter will modify this rect.
  gfx::RectF dst_rect;

  // A Skia image that should be sampled from instead of the original
  // contents.
  sk_sp<SkImage> filter_image;

  // The original contents, bound for sampling.
  std::unique_ptr<DisplayResourceProviderGL::ScopedSamplerGL>
      bypass_quad_resource_lock;

  // A mask to be applied when drawing the RPDQ.
  std::unique_ptr<DisplayResourceProviderGL::ScopedSamplerGL>
      mask_resource_lock;

  // Whether a color matrix needs to be applied by the shaders when drawing
  // the RPDQ.
  bool use_color_matrix = false;

  gfx::QuadF surface_quad;

  // |contents_device_transform| transforms from vertex geometry, which is often
  // the unit quad [-0.5, 0.5], all the way to 2D window pixel coordinates,
  // including 3D effects, frame buffer orientation, and window offset. The
  // definition of the incoming vertex geometry comes from either
  // shared_geometry_ or clipped_geometry_, which are initialized from
  // DirectRenderer::QuadVertexRect or DynamicGeometryBinding, respectively.
  // |contents_device_transform| is typically calculated as
  //    |window_matrix| * |projection_matrix| * |quad_rect_matrix|
  // and then flattened with FlattenTo2d(). Here, |quad_rect_matrix| is a
  // combination of the geometry->quad transform as well as the quad->target
  // space transform. The geometry->quad is the mapping from the bound geometry,
  // often [-0.5, 0.5], to the quad, which is quad->rect.
  gfx::Transform contents_device_transform;

  gfx::RectF tex_coord_rect;

  // The color space of the texture bound for sampling (from filter_image or
  // bypass_quad_resource_lock, depending on the path taken).
  gfx::ColorSpace contents_and_bypass_color_space;

  // Background filters block.
  // Original background texture.
  uint32_t background_texture = 0;
  GLenum background_texture_format = 0;
  // Backdrop bounding box.
  gfx::Rect background_rect;
  // Filtered background texture.
  sk_sp<SkImage> background_image;
  GLuint background_image_id = 0;
  // A multiplier for the temporary surface we create to apply the backdrop
  // filter.
  float backdrop_filter_quality = 1.0;
  // Whether the original background texture is needed for the mask.
  bool mask_for_background = false;

  bool apply_shader_based_rounded_corner = true;
};

class GLRenderer::ScopedUseGrContext {
 public:
  static std::unique_ptr<ScopedUseGrContext> Create(GLRenderer* renderer) {
    // GrContext for filters is created lazily, and may fail if the context
    // is lost.
    // TODO(vmiura,bsalomon): crbug.com/487850 Ensure that
    // ContextProvider::GrContext() does not return NULL.
    GrDirectContext* direct = GrAsDirectContext(
        renderer->output_surface_->context_provider()->GrContext());
    if (direct)
      return base::WrapUnique(new ScopedUseGrContext(renderer));
    return nullptr;
  }

  ~ScopedUseGrContext() {
    // Pass context control back to GLrenderer.
    scoped_gpu_raster_ = nullptr;
    renderer_->RestoreGLStateAfterSkia();
  }

  GrDirectContext* context() const {
    return renderer_->output_surface_->context_provider()->GrContext();
  }

 private:
  explicit ScopedUseGrContext(GLRenderer* renderer)
      : scoped_gpu_raster_(new cc::ScopedGpuRaster(
            renderer->output_surface_->context_provider())),
        renderer_(renderer) {
    // scoped_gpu_raster_ passes context control to Skia.
  }

  std::unique_ptr<cc::ScopedGpuRaster> scoped_gpu_raster_;
  GLRenderer* renderer_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUseGrContext);
};

GLRenderer::GLRenderer(
    const RendererSettings* settings,
    const DebugRendererSettings* debug_settings,
    OutputSurface* output_surface,
    DisplayResourceProviderGL* resource_provider,
    OverlayProcessorInterface* overlay_processor,
    scoped_refptr<base::SingleThreadTaskRunner> current_task_runner)
    : DirectRenderer(settings,
                     debug_settings,
                     output_surface,
                     resource_provider,
                     overlay_processor),
      shared_geometry_quad_(QuadVertexRect()),
      gl_(output_surface->context_provider()->ContextGL()),
      context_support_(output_surface->context_provider()->ContextSupport()),
      texture_deleter_(current_task_runner),
      copier_(output_surface->context_provider(), &texture_deleter_),
      sync_queries_(gl_),
      bound_geometry_(NO_BINDING),
      current_task_runner_(std::move(current_task_runner)) {
  DCHECK(gl_);
  DCHECK(context_support_);

  const auto& context_caps =
      output_surface_->context_provider()->ContextCapabilities();

  use_discard_framebuffer_ = context_caps.discard_framebuffer;
  use_sync_query_ = context_caps.sync_query;
  use_blend_equation_advanced_ = context_caps.blend_equation_advanced;
  use_blend_equation_advanced_coherent_ =
      context_caps.blend_equation_advanced_coherent;
  use_occlusion_query_ = context_caps.occlusion_query;
  use_timer_query_ = context_caps.timer_queries;
  use_swap_with_bounds_ = context_caps.swap_buffers_with_bounds;
  prefer_draw_to_copy_ = output_surface_->context_provider()
                             ->GetGpuFeatureInfo()
                             .IsWorkaroundEnabled(gpu::PREFER_DRAW_TO_COPY);
  use_fast_path_solid_color_quad_ =
      features::IsUsingFastPathForSolidColorQuad();
  InitializeSharedObjects();
}

GLRenderer::~GLRenderer() {
  CleanupSharedObjects();

  auto* context_provider = output_surface_->context_provider();
  auto* cache_controller = context_provider->CacheController();

  if (context_busy_) {
    cache_controller->ClientBecameNotBusy(std::move(context_busy_));
  }
  if (context_visibility_) {
    cache_controller->ClientBecameNotVisibleDuringShutdown(
        std::move(context_visibility_));
  }
}

bool GLRenderer::CanPartialSwap() {
  if (use_swap_with_bounds_)
    return false;
  auto* context_provider = output_surface_->context_provider();
  return context_provider->ContextCapabilities().post_sub_buffer;
}

void GLRenderer::DidChangeVisibility() {
  if (visible_) {
    output_surface_->EnsureBackbuffer();
  } else {
    TRACE_EVENT0("viz", "GLRenderer::DidChangeVisibility dropping resources");
    ReleaseRenderPassTextures();
    output_surface_->DiscardBackbuffer();
    gl_->ReleaseShaderCompiler();
  }

  PrepareGeometry(NO_BINDING);

  auto* context_provider = output_surface_->context_provider();
  auto* cache_controller = context_provider->CacheController();
  if (visible_) {
    DCHECK(!context_visibility_);
    context_visibility_ = cache_controller->ClientBecameVisible();
  } else {
    DCHECK(context_visibility_);
    cache_controller->ClientBecameNotVisible(std::move(context_visibility_));
  }
}

void GLRenderer::ReleaseRenderPassTextures() {
  render_pass_textures_.clear();
  render_pass_backdrop_textures_.clear();
}

void GLRenderer::DiscardPixels() {
  if (!use_discard_framebuffer_)
    return;
  bool using_default_framebuffer =
      !current_framebuffer_texture_ &&
      output_surface_->capabilities().uses_default_gl_framebuffer;
  GLenum attachments[] = {static_cast<GLenum>(
      using_default_framebuffer ? GL_COLOR_EXT : GL_COLOR_ATTACHMENT0_EXT)};
  gl_->DiscardFramebufferEXT(GL_FRAMEBUFFER, base::size(attachments),
                             attachments);
}

void GLRenderer::PrepareSurfaceForPass(
    SurfaceInitializationMode initialization_mode,
    const gfx::Rect& render_pass_scissor) {
  SetViewport();

  switch (initialization_mode) {
    case SURFACE_INITIALIZATION_MODE_PRESERVE:
      EnsureScissorTestDisabled();
      return;
    case SURFACE_INITIALIZATION_MODE_FULL_SURFACE_CLEAR:
      EnsureScissorTestDisabled();
      DiscardPixels();
      ClearFramebuffer();
      break;
    case SURFACE_INITIALIZATION_MODE_SCISSORED_CLEAR:
      SetScissorTestRect(render_pass_scissor);
      ClearFramebuffer();
      break;
  }

  if (OverdrawTracingEnabled()) {
    gl_->GenQueriesEXT(1, &occlusion_query_);
    gl_->BeginQueryEXT(GL_SAMPLES_PASSED_ARB, occlusion_query_);
  }

  // For each render pass, reset the drawn region.
  drawn_rects_.clear();
}

void GLRenderer::ClearFramebuffer() {
  // On DEBUG builds, opaque render passes are cleared to blue to easily see
  // regions that were not drawn on the screen.
  if (current_frame()->current_render_pass->has_transparent_background)
    gl_->ClearColor(0, 0, 0, 0);
  else
    gl_->ClearColor(0, 0, 1, 1);

  gl_->ClearStencil(0);

  bool always_clear = overdraw_feedback_;
#ifndef NDEBUG
  always_clear = true;
#endif
  if (always_clear ||
      current_frame()->current_render_pass->has_transparent_background) {
    GLbitfield clear_bits = GL_COLOR_BUFFER_BIT;
    if (always_clear)
      clear_bits |= GL_STENCIL_BUFFER_BIT;
    gl_->Clear(clear_bits);
  }
}

void GLRenderer::BeginDrawingFrame() {
  TRACE_EVENT0("viz", "GLRenderer::BeginDrawingFrame");

  if (!context_busy_) {
    context_busy_ = output_surface_->context_provider()
                        ->CacheController()
                        ->ClientBecameBusy();
  }

  // Begin batching read of shared images.
  gl_->BeginBatchReadAccessSharedImageCHROMIUM();

  scoped_refptr<ResourceFence> read_lock_fence;
  if (use_sync_query_) {
    read_lock_fence = sync_queries_.StartNewFrame();
  } else {
    read_lock_fence =
        base::MakeRefCounted<DisplayResourceProviderGL::SynchronousFence>(gl_);
  }
  resource_provider()->SetReadLockFence(read_lock_fence.get());

  // Insert WaitSyncTokenCHROMIUM on quad resources prior to drawing the frame,
  // so that drawing can proceed without GL context switching interruptions.
  for (const auto& pass : *current_frame()->render_passes_in_draw_order) {
    for (auto* quad : pass->quad_list) {
      for (ResourceId resource_id : quad->resources)
        resource_provider()->WaitSyncToken(resource_id);
    }
  }

  // TODO(enne): Do we need to reinitialize all of this state per frame?
  ReinitializeGLState();

  // Add a dummy timer query as a fence to identify the beginning of a frame in
  // the circular queue.
  if (CompositeTimeTracingEnabled())
    timer_queries_.emplace(kTimerQueryDummy, "");

  num_triangles_drawn_ = 0;
}

void GLRenderer::DoDrawQuad(const DrawQuad* quad,
                            const gfx::QuadF* clip_region) {
  DCHECK(quad->rect.Contains(quad->visible_rect));
  if (quad->material != DrawQuad::Material::kTextureContent) {
    FlushTextureQuadCache(SHARED_BINDING);
  }

  switch (quad->material) {
    case DrawQuad::Material::kInvalid:
      NOTREACHED();
      break;
    case DrawQuad::Material::kAggregatedRenderPass:
      DrawRenderPassQuad(AggregatedRenderPassDrawQuad::MaterialCast(quad),
                         clip_region);
      break;
    case DrawQuad::Material::kDebugBorder:
      DrawDebugBorderQuad(DebugBorderDrawQuad::MaterialCast(quad));
      break;
    case DrawQuad::Material::kPictureContent:
      // PictureDrawQuad should only be used for resourceless software draws.
      NOTREACHED();
      break;
    case DrawQuad::Material::kCompositorRenderPass:
      // At this point, RenderPassDrawQuads should be replaced by
      // AggregatedRenderPassDrawQuad.
      NOTREACHED();
      break;
    case DrawQuad::Material::kSolidColor:
      DrawSolidColorQuad(SolidColorDrawQuad::MaterialCast(quad), clip_region);
      break;
    case DrawQuad::Material::kStreamVideoContent:
      DrawStreamVideoQuad(StreamVideoDrawQuad::MaterialCast(quad), clip_region);
      break;
    case DrawQuad::Material::kSurfaceContent:
      // Surface content should be fully resolved to other quad types before
      // reaching a direct renderer.
      NOTREACHED();
      break;
    case DrawQuad::Material::kTextureContent:
      EnqueueTextureQuad(TextureDrawQuad::MaterialCast(quad), clip_region);
      break;
    case DrawQuad::Material::kTiledContent:
      DrawTileQuad(TileDrawQuad::MaterialCast(quad), clip_region);
      break;
    case DrawQuad::Material::kYuvVideoContent:
      DrawYUVVideoQuad(YUVVideoDrawQuad::MaterialCast(quad), clip_region);
      break;
    case DrawQuad::Material::kVideoHole:
      // VideoHoleDrawQuad should only be used by Cast, and should
      // have been replaced by cast-specific OverlayProcessor before
      // reach here. In non-cast build, an untrusted render could send such
      // Quad and the quad would then reach here unexpectedly. Therefore
      // we should skip NOTREACHED() so an untrusted render is not capable
      // of causing a crash.
      break;
  }
}

// This function does not handle 3D sorting right now, since the debug border
// quads are just drawn as their original quads and not in split pieces. This
// results in some debug border quads drawing over foreground quads.
void GLRenderer::DrawDebugBorderQuad(const DebugBorderDrawQuad* quad) {
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  SetUseProgram(ProgramKey::DebugBorder(), gfx::ColorSpace::CreateSRGB(),
                CurrentRenderPassColorSpace());

  // Use the full quad_rect for debug quads to not move the edges based on
  // partial swaps.
  gfx::Rect layer_rect = quad->rect;
  gfx::Transform render_matrix;
  QuadRectTransform(&render_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(layer_rect));
  SetShaderMatrix(current_frame()->projection_matrix * render_matrix);
  SetShaderColor(quad->color, 1.f);

  gl_->LineWidth(quad->width);

  // The indices for the line are stored in the same array as the triangle
  // indices.
  gl_->DrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, nullptr);
}

// This is a utility to convert from GrGLenum color format into the equivalent
// skColorType format. Note: this only supports the limited set of values that
// can get returned by GLRenderer::GetBackdropTexture().
static SkColorType GlFormatToSkFormat(GrGLenum format) {
  switch (format) {
    case GL_RGB:
      return kRGB_888x_SkColorType;
    case GL_RGBA:
      return kRGBA_8888_SkColorType;
    case GL_BGRA_EXT:
      return kBGRA_8888_SkColorType;
    case GL_RGB10_A2_EXT:
      return kRGBA_1010102_SkColorType;
    default:
      NOTREACHED() << std::hex << std::showbase << format;
      return kN32_SkColorType;
  }
}

static GrGLenum SkFormatToGlFormat(SkColorType format) {
  switch (format) {
    case kRGB_888x_SkColorType:
      return GL_RGB8_OES;
    case kRGBA_8888_SkColorType:
      return GL_RGBA8_OES;
    case kBGRA_8888_SkColorType:
      return GL_BGRA8_EXT;
    case kRGBA_1010102_SkColorType:
      return GL_RGB10_A2_EXT;
    default:
      NOTREACHED();
      return GL_RGBA8_OES;
  }
}

// Wrap a given texture in a Ganesh backend texture.
static sk_sp<SkImage> WrapTexture(uint32_t texture_id,
                                  uint32_t target,
                                  const gfx::Size& size,
                                  GrDirectContext* context,
                                  bool flip_texture,
                                  SkColorType format,
                                  bool adopt_texture) {
  GrGLenum texture_format = SkFormatToGlFormat(format);
  GrGLTextureInfo texture_info;
  texture_info.fTarget = target;
  texture_info.fID = texture_id;
  texture_info.fFormat = texture_format;
  GrBackendTexture backend_texture(size.width(), size.height(),
                                   GrMipMapped::kNo, texture_info);
  GrSurfaceOrigin origin =
      flip_texture ? kBottomLeft_GrSurfaceOrigin : kTopLeft_GrSurfaceOrigin;
  if (adopt_texture) {
    return SkImage::MakeFromAdoptedTexture(
        context, backend_texture, origin, format, kPremul_SkAlphaType, nullptr);
  } else {
    return SkImage::MakeFromTexture(context, backend_texture, origin, format,
                                    kPremul_SkAlphaType, nullptr);
  }
}

static gfx::RectF CenteredRect(const gfx::Rect& tile_rect) {
  return gfx::RectF(
      gfx::PointF(-0.5f * tile_rect.width(), -0.5f * tile_rect.height()),
      gfx::SizeF(tile_rect.size()));
}

bool GLRenderer::CanApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode) {
  return use_blend_equation_advanced_ || blend_mode == SkBlendMode::kSrcOver ||
         blend_mode == SkBlendMode::kDstIn ||
         blend_mode == SkBlendMode::kScreen;
}

void GLRenderer::ApplyBlendModeUsingBlendFunc(SkBlendMode blend_mode) {
  // Any modes set here must be reset in RestoreBlendFuncToDefault
  if (blend_mode == SkBlendMode::kSrcOver) {
    // Left no-op intentionally.
  } else if (blend_mode == SkBlendMode::kDstIn) {
    gl_->BlendFunc(GL_ZERO, GL_SRC_ALPHA);
  } else if (blend_mode == SkBlendMode::kDstOut) {
    gl_->BlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
  } else if (blend_mode == SkBlendMode::kScreen) {
    gl_->BlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);
  } else {
    DCHECK(use_blend_equation_advanced_);
    GLenum equation = GL_FUNC_ADD;
    switch (blend_mode) {
      case SkBlendMode::kScreen:
        equation = GL_SCREEN_KHR;
        break;
      case SkBlendMode::kOverlay:
        equation = GL_OVERLAY_KHR;
        break;
      case SkBlendMode::kDarken:
        equation = GL_DARKEN_KHR;
        break;
      case SkBlendMode::kLighten:
        equation = GL_LIGHTEN_KHR;
        break;
      case SkBlendMode::kColorDodge:
        equation = GL_COLORDODGE_KHR;
        break;
      case SkBlendMode::kColorBurn:
        equation = GL_COLORBURN_KHR;
        break;
      case SkBlendMode::kHardLight:
        equation = GL_HARDLIGHT_KHR;
        break;
      case SkBlendMode::kSoftLight:
        equation = GL_SOFTLIGHT_KHR;
        break;
      case SkBlendMode::kDifference:
        equation = GL_DIFFERENCE_KHR;
        break;
      case SkBlendMode::kExclusion:
        equation = GL_EXCLUSION_KHR;
        break;
      case SkBlendMode::kMultiply:
        equation = GL_MULTIPLY_KHR;
        break;
      case SkBlendMode::kHue:
        equation = GL_HSL_HUE_KHR;
        break;
      case SkBlendMode::kSaturation:
        equation = GL_HSL_SATURATION_KHR;
        break;
      case SkBlendMode::kColor:
        equation = GL_HSL_COLOR_KHR;
        break;
      case SkBlendMode::kLuminosity:
        equation = GL_HSL_LUMINOSITY_KHR;
        break;
      default:
        NOTREACHED() << "Unexpected blend mode: SkBlendMode::k"
                     << SkBlendMode_Name(blend_mode);
        return;
    }
    gl_->BlendEquation(equation);
  }
}

void GLRenderer::RestoreBlendFuncToDefault(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
      break;
    case SkBlendMode::kDstIn:
    case SkBlendMode::kDstOut:
    case SkBlendMode::kScreen:
      gl_->BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
      break;
    default:
      DCHECK(use_blend_equation_advanced_);
      gl_->BlendEquation(GL_FUNC_ADD);
  }
}

bool GLRenderer::ShouldApplyBackdropFilters(
    const DrawRenderPassDrawQuadParams* params) {
  if (!params->backdrop_filters)
    return false;
  if (params->quad->shared_quad_state->opacity == 0.f)
    return false;
  DCHECK(!params->backdrop_filters->IsEmpty());
  return true;
}

gfx::Rect GLRenderer::GetBackdropBoundingBoxForRenderPassQuad(
    DrawRenderPassDrawQuadParams* params,
    gfx::Transform* backdrop_filter_bounds_transform,
    base::Optional<gfx::RRectF>* backdrop_filter_bounds,
    gfx::Rect* unclipped_rect) const {
  DCHECK(backdrop_filter_bounds_transform);
  DCHECK(backdrop_filter_bounds);
  DCHECK(unclipped_rect);

  const auto* quad = params->quad;
  gfx::QuadF scaled_region;
  // |scaled_region| is a quad in [-0.5,0.5] space that represents |clip_region|
  // as a fraction of the space defined by |quad->rect|. If |clip_region| is
  // nullptr, then scaled_region is [-0.5,0.5].
  if (!GetScaledRegion(quad->rect, params->clip_region, &scaled_region)) {
    scaled_region = SharedGeometryQuad().BoundingBox();
  }
  // |backdrop_filter_bounds| is a rounded rect in [-0.5,0.5] space that
  // represents |params->backdrop_filter_bounds| as a fraction of the space
  // defined by |quad->rect|, not including its offset.
  *backdrop_filter_bounds = gfx::RRectF();
  if (!params->backdrop_filter_bounds ||
      !GetScaledRRectF(quad->rect, params->backdrop_filter_bounds.value(),
                       &backdrop_filter_bounds->value())) {
    backdrop_filter_bounds->reset();
  }

  // |backdrop_rect| is now the bounding box of clip_region, in window pixel
  // coordinates, and with flip applied.
  gfx::Rect backdrop_rect = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
      params->contents_device_transform, scaled_region.BoundingBox()));

  if (!backdrop_rect.IsEmpty() && (params->filters || params->use_aa)) {
    // If we have regular filters or antialiasing, grab an extra one-pixel
    // border around the background, so texture edge clamping gives us a
    // transparent border.
    backdrop_rect.Inset(-1, -1, -1, -1);
  }

  *unclipped_rect = backdrop_rect;
  backdrop_rect.Intersect(MoveFromDrawToWindowSpace(
      current_frame()->current_render_pass->output_rect));
  if (ShouldApplyBackdropFilters(params)) {
    float max_pixel_movement = params->backdrop_filters->MaximumPixelMovement();
    gfx::Rect scissor_rect(current_window_space_viewport_);
    scissor_rect.Inset(-max_pixel_movement, -max_pixel_movement);
    backdrop_rect.Intersect(scissor_rect);
  }

  // The frame buffer flip is already included in the captured backdrop image,
  // and it is included in |contents_device_transform| (through
  // |projection_matrix|). Don't double-flip.
  *backdrop_filter_bounds_transform = params->contents_device_transform;
  float new_y = 2 * backdrop_filter_bounds_transform->To2dTranslation().y() +
                backdrop_rect.bottom() - unclipped_rect->bottom() +
                backdrop_rect.y() - unclipped_rect->y();
  backdrop_filter_bounds_transform->PostScale(1, -1);
  backdrop_filter_bounds_transform->PostTranslate(0, new_y);

  // Shift to the space of the captured backdrop image.
  backdrop_filter_bounds_transform->PostTranslate(-backdrop_rect.x(),
                                                  -backdrop_rect.y());

  return backdrop_rect;
}

GLenum GLRenderer::GetFramebufferCopyTextureFormat() {
  // If copying a non-root renderpass then use the format of the bound
  // texture. Otherwise, we use the format of the default framebuffer. But
  // whatever the format is, convert it to a valid format for CopyTexSubImage2D.
  GLenum format;
  if (!current_framebuffer_texture_) {
    format = output_surface_->GetFramebufferCopyTextureFormat();
  } else {
    ResourceFormat resource_format = CurrentRenderPassResourceFormat();
    DCHECK(GLSupportsFormat(resource_format));
    format = GLCopyTextureInternalFormat(resource_format);
  }
  // Verify the format is valid for GLES2's glCopyTexSubImage2D.
  DCHECK(format == GL_ALPHA || format == GL_LUMINANCE ||
         format == GL_LUMINANCE_ALPHA || format == GL_RGB ||
         format == GL_RGBA ||
         (output_surface_->context_provider()
              ->ContextCapabilities()
              .texture_format_bgra8888 &&
          format == GL_BGRA_EXT) ||
         format == GL_RGB10_A2_EXT)
      << std::hex << std::showbase << format;
  return format;
}

uint32_t GLRenderer::GetBackdropTexture(const gfx::Rect& window_rect,
                                        float scale,
                                        GLenum* internal_format) {
  DCHECK(internal_format);
  DCHECK_GE(window_rect.x(), 0);
  DCHECK_GE(window_rect.y(), 0);
  DCHECK_LE(window_rect.right(), current_surface_size_.width());
  DCHECK_LE(window_rect.bottom(), current_surface_size_.height());

  uint32_t texture_id;
  gl_->GenTextures(1, &texture_id);
  DCHECK(texture_id);
  gl_->BindTexture(GL_TEXTURE_2D, texture_id);

  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  ResourceFormat resource_format = CurrentRenderPassResourceFormat();
  // Get gl_format, gl_type and internal_format.
  DCHECK(GLSupportsFormat(resource_format));
  *internal_format = GLInternalFormat(resource_format);
  GLenum gl_format = GLDataFormat(resource_format);
  GLenum gl_type = GLDataType(resource_format);

  if (scale != 1.0f) {
    DCHECK(!prefer_draw_to_copy_ || !current_framebuffer_texture_);

    gfx::Size target_size = window_rect.size();
    target_size = gfx::ScaleToCeiledSize(target_size, scale);

    gl_->TexImage2D(GL_TEXTURE_2D, 0, *internal_format, target_size.width(),
                    target_size.height(), 0, gl_format, gl_type, nullptr);

    unsigned fbo = 0;
    gl_->GenFramebuffers(1, &fbo);
    gl_->BindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    gl_->FramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0,
                              GL_TEXTURE_2D, texture_id, 0);
    DCHECK_EQ(static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE),
              gl_->CheckFramebufferStatus(GL_DRAW_FRAMEBUFFER_EXT));

    gl_->Scissor(0, 0, target_size.width(), target_size.height());

    gl_->BlitFramebufferCHROMIUM(window_rect.x(), window_rect.y(),
                                 window_rect.right(), window_rect.bottom(), 0,
                                 0, target_size.width(), target_size.height(),
                                 GL_COLOR_BUFFER_BIT, GL_LINEAR);

    gl_->DeleteFramebuffers(1, &fbo);
  } else if (prefer_draw_to_copy_ && current_framebuffer_texture_) {
    // If there is a source texture |current_framebuffer_texture_| and the
    // workaround |prefer_draw_to_copy_| is enabled, then do texture to texture
    // copy via draw instead of glCopyTexImage2D.

    // Size the destination texture with empty data. This is required since
    // CopySubTextureCHROMIUM() does not sizes the texture but CopyTexImage2D
    // does.
    gl_->TexImage2D(GL_TEXTURE_2D, 0, *internal_format, window_rect.width(),
                    window_rect.height(), 0, gl_format, gl_type, nullptr);
    gl_->CopySubTextureCHROMIUM(
        current_framebuffer_texture_->id(), 0, GL_TEXTURE_2D, texture_id, 0, 0,
        0, window_rect.x(), window_rect.y(), window_rect.width(),
        window_rect.height(), GL_FALSE, GL_FALSE, GL_FALSE);
  } else {
    *internal_format = GetFramebufferCopyTextureFormat();

    // CopyTexImage2D requires inernalformat channels to be a subset of
    // the channels of the source texture internalformat.
    DCHECK(*internal_format == GL_RGB || *internal_format == GL_RGBA ||
           *internal_format == GL_BGRA_EXT ||
           *internal_format == GL_RGB10_A2_EXT);
    if (*internal_format == GL_BGRA_EXT)
      *internal_format = GL_RGBA;
    gl_->CopyTexImage2D(GL_TEXTURE_2D, 0, *internal_format, window_rect.x(),
                        window_rect.y(), window_rect.width(),
                        window_rect.height(), 0);
  }
  gl_->BindTexture(GL_TEXTURE_2D, 0);
  return texture_id;
}

static sk_sp<SkImage> FinalizeImage(sk_sp<SkSurface> surface) {
  // Flush the drawing before source texture read lock goes out of scope.
  // Skia API does not guarantee that when the SkImage goes out of scope,
  // its externally referenced resources would force the rendering to be
  // flushed.
  surface->getCanvas()->flush();
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  if (!image || !image->isTextureBacked()) {
    return nullptr;
  }
  return image;
}

sk_sp<SkImage> GLRenderer::ApplyBackdropFilters(
    DrawRenderPassDrawQuadParams* params,
    const gfx::Rect& unclipped_rect,
    const base::Optional<gfx::RRectF>& backdrop_filter_bounds,
    const gfx::Transform& backdrop_filter_bounds_transform) {
  DCHECK(ShouldApplyBackdropFilters(params));
  DCHECK(params->backdrop_filter_quality > 0.0f &&
         params->backdrop_filter_quality <= 1.0f);
  DCHECK(!params->filters)
      << "Filters should always be in a separate Effect node";
  const auto* quad = params->quad;
  auto use_gr_context = ScopedUseGrContext::Create(this);

  // Check if cached result can be used
  auto bg_texture_it =
      render_pass_backdrop_textures_.find(quad->render_pass_id);
  if (bg_texture_it != render_pass_backdrop_textures_.end()) {
    if (!quad->intersects_damage_under)
      return bg_texture_it->second;
    else
      render_pass_backdrop_textures_.erase(bg_texture_it);
  }

  gfx::Vector2d clipping_offset =
      (params->background_rect.top_right() - unclipped_rect.top_right()) +
      (params->background_rect.bottom_left() - unclipped_rect.bottom_left());

  gfx::Rect quality_adjusted_rect = ScaleToEnclosingRect(
      params->background_rect, params->backdrop_filter_quality);

  // When backdrop_filter_quality is less than 1.0f, scale the blur amount
  // accordingly.
  cc::FilterOperations filter_operations;
  if (params->backdrop_filter_quality < 1.0f) {
    for (const cc::FilterOperation& op :
         params->backdrop_filters->operations()) {
      if (op.type() == cc::FilterOperation::BLUR) {
        cc::FilterOperation blur_op(op);
        blur_op.set_amount(op.amount() * params->backdrop_filter_quality);
        filter_operations.Append(blur_op);
      } else {
        filter_operations.Append(op);
      }
    }
  }
  const cc::FilterOperations& filters = params->backdrop_filter_quality < 1.0f
                                            ? filter_operations
                                            : *params->backdrop_filters;

  auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
      filters, gfx::SizeF(quality_adjusted_rect.size()),
      gfx::Vector2dF(clipping_offset));

  // TODO(senorblanco): background filters should be moved to the
  // makeWithFilter fast-path, and go back to calling ApplyImageFilter().
  // See http://crbug.com/613233.
  if (!paint_filter || !use_gr_context)
    return nullptr;

  auto filter = paint_filter->cached_sk_filter_;
  sk_sp<SkImage> src_image = WrapTexture(
      params->background_texture, GL_TEXTURE_2D, quality_adjusted_rect.size(),
      use_gr_context->context(), /*flip_texture=*/true,
      GlFormatToSkFormat(params->background_texture_format),
      /*adopt_texture=*/false);
  if (!src_image) {
    TRACE_EVENT_INSTANT0("cc",
                         "ApplyBackdropFilters wrap background texture failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return nullptr;
  }

  // Create surface to draw into.
  SkImageInfo dst_info = SkImageInfo::MakeN32Premul(
      quality_adjusted_rect.width(), quality_adjusted_rect.height());
  sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(
      use_gr_context->context(), SkBudgeted::kYes, dst_info);
  if (!surface) {
    TRACE_EVENT_INSTANT0("viz",
                         "ApplyBackdropFilters surface allocation failed",
                         TRACE_EVENT_SCOPE_THREAD);
    return nullptr;
  }

  // Big filters can sometimes fallback to CPU. Therefore, we need
  // to disable subnormal floats for performance and security reasons.
  cc::ScopedSubnormalFloatDisabler disabler;

  gfx::RectF src_image_rect =
      gfx::RectF(quality_adjusted_rect.width(), quality_adjusted_rect.height());
  SkRect dest_rect = RectToSkRect(gfx::Rect(quality_adjusted_rect.size()));

  // If the content underneath the backdrop filter can be exposed because of
  // blending or bounds, paint the backdrop at full opacity first. The
  // backdrop-filtered content will not be blended with the backdrop later, it
  // will be rastered over the top. So we need to paint it here, unfiltered.
  if (backdrop_filter_bounds.has_value() || quad->ShouldDrawWithBlending()) {
    surface->getCanvas()->drawImageRect(
        src_image, RectFToSkRect(src_image_rect), dest_rect,
        SkSamplingOptions(), nullptr, SkCanvas::kStrict_SrcRectConstraint);
  }

  if (backdrop_filter_bounds.has_value()) {
    // Crop the source image to the backdrop_filter_bounds.
    gfx::Rect filter_clip = gfx::ToEnclosingRect(cc::MathUtil::MapClippedRect(
        backdrop_filter_bounds_transform, backdrop_filter_bounds->rect()));
    gfx::Rect src_rect(params->background_rect.width(),
                       params->background_rect.height());
    filter_clip.Intersect(src_rect);
    if (filter_clip.IsEmpty())
      return FinalizeImage(surface);
    if (filter_clip != src_rect) {
      filter_clip = gfx::ScaleToEnclosingRect(filter_clip,
                                              params->backdrop_filter_quality);
      src_image = src_image->makeSubset(RectToSkIRect(filter_clip),
                                        use_gr_context->context());
      src_image_rect = gfx::RectF(filter_clip.width(), filter_clip.height());
      dest_rect = RectToSkRect(filter_clip);
    }
  }

  SkIPoint offset;
  SkIRect subset;
  sk_sp<SkImage> filtered_image = SkiaHelper::ApplyImageFilter(
      use_gr_context->context(), src_image, src_image_rect, src_image_rect,
      quad->filters_scale, std::move(filter), &offset, &subset,
      quad->filters_origin, true);

  // Clip the filtered image to the (rounded) bounding box of the element.
  if (backdrop_filter_bounds.has_value()) {
    surface->getCanvas()->save();
    gfx::RRectF clip_rect(backdrop_filter_bounds.value());
    surface->getCanvas()->setMatrix(
        SkMatrix(backdrop_filter_bounds_transform.matrix()));
    surface->getCanvas()->clipRRect(SkRRect(clip_rect), SkClipOp::kIntersect,
                                    true /* antialias */);
    surface->getCanvas()->resetMatrix();
  }

  SkPaint paint;
  // Paint the filtered backdrop image with opacity.
  if (quad->shared_quad_state->opacity < 1.0) {
    paint.setImageFilter(
        SkiaHelper::BuildOpacityFilter(quad->shared_quad_state->opacity));
  }
  // Now paint the pre-filtered image onto the canvas (possibly with mask
  // applied).
  surface->getCanvas()->drawImageRect(filtered_image, SkRect::Make(subset),
                                      dest_rect, SkSamplingOptions(), &paint,
                                      SkCanvas::kStrict_SrcRectConstraint);

  if (backdrop_filter_bounds.has_value()) {
    surface->getCanvas()->restore();
  }

  sk_sp<SkImage> filtered_image_texture = FinalizeImage(surface);
  if (!quad->intersects_damage_under) {
    render_pass_backdrop_textures_[params->quad->render_pass_id] =
        filtered_image_texture;
  }
  return filtered_image_texture;
}

const DrawQuad* GLRenderer::CanPassBeDrawnDirectly(
    const AggregatedRenderPass* pass) {
#if defined(OS_APPLE)
  // On Macs, this path can sometimes lead to all black output.
  // TODO(enne): investigate this and remove this hack.
  return nullptr;
#else
  // Can only collapse a single tile quad.
  if (pass->quad_list.size() != 1)
    return nullptr;

  const DrawQuad* quad = *pass->quad_list.BackToFrontBegin();
  // Hack: this could be supported by concatenating transforms, but
  // in practice if there is one quad, it is at the origin of the render pass
  // and has the same size as the pass.
  if (!quad->shared_quad_state->quad_to_target_transform.IsIdentity() ||
      quad->rect != pass->output_rect)
    return nullptr;
  // The quad is expected to be the entire layer so that AA edges are correct.
  if (quad->shared_quad_state->quad_layer_rect != quad->rect)
    return nullptr;
  if (quad->material != DrawQuad::Material::kTiledContent)
    return nullptr;

  // TODO(chrishtr): support could be added for opacity, but care needs
  // to be taken to make sure it is correct w.r.t. non-commutative filters etc.
  if (quad->shared_quad_state->opacity != 1.0f)
    return nullptr;

  if (quad->shared_quad_state->blend_mode != SkBlendMode::kSrcOver)
    return nullptr;

  const TileDrawQuad* tile_quad = TileDrawQuad::MaterialCast(quad);
  // Hack: this could be supported by passing in a subrectangle to draw
  // render pass, although in practice if there is only one quad there
  // will be no border texels on the input.
  if (tile_quad->tex_coord_rect != gfx::RectF(tile_quad->rect))
    return nullptr;
  // Tile quad features not supported in render pass shaders.
  if (tile_quad->nearest_neighbor)
    return nullptr;
  // BUG=skia:3868, Skia currently doesn't support texture rectangle inputs.
  // See also the DCHECKs about GL_TEXTURE_2D in DrawRenderPassQuad.
  GLenum target =
      resource_provider()->GetResourceTextureTarget(tile_quad->resource_id());
  if (target != GL_TEXTURE_2D)
    return nullptr;

  return tile_quad;
#endif
}

void GLRenderer::DrawRenderPassQuad(const AggregatedRenderPassDrawQuad* quad,
                                    const gfx::QuadF* clip_region) {
  auto bypass = render_pass_bypass_quads_.find(quad->render_pass_id);
  DrawRenderPassDrawQuadParams params;
  params.quad = quad;
  params.clip_region = clip_region;
  params.window_matrix = current_frame()->window_matrix;
  params.projection_matrix = current_frame()->projection_matrix;
  params.tex_coord_rect = quad->tex_coord_rect;
  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_, "kRenderPassDrawQuad");
  if (bypass != render_pass_bypass_quads_.end()) {
    DCHECK(bypass->second->material == DrawQuad::Material::kTiledContent);
    const TileDrawQuad* tile_quad = TileDrawQuad::MaterialCast(bypass->second);
    // The projection matrix used by GLRenderer has a flip.  As tile texture
    // inputs are oriented opposite to framebuffer outputs, don't flip via
    // texture coords and let the projection matrix naturallyd o it.
    params.flip_texture = false;
    params.bypass_quad_texture.resource_id = tile_quad->resource_id();
    params.bypass_quad_texture.size = tile_quad->texture_size;
    DrawRenderPassQuadInternal(&params);
  } else {
    auto contents_texture_it = render_pass_textures_.find(quad->render_pass_id);
    DCHECK(contents_texture_it->second.id());
    // See above comments about texture flipping.  When the input is a
    // render pass, it needs to an extra flip to be oriented correctly.
    params.flip_texture = true;
    params.contents_texture = &contents_texture_it->second;
    DrawRenderPassQuadInternal(&params);
  }

  if (params.background_texture) {
    gl_->DeleteTextures(1, &params.background_texture);
    params.background_texture = 0;
  }
}

void GLRenderer::DrawRenderPassQuadInternal(
    DrawRenderPassDrawQuadParams* params) {
  params->quad_to_target_transform =
      params->quad->shared_quad_state->quad_to_target_transform;
  if (!InitializeRPDQParameters(params))
    return;

  UpdateRPDQShadersForBlending(params);
  bool can_draw = UpdateRPDQWithSkiaFilters(params);
  // The above calls use ScopedUseGrContext which can change the bound
  // framebuffer, so we need to restore it for the current RenderPass.
  UseRenderPass(current_frame()->current_render_pass);
  // As part of restoring the framebuffer, we call SetViewport directly, rather
  // than through PrepareSurfaceForPass. PrepareSurfaceForPass also clears the
  // surface, which is not desired when restoring.
  SetViewport();

  if (!can_draw)
    return;

  UpdateRPDQTexturesForSampling(params);
  UpdateRPDQBlendMode(params);
  ChooseRPDQProgram(params, CurrentRenderPassColorSpace());
  UpdateRPDQUniforms(params);
  DrawRPDQ(*params);

  AccumulateDrawRects(params->quad->visible_rect,
                      params->quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);
}

bool GLRenderer::InitializeRPDQParameters(
    DrawRenderPassDrawQuadParams* params) {
  DCHECK(params);
  const auto* quad = params->quad;
  SkMatrix local_matrix;
  local_matrix.setTranslate(quad->filters_origin.x(), quad->filters_origin.y());
  local_matrix.postScale(quad->filters_scale.x(), quad->filters_scale.y());
  params->filters = FiltersForPass(quad->render_pass_id);
  params->backdrop_filters = BackdropFiltersForPass(quad->render_pass_id);
  if (ShouldApplyBackdropFilters(params)) {
    params->backdrop_filter_bounds =
        BackdropFilterBoundsForPass(quad->render_pass_id);
    if (params->backdrop_filter_bounds.has_value()) {
      params->backdrop_filter_bounds->Scale(quad->filters_scale.x(),
                                            quad->filters_scale.y());
    }
  } else {
    params->backdrop_filter_bounds.reset();
  }
  params->backdrop_filter_quality = quad->backdrop_filter_quality;
  gfx::Rect dst_rect = params->filters
                           ? params->filters->MapRect(quad->rect, local_matrix)
                           : quad->rect;
  params->dst_rect.SetRect(static_cast<float>(dst_rect.x()),
                           static_cast<float>(dst_rect.y()),
                           static_cast<float>(dst_rect.width()),
                           static_cast<float>(dst_rect.height()));
  gfx::Transform quad_rect_matrix;
  gfx::Rect quad_layer_rect(quad->shared_quad_state->quad_layer_rect);
  if (params->filters)
    quad_layer_rect = params->filters->MapRect(quad_layer_rect, local_matrix);
  QuadRectTransform(&quad_rect_matrix, params->quad_to_target_transform,
                    gfx::RectF(quad_layer_rect));
  params->contents_device_transform =
      params->window_matrix * params->projection_matrix * quad_rect_matrix;
  params->contents_device_transform.FlattenTo2d();

  // Can only draw surface if device matrix is invertible.
  if (!params->contents_device_transform.IsInvertible())
    return false;

  // TODO(sunxd): unify the anti-aliasing logic of RPDQ and TileDrawQuad.
  params->surface_quad = SharedGeometryQuad();
  gfx::QuadF device_layer_quad;
  if (settings_->allow_antialiasing && !quad->force_anti_aliasing_off &&
      quad->IsEdge()) {
    bool clipped = false;
    device_layer_quad = cc::MathUtil::MapQuad(params->contents_device_transform,
                                              params->surface_quad, &clipped);
    params->use_aa = ShouldAntialiasQuad(device_layer_quad, clipped,
                                         settings_->force_antialiasing);
  }

  const gfx::QuadF* aa_quad = params->use_aa ? &device_layer_quad : nullptr;
  SetupRenderPassQuadForClippingAndAntialiasing(
      params->contents_device_transform, quad, aa_quad, params->clip_region,
      &params->surface_quad, params->edge);

  return true;
}

// Get a GL texture id from an SkImage. An optional origin pointer can be
// passed in which will be filled out with the origin for the texture
// backing the SkImage.
static GLuint GetGLTextureIDFromSkImage(const SkImage* image,
                                        GrSurfaceOrigin* origin = nullptr) {
  GrBackendTexture backend_texture = image->getBackendTexture(true, origin);
  if (!backend_texture.isValid()) {
    return 0;
  }
  GrGLTextureInfo info;
  bool result = backend_texture.getGLTextureInfo(&info);
  DCHECK(result);
  return info.fID;
}

void GLRenderer::UpdateRPDQShadersForBlending(
    DrawRenderPassDrawQuadParams* params) {
  const auto* quad = params->quad;
  params->blend_mode = quad->shared_quad_state->blend_mode;
  params->use_shaders_for_blending =
      !CanApplyBlendModeUsingBlendFunc(params->blend_mode) ||
      ShouldApplyBackdropFilters(params) ||
      settings_->force_blending_with_shaders;

  if (params->use_shaders_for_blending) {
    // Compute a bounding box around the pixels that will be visible through
    // the quad.
    base::Optional<gfx::RRectF> backdrop_filter_bounds;
    gfx::Transform backdrop_filter_bounds_transform;
    gfx::Rect unclipped_rect;
    params->background_rect = GetBackdropBoundingBoxForRenderPassQuad(
        params, &backdrop_filter_bounds_transform, &backdrop_filter_bounds,
        &unclipped_rect);

    if (!params->background_rect.IsEmpty()) {
      // The pixels from the filtered background should completely replace the
      // current pixel values.
      if (blend_enabled())
        SetBlendEnabled(false);

      // Read the pixels in the bounding box into a buffer R.
      // This function allocates a texture, which should contribute to the
      // amount of memory used by render surfaces:
      // LayerTreeHost::CalculateMemoryForRenderSurfaces.
      const auto& operations = params->backdrop_filters->operations();
      DCHECK(params->backdrop_filter_quality == 1.0f ||
             (operations.size() == 1 &&
              operations.front().type() == cc::FilterOperation::BLUR));
      params->background_texture = GetBackdropTexture(
          params->background_rect, params->backdrop_filter_quality,
          &params->background_texture_format);

      if (ShouldApplyBackdropFilters(params)) {
        // Apply the background filters to R, so that it is applied in the
        // pixels' coordinate space.
        params->background_image =
            ApplyBackdropFilters(params, unclipped_rect, backdrop_filter_bounds,
                                 backdrop_filter_bounds_transform);
        if (params->background_image) {
          params->background_image_id =
              GetGLTextureIDFromSkImage(params->background_image.get());
          DCHECK(params->background_image_id || IsContextLost());
        }
      }
      if (params->background_image_id) {
        // Reset original background texture if there is not any mask.
        if (!quad->mask_resource_id()) {
          gl_->DeleteTextures(1, &params->background_texture);
          params->background_texture = 0;
        }
      } else if (CanApplyBlendModeUsingBlendFunc(params->blend_mode) &&
                 ShouldApplyBackdropFilters(params)) {
        // Something went wrong with applying backdrop filters to the
        // backdrop.
        params->use_shaders_for_blending = false;
        gl_->DeleteTextures(1, &params->background_texture);
        params->background_texture = 0;
      }
    } else {  // params->background_rect.IsEmpty()
      DCHECK(!params->background_image_id);
      params->use_shaders_for_blending = false;
      params->blend_mode = SkBlendMode::kSrcOver;
    }
  }

  // Need original background texture for mask?
  params->mask_for_background =
      params->background_texture &&  // Have original background texture
      params->background_image_id;   // Have mask texture
  // If we have background texture + background image, then we also have mask
  // resource.
  if (params->background_texture && params->background_image_id) {
    DCHECK(params->mask_for_background);
    DCHECK(quad->mask_resource_id());
  }

  DCHECK_EQ(params->background_texture || params->background_image_id,
            params->use_shaders_for_blending);
}

bool GLRenderer::UpdateRPDQWithSkiaFilters(
    DrawRenderPassDrawQuadParams* params) {
  const auto* quad = params->quad;
  // Apply filters to the contents texture.
  if (params->filters) {
    DCHECK(!params->filters->IsEmpty());
    gfx::Size size = params->contents_texture
                         ? params->contents_texture->size()
                         : params->bypass_quad_texture.size;
    auto paint_filter = cc::RenderSurfaceFilters::BuildImageFilter(
        *params->filters, gfx::SizeF(size));
    auto filter = paint_filter ? paint_filter->cached_sk_filter_ : nullptr;
    if (filter) {
      SkColorFilter* colorfilter_rawptr = nullptr;
      filter->asColorFilter(&colorfilter_rawptr);
      sk_sp<SkColorFilter> cf(colorfilter_rawptr);

      if (cf && cf->asAColorMatrix(params->color_matrix)) {
        // We have a color matrix at the root of the filter DAG; apply it
        // locally in the compositor and process the rest of the DAG (if any)
        // in Skia.
        params->use_color_matrix = true;
        filter = sk_ref_sp(filter->getInput(0));
      }
      if (filter) {
        gfx::Rect clip_rect = quad->shared_quad_state->clip_rect;
        if (clip_rect.IsEmpty()) {
          clip_rect = current_draw_rect_;
        }
        gfx::Transform transform = params->quad_to_target_transform;
        if (!transform.IsInvertible()) {
          return false;
        }
        gfx::QuadF clip_quad = gfx::QuadF(gfx::RectF(clip_rect));
        gfx::QuadF local_clip =
            cc::MathUtil::InverseMapQuadToLocalSpace(transform, clip_quad);
        params->dst_rect.Intersect(local_clip.BoundingBox());
        // If we've been fully clipped out (by crop rect or clipping), there's
        // nothing to draw.
        if (params->dst_rect.IsEmpty()) {
          return false;
        }
        SkIPoint offset;
        SkIRect subset;
        gfx::RectF src_rect(quad->rect);
        auto use_gr_context = ScopedUseGrContext::Create(this);
        if (!use_gr_context)
          return false;

        if (params->contents_texture) {
          params->contents_and_bypass_color_space =
              params->contents_texture->color_space();
          sk_sp<SkImage> src_image = WrapTexture(
              params->contents_texture->id(), GL_TEXTURE_2D,
              params->contents_texture->size(), use_gr_context->context(),
              params->flip_texture, kN32_SkColorType, /*adopt_texture=*/false);
          params->filter_image = SkiaHelper::ApplyImageFilter(
              use_gr_context->context(), src_image, src_rect, params->dst_rect,
              quad->filters_scale, std::move(filter), &offset, &subset,
              quad->filters_origin, true);
        } else {
          DisplayResourceProviderGL::ScopedReadLockGL
              prefilter_bypass_quad_texture_lock(
                  resource_provider(), params->bypass_quad_texture.resource_id);
          params->contents_and_bypass_color_space =
              prefilter_bypass_quad_texture_lock.color_space();
          sk_sp<SkImage> src_image =
              WrapTexture(prefilter_bypass_quad_texture_lock.texture_id(),
                          prefilter_bypass_quad_texture_lock.target(),
                          prefilter_bypass_quad_texture_lock.size(),
                          use_gr_context->context(), params->flip_texture,
                          kN32_SkColorType, /*adopt_texture=*/false);
          params->filter_image = SkiaHelper::ApplyImageFilter(
              use_gr_context->context(), src_image, src_rect, params->dst_rect,
              quad->filters_scale, std::move(filter), &offset, &subset,
              quad->filters_origin, true);
        }

        if (!params->filter_image)
          return false;
        params->dst_rect =
            gfx::RectF(src_rect.x() + offset.fX, src_rect.y() + offset.fY,
                       subset.width(), subset.height());
        gfx::RectF tex_rect = gfx::RectF(gfx::PointF(subset.x(), subset.y()),
                                         params->dst_rect.size());
        params->tex_coord_rect = tex_rect;
      }
    }
  }
  return true;
}

void GLRenderer::UpdateRPDQTexturesForSampling(
    DrawRenderPassDrawQuadParams* params) {
  if (params->quad->mask_resource_id()) {
    params->mask_resource_lock =
        std::make_unique<DisplayResourceProviderGL::ScopedSamplerGL>(

            resource_provider(), params->quad->mask_resource_id(), GL_TEXTURE1,
            GL_LINEAR);
  }

  if (params->filter_image) {
    GrSurfaceOrigin origin;
    GLuint filter_image_id =
        GetGLTextureIDFromSkImage(params->filter_image.get(), &origin);
    DCHECK(filter_image_id || IsContextLost());
    DCHECK_EQ(GL_TEXTURE0, GetActiveTextureUnit(gl_));
    gl_->BindTexture(GL_TEXTURE_2D, filter_image_id);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // |params->contents_and_bypass_color_space| was populated when
    // |params->filter_image| was populated.
    params->source_needs_flip = kBottomLeft_GrSurfaceOrigin == origin;
  } else if (params->contents_texture) {
    params->contents_texture->BindForSampling();
    params->contents_and_bypass_color_space =
        params->contents_texture->color_space();
    params->source_needs_flip = params->flip_texture;
  } else {
    params->bypass_quad_resource_lock =
        std::make_unique<DisplayResourceProviderGL::ScopedSamplerGL>(
            resource_provider(), params->bypass_quad_texture.resource_id,
            GL_LINEAR);
    DCHECK_EQ(static_cast<GLenum>(GL_TEXTURE_2D),
              params->bypass_quad_resource_lock->target());
    params->contents_and_bypass_color_space =
        params->bypass_quad_resource_lock->color_space();
    params->source_needs_flip = params->flip_texture;
  }
}

void GLRenderer::UpdateRPDQBlendMode(DrawRenderPassDrawQuadParams* params) {
  SkBlendMode blend_mode = params->blend_mode;
  SetBlendEnabled((!params->use_shaders_for_blending &&
                   (params->quad->ShouldDrawWithBlending() ||
                    !IsDefaultBlendMode(blend_mode))) ||
                  ShouldApplyRoundedCorner(params->quad));
  if (!params->use_shaders_for_blending) {
    if (!use_blend_equation_advanced_coherent_ && use_blend_equation_advanced_)
      gl_->BlendBarrierKHR();

    ApplyBlendModeUsingBlendFunc(blend_mode);
  }
}

void GLRenderer::ChooseRPDQProgram(DrawRenderPassDrawQuadParams* params,
                                   const gfx::ColorSpace& target_color_space) {
  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      gl_, &highp_threshold_cache_, settings_->highp_threshold_min,
      params->quad->shared_quad_state->visible_quad_layer_rect.size());

  BlendMode shader_blend_mode =
      params->use_shaders_for_blending
          ? BlendModeFromSkXfermode(params->blend_mode)
          : BLEND_MODE_NONE;

  SamplerType sampler_type = SAMPLER_TYPE_2D;
  MaskMode mask_mode = NO_MASK;
  bool mask_for_background = params->mask_for_background;
  if (params->mask_resource_lock) {
    mask_mode = HAS_MASK;
    sampler_type =
        SamplerTypeFromTextureTarget(params->mask_resource_lock->target());
  }
  SetUseProgram(
      ProgramKey::RenderPass(
          tex_coord_precision, sampler_type, shader_blend_mode,
          params->use_aa ? USE_AA : NO_AA, mask_mode, mask_for_background,
          params->use_color_matrix, tint_gl_composited_content_,
          params->apply_shader_based_rounded_corner &&
              ShouldApplyRoundedCorner(params->quad)),
      params->contents_and_bypass_color_space, target_color_space);
}

void GLRenderer::UpdateRPDQUniforms(DrawRenderPassDrawQuadParams* params) {
  gfx::RectF tex_rect = params->tex_coord_rect;

  gfx::Size texture_size;
  if (params->filter_image) {
    texture_size.set_width(params->filter_image->width());
    texture_size.set_height(params->filter_image->height());
  } else if (params->contents_texture) {
    texture_size = params->contents_texture->size();
  } else {
    texture_size = params->bypass_quad_texture.size;
  }

  tex_rect.Scale(1.0f / texture_size.width(), 1.0f / texture_size.height());

  DCHECK(current_program_->vertex_tex_transform_location() != -1 ||
         IsContextLost());
  if (params->source_needs_flip) {
    // Flip the content vertically in the shader, as the RenderPass input
    // texture is already oriented the same way as the framebuffer, but the
    // projection transform does a flip.
    gl_->Uniform4f(current_program_->vertex_tex_transform_location(),
                   tex_rect.x(), 1.0f - tex_rect.y(), tex_rect.width(),
                   -tex_rect.height());
  } else {
    // Tile textures are oriented opposite the framebuffer, so can use
    // the projection transform to do the flip.
    gl_->Uniform4f(current_program_->vertex_tex_transform_location(),
                   tex_rect.x(), tex_rect.y(), tex_rect.width(),
                   tex_rect.height());
  }

  GLint last_texture_unit = 0;
  if (current_program_->mask_sampler_location() != -1) {
    DCHECK(params->mask_resource_lock);
    DCHECK_NE(current_program_->mask_tex_coord_scale_location(), 1);
    DCHECK_NE(current_program_->mask_tex_coord_offset_location(), 1);
    gl_->Uniform1i(current_program_->mask_sampler_location(), 1);

    gfx::RectF mask_uv_rect = params->quad->mask_uv_rect;
    if (SamplerTypeFromTextureTarget(params->mask_resource_lock->target()) !=
        SAMPLER_TYPE_2D) {
      mask_uv_rect.Scale(params->quad->mask_texture_size.width(),
                         params->quad->mask_texture_size.height());
    }

    SkMatrix tex_to_mask = SkMatrix::RectToRect(RectFToSkRect(tex_rect),
                                                RectFToSkRect(mask_uv_rect));

    if (params->source_needs_flip) {
      // Mask textures are oriented vertically flipped relative to the
      // framebuffer and the RenderPass contents texture, so we flip the tex
      // coords from the RenderPass texture to find the mask texture coords.
      tex_to_mask.preTranslate(0, 1);
      tex_to_mask.preScale(1, -1);
    }

    gl_->Uniform2f(current_program_->mask_tex_coord_offset_location(),
                   tex_to_mask.getTranslateX(), tex_to_mask.getTranslateY());
    gl_->Uniform2f(current_program_->mask_tex_coord_scale_location(),
                   tex_to_mask.getScaleX(), tex_to_mask.getScaleY());
    last_texture_unit = 1;
  }

  if (current_program_->edge_location() != -1)
    gl_->Uniform3fv(current_program_->edge_location(), 8, params->edge);

  if (current_program_->color_matrix_location() != -1) {
    float matrix[16];
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j)
        matrix[i * 4 + j] = SkScalarToFloat(params->color_matrix[j * 5 + i]);
    }
    gl_->UniformMatrix4fv(current_program_->color_matrix_location(), 1, false,
                          matrix);
  }

  if (current_program_->color_offset_location() != -1) {
    float offset[4];
    for (int i = 0; i < 4; ++i)
      offset[i] = params->color_matrix[i * 5 + 4];

    gl_->Uniform4fv(current_program_->color_offset_location(), 1, offset);
  }

  if (current_program_->tint_color_matrix_location() != -1) {
    auto matrix = cc::DebugColors::TintCompositedContentColorTransformMatrix();
    gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                          false, matrix.data());
  }

  if (current_program_->backdrop_location() != -1) {
    DCHECK(params->background_texture || params->background_image_id);
    DCHECK_NE(current_program_->backdrop_location(), 0);
    DCHECK_NE(current_program_->backdrop_rect_location(), 0);

    ++last_texture_unit;
    gl_->Uniform1i(current_program_->backdrop_location(), last_texture_unit);

    gl_->Uniform4f(current_program_->backdrop_rect_location(),
                   params->background_rect.x(), params->background_rect.y(),
                   1.0f / params->background_rect.width(),
                   1.0f / params->background_rect.height());

    // Either |background_image_id| or |background_texture| will be the
    // |backdrop_location| in the shader.
    if (params->background_image_id) {
      gl_->ActiveTexture(GL_TEXTURE0 + last_texture_unit);
      gl_->BindTexture(GL_TEXTURE_2D, params->background_image_id);
      if (params->backdrop_filter_quality != 1.0f)
        gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      gl_->ActiveTexture(GL_TEXTURE0);
    }
    // If |mask_for_background| then we have both |background_image_id| and
    // |background_texture|, and the latter will be the
    // |original_backdrop_location| in the shader.
    if (params->mask_for_background) {
      DCHECK(params->background_image_id);
      DCHECK(params->background_texture);
      ++last_texture_unit;
      gl_->Uniform1i(current_program_->original_backdrop_location(),
                     last_texture_unit);
    }
    if (params->background_texture) {
      gl_->ActiveTexture(GL_TEXTURE0 + last_texture_unit);
      gl_->BindTexture(GL_TEXTURE_2D, params->background_texture);
      gl_->ActiveTexture(GL_TEXTURE0);
    }
  }

  SetShaderOpacity(params->quad->shared_quad_state->opacity);
  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(params->quad->shared_quad_state->mask_filter_info
                               .rounded_corner_bounds(),
                           params->window_matrix * params->projection_matrix);
  }
  SetShaderQuadF(params->surface_quad);
}

void GLRenderer::DrawRPDQ(const DrawRenderPassDrawQuadParams& params) {
  DrawQuadGeometry(params.projection_matrix, params.quad_to_target_transform,
                   params.dst_rect);

  // Flush the compositor context before the filter bitmap goes out of
  // scope, so the draw gets processed before the filter texture gets deleted.
  if (params.filter_image)
    gl_->Flush();

  if (!params.use_shaders_for_blending)
    RestoreBlendFuncToDefault(params.blend_mode);
}

namespace {
// These functions determine if a quad, clipped by a clip_region contains
// the entire {top|bottom|left|right} edge.
bool is_top(const gfx::QuadF* clip_region, const DrawQuad* quad) {
  if (!quad->IsTopEdge())
    return false;
  if (!clip_region)
    return true;

  return std::abs(clip_region->p1().y()) < kAntiAliasingEpsilon &&
         std::abs(clip_region->p2().y()) < kAntiAliasingEpsilon;
}

bool is_bottom(const gfx::QuadF* clip_region, const DrawQuad* quad) {
  if (!quad->IsBottomEdge())
    return false;
  if (!clip_region)
    return true;

  return std::abs(clip_region->p3().y() -
                  quad->shared_quad_state->quad_layer_rect.height()) <
             kAntiAliasingEpsilon &&
         std::abs(clip_region->p4().y() -
                  quad->shared_quad_state->quad_layer_rect.height()) <
             kAntiAliasingEpsilon;
}

bool is_left(const gfx::QuadF* clip_region, const DrawQuad* quad) {
  if (!quad->IsLeftEdge())
    return false;
  if (!clip_region)
    return true;

  return std::abs(clip_region->p1().x()) < kAntiAliasingEpsilon &&
         std::abs(clip_region->p4().x()) < kAntiAliasingEpsilon;
}

bool is_right(const gfx::QuadF* clip_region, const DrawQuad* quad) {
  if (!quad->IsRightEdge())
    return false;
  if (!clip_region)
    return true;

  return std::abs(clip_region->p2().x() -
                  quad->shared_quad_state->quad_layer_rect.width()) <
             kAntiAliasingEpsilon &&
         std::abs(clip_region->p3().x() -
                  quad->shared_quad_state->quad_layer_rect.width()) <
             kAntiAliasingEpsilon;
}
}  // anonymous namespace

static gfx::QuadF GetDeviceQuadWithAntialiasingOnExteriorEdges(
    const LayerQuad& device_layer_edges,
    const gfx::Transform& device_transform,
    const gfx::QuadF& tile_quad,
    const gfx::QuadF* clip_region,
    const DrawQuad* quad) {
  auto tile_rect = gfx::RectF(quad->visible_rect);

  gfx::PointF bottom_right = tile_quad.p3();
  gfx::PointF bottom_left = tile_quad.p4();
  gfx::PointF top_left = tile_quad.p1();
  gfx::PointF top_right = tile_quad.p2();
  bool clipped = false;

  // Map points to device space. We ignore |clipped|, since the result of
  // |MapPoint()| still produces a valid point to draw the quad with. When
  // clipped, the point will be outside of the viewport. See crbug.com/416367.
  bottom_right =
      cc::MathUtil::MapPoint(device_transform, bottom_right, &clipped);
  bottom_left = cc::MathUtil::MapPoint(device_transform, bottom_left, &clipped);
  top_left = cc::MathUtil::MapPoint(device_transform, top_left, &clipped);
  top_right = cc::MathUtil::MapPoint(device_transform, top_right, &clipped);

  LayerQuad::Edge bottom_edge(bottom_right, bottom_left);
  LayerQuad::Edge left_edge(bottom_left, top_left);
  LayerQuad::Edge top_edge(top_left, top_right);
  LayerQuad::Edge right_edge(top_right, bottom_right);

  // Only apply anti-aliasing to edges not clipped by culling or scissoring.
  // If an edge is degenerate we do not want to replace it with a "proper" edge
  // as that will cause the quad to possibly expand in strange ways.
  if (!top_edge.degenerate() && is_top(clip_region, quad) &&
      tile_rect.y() == quad->rect.y()) {
    top_edge = device_layer_edges.top();
  }
  if (!left_edge.degenerate() && is_left(clip_region, quad) &&
      tile_rect.x() == quad->rect.x()) {
    left_edge = device_layer_edges.left();
  }
  if (!right_edge.degenerate() && is_right(clip_region, quad) &&
      tile_rect.right() == quad->rect.right()) {
    right_edge = device_layer_edges.right();
  }
  if (!bottom_edge.degenerate() && is_bottom(clip_region, quad) &&
      tile_rect.bottom() == quad->rect.bottom()) {
    bottom_edge = device_layer_edges.bottom();
  }

  float sign = tile_quad.IsCounterClockwise() ? -1 : 1;
  bottom_edge.scale(sign);
  left_edge.scale(sign);
  top_edge.scale(sign);
  right_edge.scale(sign);

  // Create device space quad.
  return LayerQuad(left_edge, top_edge, right_edge, bottom_edge).ToQuadF();
}

float GetTotalQuadError(const gfx::QuadF* clipped_quad,
                        const gfx::QuadF* ideal_rect) {
  return (clipped_quad->p1() - ideal_rect->p1()).LengthSquared() +
         (clipped_quad->p2() - ideal_rect->p2()).LengthSquared() +
         (clipped_quad->p3() - ideal_rect->p3()).LengthSquared() +
         (clipped_quad->p4() - ideal_rect->p4()).LengthSquared();
}

// Attempt to rotate the clipped quad until it lines up the most
// correctly. This is necessary because we check the edges of this
// quad against the expected left/right/top/bottom for anti-aliasing.
void AlignQuadToBoundingBox(gfx::QuadF* clipped_quad) {
  auto bounding_quad = gfx::QuadF(clipped_quad->BoundingBox());
  gfx::QuadF best_rotation = *clipped_quad;
  float least_error_amount = GetTotalQuadError(clipped_quad, &bounding_quad);
  for (size_t i = 1; i < 4; ++i) {
    clipped_quad->Realign(1);
    float new_error = GetTotalQuadError(clipped_quad, &bounding_quad);
    if (new_error < least_error_amount) {
      least_error_amount = new_error;
      best_rotation = *clipped_quad;
    }
  }
  *clipped_quad = best_rotation;
}

void InflateAntiAliasingDistances(const gfx::QuadF& quad,
                                  LayerQuad* device_layer_edges,
                                  float edge[24]) {
  DCHECK(!quad.BoundingBox().IsEmpty());
  LayerQuad device_layer_bounds(gfx::QuadF(quad.BoundingBox()));

  device_layer_edges->InflateAntiAliasingDistance();
  device_layer_edges->ToFloatArray(edge);

  device_layer_bounds.InflateAntiAliasingDistance();
  device_layer_bounds.ToFloatArray(&edge[12]);
}

// static
bool GLRenderer::ShouldAntialiasQuad(const gfx::QuadF& device_layer_quad,
                                     bool clipped,
                                     bool force_aa) {
  // AAing clipped quads is not supported by the code yet.
  if (clipped)
    return false;
  if (device_layer_quad.BoundingBox().IsEmpty())
    return false;
  if (force_aa)
    return true;

  bool is_axis_aligned_in_target = device_layer_quad.IsRectilinear();
  bool is_nearest_rect_within_epsilon =
      is_axis_aligned_in_target &&
      gfx::IsNearestRectWithinDistance(device_layer_quad.BoundingBox(),
                                       kAntiAliasingEpsilon);
  return !is_nearest_rect_within_epsilon;
}

// static
void GLRenderer::SetupQuadForClippingAndAntialiasing(
    const gfx::Transform& device_transform,
    const DrawQuad* quad,
    const gfx::QuadF* aa_quad,
    const gfx::QuadF* clip_region,
    gfx::QuadF* local_quad,
    float edge[24]) {
  gfx::QuadF rotated_clip;
  const gfx::QuadF* local_clip_region = clip_region;
  if (local_clip_region) {
    rotated_clip = *clip_region;
    AlignQuadToBoundingBox(&rotated_clip);
    local_clip_region = &rotated_clip;
  }

  if (!aa_quad) {
    if (local_clip_region)
      *local_quad = *local_clip_region;
    return;
  }

  LayerQuad device_layer_edges(*aa_quad);
  InflateAntiAliasingDistances(*aa_quad, &device_layer_edges, edge);

  // If we have a clip region then we are split, and therefore
  // by necessity, at least one of our edges is not an external
  // one.
  bool is_full_rect = quad->visible_rect == quad->rect;

  bool region_contains_all_outside_edges =
      is_full_rect &&
      (is_top(local_clip_region, quad) && is_left(local_clip_region, quad) &&
       is_bottom(local_clip_region, quad) && is_right(local_clip_region, quad));

  bool use_aa_on_all_four_edges =
      !local_clip_region && region_contains_all_outside_edges;

  gfx::QuadF device_quad;
  if (use_aa_on_all_four_edges) {
    device_quad = device_layer_edges.ToQuadF();
  } else {
    gfx::QuadF tile_quad(local_clip_region
                             ? *local_clip_region
                             : gfx::QuadF(gfx::RectF(quad->visible_rect)));
    device_quad = GetDeviceQuadWithAntialiasingOnExteriorEdges(
        device_layer_edges, device_transform, tile_quad, local_clip_region,
        quad);
  }

  *local_quad =
      cc::MathUtil::InverseMapQuadToLocalSpace(device_transform, device_quad);
}

// static
void GLRenderer::SetupRenderPassQuadForClippingAndAntialiasing(
    const gfx::Transform& device_transform,
    const AggregatedRenderPassDrawQuad* quad,
    const gfx::QuadF* aa_quad,
    const gfx::QuadF* clip_region,
    gfx::QuadF* local_quad,
    float edge[24]) {
  gfx::QuadF rotated_clip;
  const gfx::QuadF* local_clip_region = clip_region;
  if (local_clip_region) {
    rotated_clip = *clip_region;
    AlignQuadToBoundingBox(&rotated_clip);
    local_clip_region = &rotated_clip;
  }

  if (!aa_quad) {
    GetScaledRegion(quad->rect, local_clip_region, local_quad);
    return;
  }

  LayerQuad device_layer_edges(*aa_quad);
  InflateAntiAliasingDistances(*aa_quad, &device_layer_edges, edge);

  gfx::QuadF device_quad;

  // Apply anti-aliasing only to the edges that are not being clipped
  if (local_clip_region) {
    gfx::QuadF tile_quad(gfx::RectF(quad->visible_rect));
    GetScaledRegion(quad->rect, local_clip_region, &tile_quad);
    device_quad = GetDeviceQuadWithAntialiasingOnExteriorEdges(
        device_layer_edges, device_transform, tile_quad, local_clip_region,
        quad);
  } else {
    device_quad = device_layer_edges.ToQuadF();
  }

  *local_quad =
      cc::MathUtil::InverseMapQuadToLocalSpace(device_transform, device_quad);
}

void GLRenderer::DrawSolidColorQuad(const SolidColorDrawQuad* quad,
                                    const gfx::QuadF* clip_region) {
  gfx::Rect tile_rect = quad->visible_rect;

  SkColor color = quad->color;
  float opacity = quad->shared_quad_state->opacity;

  // Early out if alpha is small enough that quad doesn't contribute to output,
  // for kSrcOver blend mode.
  if (quad->shared_quad_state->blend_mode == SkBlendMode::kSrcOver) {
    float alpha = (SkColorGetA(color) * (1.0f / 255.0f)) * opacity;
    if (alpha < std::numeric_limits<float>::epsilon() &&
        quad->ShouldDrawWithBlending() &&
        quad->shared_quad_state->blend_mode == SkBlendMode::kSrcOver)
      return;
  }

  gfx::Transform device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad->shared_quad_state->quad_to_target_transform;
  device_transform.FlattenTo2d();
  if (!device_transform.IsInvertible())
    return;

  auto local_quad = gfx::QuadF(gfx::RectF(tile_rect));

  gfx::QuadF device_layer_quad;
  bool use_aa = false;
  bool allow_aa = settings_->allow_antialiasing &&
                  !quad->force_anti_aliasing_off && quad->IsEdge();

  if (allow_aa) {
    bool clipped = false;
    bool force_aa = false;
    device_layer_quad = cc::MathUtil::MapQuad(
        device_transform,
        gfx::QuadF(
            gfx::RectF(quad->shared_quad_state->visible_quad_layer_rect)),
        &clipped);
    use_aa = ShouldAntialiasQuad(device_layer_quad, clipped, force_aa);
  }

  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_,
                                      use_aa ? "kSolidColorAA" : "kSolidColor");

  float edge[24];
  const gfx::QuadF* aa_quad = use_aa ? &device_layer_quad : nullptr;
  SetupQuadForClippingAndAntialiasing(device_transform, quad, aa_quad,
                                      clip_region, &local_quad, edge);

  SetUseProgram(ProgramKey::SolidColor(use_aa ? USE_AA : NO_AA,
                                       tint_gl_composited_content_,
                                       ShouldApplyRoundedCorner(quad)),
                CurrentRenderPassColorSpace(), CurrentRenderPassColorSpace());

  gfx::ColorSpace quad_color_space = gfx::ColorSpace::CreateSRGB();
  SkColor4f color_f = SkColor4f::FromColor(color);

  // Apply color transform if the color space or source and target do not match.
  if (quad_color_space != CurrentRenderPassColorSpace()) {
    const gfx::ColorTransform* color_transform =
        GetColorTransform(quad_color_space, CurrentRenderPassColorSpace());
    gfx::ColorTransform::TriStim col(color_f.fR, color_f.fG, color_f.fB);
    color_transform->Transform(&col, 1);
    color_f.fR = col.x();
    color_f.fG = col.y();
    color_f.fB = col.z();
    color = color_f.toSkColor();
  }

  // Apply any color matrix that may be present.
  if (HasOutputColorMatrix()) {
    const SkMatrix44& output_color_matrix = output_surface_->color_matrix();
    const SkVector4 color_v(color_f.fR, color_f.fG, color_f.fB, color_f.fA);
    const SkVector4 result = output_color_matrix * color_v;
    std::copy(result.fData, result.fData + 4, color_f.vec());
    color = color_f.toSkColor();
  }

  // Try using glClear to draw the solid color quad if possible. This is much
  // more performant than executing the shader pipeline.
  if (CanUseFastSolidColorDraw(quad) && !use_aa) {
    // Pre-multiply the alpha and opacity to get the correct blending in case of
    // transparent buffers. glClear does not have any alpha blending stage.
    Float4 result = PremultipliedColor(color, opacity);
    SkRGBA4f<kPremul_SkAlphaType> color_f_premul;
    std::copy(result.data, result.data + 4, color_f_premul.vec());

    gfx::RectF quad_rect_in_target_f(quad->visible_rect);

    device_transform.TransformRect(&quad_rect_in_target_f);
    gfx::Rect quad_rect_in_target = gfx::ToRoundedRect(quad_rect_in_target_f);

    // If we are using partial swap, make sure the new scissor rect is within
    // the partial swap bounds.
    if (!scissor_rect_.IsEmpty() && is_scissor_enabled_)
      quad_rect_in_target.Intersect(scissor_rect_);

    gl_->Enable(GL_SCISSOR_TEST);
    gl_->Scissor(quad_rect_in_target.x(), quad_rect_in_target.y(),
                 quad_rect_in_target.width(), quad_rect_in_target.height());

    gl_->ClearColor(color_f_premul.fR, color_f_premul.fG, color_f_premul.fB,
                    color_f_premul.fA);
    gl_->Clear(GL_COLOR_BUFFER_BIT);

    // Restore GL scissor state.
    if (is_scissor_enabled_)
      gl_->Enable(GL_SCISSOR_TEST);
    else
      gl_->Disable(GL_SCISSOR_TEST);

    gl_->Scissor(scissor_rect_.x(), scissor_rect_.y(), scissor_rect_.width(),
                 scissor_rect_.height());
  } else {
    SetShaderColor(color, opacity);
    if (current_program_->rounded_corner_rect_location() != -1) {
      SetShaderRoundedCorner(
          quad->shared_quad_state->mask_filter_info.rounded_corner_bounds(),
          current_frame()->window_matrix * current_frame()->projection_matrix);
    }

    if (current_program_->tint_color_matrix_location() != -1) {
      auto matrix =
          cc::DebugColors::TintCompositedContentColorTransformMatrix();
      gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                            false, matrix.data());
    }

    if (use_aa) {
      gl_->Uniform3fv(current_program_->edge_location(), 8, edge);
    }

    // Enable blending when the quad properties require it or if we decided
    // to use antialiasing.
    SetBlendEnabled(quad->ShouldDrawWithBlending() || use_aa);
    ApplyBlendModeUsingBlendFunc(quad->shared_quad_state->blend_mode);

    // Antialiasing requires a normalized quad, but this could lead to floating
    // point precision errors, so only normalize when antialiasing is on.
    if (use_aa) {
      DrawQuadGeometryWithAA(quad, &local_quad, tile_rect);
    } else {
      PrepareGeometry(SHARED_BINDING);
      SetShaderQuadF(local_quad);
      SetShaderMatrix(current_frame()->projection_matrix *
                      quad->shared_quad_state->quad_to_target_transform);
      gl_->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
      num_triangles_drawn_ += 2;
    }
    RestoreBlendFuncToDefault(quad->shared_quad_state->blend_mode);
  }

  // Add the quad to the region that has been drawn.
  AccumulateDrawRects(quad->visible_rect,
                      quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);
}

void GLRenderer::DrawTileQuad(const TileDrawQuad* quad,
                              const gfx::QuadF* clip_region) {
  DrawContentQuad(quad, quad->resource_id(), clip_region);
}

void GLRenderer::DrawContentQuad(const ContentDrawQuadBase* quad,
                                 ResourceId resource_id,
                                 const gfx::QuadF* clip_region) {
  gfx::Transform device_transform =
      current_frame()->window_matrix * current_frame()->projection_matrix *
      quad->shared_quad_state->quad_to_target_transform;
  device_transform.FlattenTo2d();

  gfx::QuadF device_layer_quad;
  bool use_aa = false;
  bool allow_aa = settings_->allow_antialiasing &&
                  !quad->force_anti_aliasing_off && quad->IsEdge();
  if (allow_aa) {
    bool clipped = false;
    bool force_aa = false;
    device_layer_quad = cc::MathUtil::MapQuad(
        device_transform,
        gfx::QuadF(
            gfx::RectF(quad->shared_quad_state->visible_quad_layer_rect)),
        &clipped);
    use_aa = ShouldAntialiasQuad(device_layer_quad, clipped, force_aa);
  }

  // TODO(timav): simplify coordinate transformations in DrawContentQuadAA
  // similar to the way DrawContentQuadNoAA works and then consider
  // combining DrawContentQuadAA and DrawContentQuadNoAA into one method.
  if (use_aa)
    DrawContentQuadAA(quad, resource_id, device_transform, device_layer_quad,
                      clip_region);
  else
    DrawContentQuadNoAA(quad, resource_id, clip_region);

  AccumulateDrawRects(quad->visible_rect,
                      quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);
}

void GLRenderer::DrawContentQuadAA(const ContentDrawQuadBase* quad,
                                   ResourceId resource_id,
                                   const gfx::Transform& device_transform,
                                   const gfx::QuadF& aa_quad,
                                   const gfx::QuadF* clip_region) {
  if (!device_transform.IsInvertible())
    return;

  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_, "kTiledContentAA");

  gfx::Rect tile_rect = quad->visible_rect;

  gfx::RectF tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect), gfx::RectF(tile_rect));
  float tex_to_geom_scale_x = quad->rect.width() / quad->tex_coord_rect.width();
  float tex_to_geom_scale_y =
      quad->rect.height() / quad->tex_coord_rect.height();

  gfx::RectF clamp_geom_rect(tile_rect);
  gfx::RectF clamp_tex_rect(tex_coord_rect);
  // Clamp texture coordinates to avoid sampling outside the layer
  // by deflating the tile region half a texel or half a texel
  // minus epsilon for one pixel layers. The resulting clamp region
  // is mapped to the unit square by the vertex shader and mapped
  // back to normalized texture coordinates by the fragment shader
  // after being clamped to 0-1 range.
  float tex_clamp_x =
      std::min(0.5f, 0.5f * clamp_tex_rect.width() - kAntiAliasingEpsilon);
  float tex_clamp_y =
      std::min(0.5f, 0.5f * clamp_tex_rect.height() - kAntiAliasingEpsilon);
  float geom_clamp_x =
      std::min(tex_clamp_x * tex_to_geom_scale_x,
               0.5f * clamp_geom_rect.width() - kAntiAliasingEpsilon);
  float geom_clamp_y =
      std::min(tex_clamp_y * tex_to_geom_scale_y,
               0.5f * clamp_geom_rect.height() - kAntiAliasingEpsilon);
  clamp_geom_rect.Inset(geom_clamp_x, geom_clamp_y, geom_clamp_x, geom_clamp_y);
  clamp_tex_rect.Inset(tex_clamp_x, tex_clamp_y, tex_clamp_x, tex_clamp_y);

  // Map clamping rectangle to unit square.
  float vertex_tex_translate_x = -clamp_geom_rect.x() / clamp_geom_rect.width();
  float vertex_tex_translate_y =
      -clamp_geom_rect.y() / clamp_geom_rect.height();
  float vertex_tex_scale_x = tile_rect.width() / clamp_geom_rect.width();
  float vertex_tex_scale_y = tile_rect.height() / clamp_geom_rect.height();

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      gl_, &highp_threshold_cache_, settings_->highp_threshold_min,
      quad->texture_size);

  auto local_quad = gfx::QuadF(gfx::RectF(tile_rect));
  float edge[24];
  SetupQuadForClippingAndAntialiasing(device_transform, quad, &aa_quad,
                                      clip_region, &local_quad, edge);
  DisplayResourceProviderGL::ScopedSamplerGL quad_resource_lock(
      resource_provider(), resource_id,
      quad->nearest_neighbor ? GL_NEAREST : GL_LINEAR);
  SamplerType sampler =
      SamplerTypeFromTextureTarget(quad_resource_lock.target());

  float fragment_tex_translate_x = clamp_tex_rect.x();
  float fragment_tex_translate_y = clamp_tex_rect.y();
  float fragment_tex_scale_x = clamp_tex_rect.width();
  float fragment_tex_scale_y = clamp_tex_rect.height();

  // Map to normalized texture coordinates.
  if (sampler != SAMPLER_TYPE_2D_RECT) {
    gfx::Size texture_size = quad->texture_size;
    DCHECK(!texture_size.IsEmpty());
    fragment_tex_translate_x /= texture_size.width();
    fragment_tex_translate_y /= texture_size.height();
    fragment_tex_scale_x /= texture_size.width();
    fragment_tex_scale_y /= texture_size.height();
  }

  SetUseProgram(
      ProgramKey::Tile(tex_coord_precision, sampler, USE_AA,
                       quad->is_premultiplied ? PREMULTIPLIED_ALPHA
                                              : NON_PREMULTIPLIED_ALPHA,
                       false, false, tint_gl_composited_content_,
                       ShouldApplyRoundedCorner(quad)),
      quad_resource_lock.color_space(), CurrentRenderPassColorSpace());

  if (current_program_->tint_color_matrix_location() != -1) {
    auto matrix = cc::DebugColors::TintCompositedContentColorTransformMatrix();
    gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                          false, matrix.data());
  }

  gl_->Uniform3fv(current_program_->edge_location(), 8, edge);

  gl_->Uniform4f(current_program_->vertex_tex_transform_location(),
                 vertex_tex_translate_x, vertex_tex_translate_y,
                 vertex_tex_scale_x, vertex_tex_scale_y);
  gl_->Uniform4f(current_program_->fragment_tex_transform_location(),
                 fragment_tex_translate_x, fragment_tex_translate_y,
                 fragment_tex_scale_x, fragment_tex_scale_y);

  // Blending is required for antialiasing.
  SetBlendEnabled(true);
  SetShaderOpacity(quad->shared_quad_state->opacity);
  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(
        quad->shared_quad_state->mask_filter_info.rounded_corner_bounds(),
        current_frame()->window_matrix * current_frame()->projection_matrix);
  }
  DCHECK(CanApplyBlendModeUsingBlendFunc(quad->shared_quad_state->blend_mode));
  ApplyBlendModeUsingBlendFunc(quad->shared_quad_state->blend_mode);

  // Draw the quad with antialiasing.
  DrawQuadGeometryWithAA(quad, &local_quad, tile_rect);
  RestoreBlendFuncToDefault(quad->shared_quad_state->blend_mode);
}

void GLRenderer::DrawContentQuadNoAA(const ContentDrawQuadBase* quad,
                                     ResourceId resource_id,
                                     const gfx::QuadF* clip_region) {
  gfx::RectF tex_coord_rect = cc::MathUtil::ScaleRectProportional(
      quad->tex_coord_rect, gfx::RectF(quad->rect),
      gfx::RectF(quad->visible_rect));
  float tex_to_geom_scale_x = quad->rect.width() / quad->tex_coord_rect.width();
  float tex_to_geom_scale_y =
      quad->rect.height() / quad->tex_coord_rect.height();

  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_, "kTiledContent");

  bool scaled = (tex_to_geom_scale_x != 1.f || tex_to_geom_scale_y != 1.f);
  GLenum filter = (scaled || !quad->shared_quad_state->quad_to_target_transform
                                  .IsIdentityOrIntegerTranslation()) &&
                          !quad->nearest_neighbor
                      ? GL_LINEAR
                      : GL_NEAREST;

  DisplayResourceProviderGL::ScopedSamplerGL quad_resource_lock(
      resource_provider(), resource_id, filter);
  SamplerType sampler =
      SamplerTypeFromTextureTarget(quad_resource_lock.target());

  // Tiles are guaranteed to have been entirely filled except for the
  // bottom/right external edge tiles.  Because of border texels, any
  // internal edge will have uvs that are offset from 0 and 1, so
  // clamping to tex_coord_rect in all cases would cause these border
  // texels to not be sampled.  Therefore, only clamp texture coordinates
  // for external edge bottom/right tiles that don't have content all
  // the way to the edge and are using bilinear filtering.
  gfx::Size texture_size = quad->texture_size;
  bool fills_right_edge =
      !quad->IsRightEdge() || texture_size.width() == tex_coord_rect.right();
  bool fills_bottom_edge =
      !quad->IsBottomEdge() || texture_size.height() == tex_coord_rect.bottom();
  bool has_tex_clamp_rect =
      filter == GL_LINEAR && (!fills_right_edge || !fills_bottom_edge);
  gfx::SizeF tex_clamp_size(texture_size);
  // Clamp from the original tex coord rect, instead of the one that has
  // been adjusted by the visible rect.
  if (!fills_right_edge)
    tex_clamp_size.set_width(quad->tex_coord_rect.right() - 0.5f);
  if (!fills_bottom_edge)
    tex_clamp_size.set_height(quad->tex_coord_rect.bottom() - 0.5f);

  // Map to normalized texture coordinates.
  if (sampler != SAMPLER_TYPE_2D_RECT) {
    DCHECK(!texture_size.IsEmpty());
    tex_coord_rect.Scale(1.f / texture_size.width(),
                         1.f / texture_size.height());
    tex_clamp_size.Scale(1.f / texture_size.width(),
                         1.f / texture_size.height());
  }

  TexCoordPrecision tex_coord_precision =
      TexCoordPrecisionRequired(gl_, &highp_threshold_cache_,
                                settings_->highp_threshold_min, texture_size);
  SetUseProgram(
      ProgramKey::Tile(tex_coord_precision, sampler, NO_AA,
                       quad->is_premultiplied ? PREMULTIPLIED_ALPHA
                                              : NON_PREMULTIPLIED_ALPHA,
                       !quad->ShouldDrawWithBlending(), has_tex_clamp_rect,
                       tint_gl_composited_content_,
                       ShouldApplyRoundedCorner(quad)),
      quad_resource_lock.color_space(), CurrentRenderPassColorSpace());

  if (current_program_->tint_color_matrix_location() != -1) {
    auto matrix = cc::DebugColors::TintCompositedContentColorTransformMatrix();
    gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                          false, matrix.data());
  }

  if (has_tex_clamp_rect) {
    gl_->Uniform4f(current_program_->tex_clamp_rect_location(), 0, 0,
                   tex_clamp_size.width(), tex_clamp_size.height());
  }
  gl_->Uniform4f(current_program_->vertex_tex_transform_location(),
                 tex_coord_rect.x(), tex_coord_rect.y(), tex_coord_rect.width(),
                 tex_coord_rect.height());

  DCHECK(CanApplyBlendModeUsingBlendFunc(quad->shared_quad_state->blend_mode));
  SetBlendEnabled(quad->ShouldDrawWithBlending());
  ApplyBlendModeUsingBlendFunc(quad->shared_quad_state->blend_mode);

  SetShaderOpacity(quad->shared_quad_state->opacity);
  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(
        quad->shared_quad_state->mask_filter_info.rounded_corner_bounds(),
        current_frame()->window_matrix * current_frame()->projection_matrix);
  }

  // Pass quad coordinates to the uniform in the same order as GeometryBinding
  // does, then vertices will match the texture mapping in the vertex buffer.
  // The method SetShaderQuadF() changes the order of vertices and so it's
  // not used here.
  auto tile_quad = gfx::QuadF(gfx::RectF(quad->visible_rect));
  float width = quad->visible_rect.width();
  float height = quad->visible_rect.height();
  auto top_left = gfx::PointF(quad->visible_rect.origin());
  if (clip_region) {
    tile_quad = *clip_region;
    float gl_uv[8] = {
        (tile_quad.p4().x() - top_left.x()) / width,
        (tile_quad.p4().y() - top_left.y()) / height,
        (tile_quad.p1().x() - top_left.x()) / width,
        (tile_quad.p1().y() - top_left.y()) / height,
        (tile_quad.p2().x() - top_left.x()) / width,
        (tile_quad.p2().y() - top_left.y()) / height,
        (tile_quad.p3().x() - top_left.x()) / width,
        (tile_quad.p3().y() - top_left.y()) / height,
    };
    PrepareGeometry(CLIPPED_BINDING);
    clipped_geometry_->InitializeCustomQuadWithUVs(
        gfx::QuadF(gfx::RectF(quad->visible_rect)), gl_uv);
  } else {
    PrepareGeometry(SHARED_BINDING);
  }
  float gl_quad[8] = {
      tile_quad.p4().x(), tile_quad.p4().y(), tile_quad.p1().x(),
      tile_quad.p1().y(), tile_quad.p2().x(), tile_quad.p2().y(),
      tile_quad.p3().x(), tile_quad.p3().y(),
  };
  gl_->Uniform2fv(current_program_->quad_location(), 4, gl_quad);

  SetShaderMatrix(current_frame()->projection_matrix *
                  quad->shared_quad_state->quad_to_target_transform);

  gl_->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
  num_triangles_drawn_ += 2;
  RestoreBlendFuncToDefault(quad->shared_quad_state->blend_mode);
}

void GLRenderer::DrawYUVVideoQuad(const YUVVideoDrawQuad* quad,
                                  const gfx::QuadF* clip_region) {
  std::string gpu_composite_time_string;
  if (!clip_region && quad->rect == quad->visible_rect)
    gpu_composite_time_string = "kYuvVideoContent";
  else
    gpu_composite_time_string = "kYuvVideoContentClipped";
  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_,
                                      gpu_composite_time_string);

  SetBlendEnabled(quad->ShouldDrawWithBlending());

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      gl_, &highp_threshold_cache_, settings_->highp_threshold_min,
      quad->shared_quad_state->visible_quad_layer_rect.size());
  YUVAlphaTextureMode alpha_texture_mode = quad->a_plane_resource_id()
                                               ? YUV_HAS_ALPHA_TEXTURE
                                               : YUV_NO_ALPHA_TEXTURE;
  UVTextureMode uv_texture_mode =
      quad->v_plane_resource_id() == quad->u_plane_resource_id()
          ? UV_TEXTURE_MODE_UV
          : UV_TEXTURE_MODE_U_V;

  DisplayResourceProviderGL::ScopedSamplerGL y_plane_lock(
      resource_provider(), quad->y_plane_resource_id(), GL_TEXTURE1, GL_LINEAR);
  DisplayResourceProviderGL::ScopedSamplerGL u_plane_lock(
      resource_provider(), quad->u_plane_resource_id(), GL_TEXTURE2, GL_LINEAR);
  DCHECK_EQ(y_plane_lock.target(), u_plane_lock.target());
  DCHECK_EQ(y_plane_lock.color_space(), u_plane_lock.color_space());

  // TODO(ccameron): There are currently two sources of the color space: the
  // resource color space and quad->video_color_space. Remove one of them.
  gfx::ColorSpace src_color_space = quad->video_color_space;
  // Invalid or unspecified color spaces should be treated as REC709.
  if (!src_color_space.IsValid())
    src_color_space = gfx::ColorSpace::CreateREC709();
  else
    DCHECK_EQ(src_color_space, y_plane_lock.color_space());
  // The source color space should never be RGB.
  DCHECK_NE(src_color_space, src_color_space.GetAsFullRangeRGB());

  gfx::ColorSpace dst_color_space = CurrentRenderPassColorSpace();

#if defined(OS_WIN)
  // Force sRGB output on Windows for overlay candidate video quads to match
  // DirectComposition behavior in case these switch between overlays and
  // compositing. See https://crbug.com/811118 for details.
  // Currently if HDR is supported, OverlayProcessor doesn't promote HDR video
  // frame as overlay candidate. So it's unnecessary to worry about the
  // compositing-overlay switch here. In addition drawing a HDR video using sRGB
  // can cancel the advantages of HDR.
  const bool supports_dc_layers =
      output_surface_->capabilities().supports_dc_layers;
  if (supports_dc_layers && !src_color_space.IsHDR() &&
      resource_provider()->IsOverlayCandidate(quad->y_plane_resource_id())) {
    DCHECK(
        resource_provider()->IsOverlayCandidate(quad->u_plane_resource_id()));
    dst_color_space = gfx::ColorSpace::CreateSRGB();
  }
#endif

  // TODO(jbauman): Use base::Optional when available.
  std::unique_ptr<DisplayResourceProviderGL::ScopedSamplerGL> v_plane_lock;
  if (uv_texture_mode == UV_TEXTURE_MODE_U_V) {
    v_plane_lock = std::make_unique<DisplayResourceProviderGL::ScopedSamplerGL>(
        resource_provider(), quad->v_plane_resource_id(), GL_TEXTURE3,
        GL_LINEAR);
    DCHECK_EQ(y_plane_lock.target(), v_plane_lock->target());
    DCHECK_EQ(y_plane_lock.color_space(), v_plane_lock->color_space());
  }
  std::unique_ptr<DisplayResourceProviderGL::ScopedSamplerGL> a_plane_lock;
  if (alpha_texture_mode == YUV_HAS_ALPHA_TEXTURE) {
    a_plane_lock = std::make_unique<DisplayResourceProviderGL::ScopedSamplerGL>(
        resource_provider(), quad->a_plane_resource_id(), GL_TEXTURE4,
        GL_LINEAR);
    DCHECK_EQ(y_plane_lock.target(), a_plane_lock->target());
  }

  // All planes must have the same sampler type.
  SamplerType sampler = SamplerTypeFromTextureTarget(y_plane_lock.target());

  SetUseProgram(
      ProgramKey::YUVVideo(tex_coord_precision, sampler, alpha_texture_mode,
                           uv_texture_mode, tint_gl_composited_content_,
                           ShouldApplyRoundedCorner(quad)),
      src_color_space, dst_color_space, /*adjust_src_white_level=*/true);

  if (current_program_->tint_color_matrix_location() != -1) {
    auto matrix = cc::DebugColors::TintCompositedContentColorTransformMatrix();
    gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                          false, matrix.data());
  }

  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(
        quad->shared_quad_state->mask_filter_info.rounded_corner_bounds(),
        current_frame()->window_matrix * current_frame()->projection_matrix);
  }

  gfx::SizeF ya_tex_scale(1.0f, 1.0f);
  gfx::SizeF uv_tex_scale(1.0f, 1.0f);
  if (sampler != SAMPLER_TYPE_2D_RECT) {
    DCHECK(!quad->ya_tex_size.IsEmpty());
    DCHECK(!quad->uv_tex_size.IsEmpty());
    ya_tex_scale = gfx::SizeF(1.0f / quad->ya_tex_size.width(),
                              1.0f / quad->ya_tex_size.height());
    uv_tex_scale = gfx::SizeF(1.0f / quad->uv_tex_size.width(),
                              1.0f / quad->uv_tex_size.height());
  }

  float ya_vertex_tex_translate_x =
      quad->ya_tex_coord_rect.x() * ya_tex_scale.width();
  float ya_vertex_tex_translate_y =
      quad->ya_tex_coord_rect.y() * ya_tex_scale.height();
  float ya_vertex_tex_scale_x =
      quad->ya_tex_coord_rect.width() * ya_tex_scale.width();
  float ya_vertex_tex_scale_y =
      quad->ya_tex_coord_rect.height() * ya_tex_scale.height();

  float uv_vertex_tex_translate_x =
      quad->uv_tex_coord_rect.x() * uv_tex_scale.width();
  float uv_vertex_tex_translate_y =
      quad->uv_tex_coord_rect.y() * uv_tex_scale.height();
  float uv_vertex_tex_scale_x =
      quad->uv_tex_coord_rect.width() * uv_tex_scale.width();
  float uv_vertex_tex_scale_y =
      quad->uv_tex_coord_rect.height() * uv_tex_scale.height();

  gl_->Uniform2f(current_program_->ya_tex_scale_location(),
                 ya_vertex_tex_scale_x, ya_vertex_tex_scale_y);
  gl_->Uniform2f(current_program_->ya_tex_offset_location(),
                 ya_vertex_tex_translate_x, ya_vertex_tex_translate_y);
  gl_->Uniform2f(current_program_->uv_tex_scale_location(),
                 uv_vertex_tex_scale_x, uv_vertex_tex_scale_y);
  gl_->Uniform2f(current_program_->uv_tex_offset_location(),
                 uv_vertex_tex_translate_x, uv_vertex_tex_translate_y);

  gfx::RectF ya_clamp_rect(ya_vertex_tex_translate_x, ya_vertex_tex_translate_y,
                           ya_vertex_tex_scale_x, ya_vertex_tex_scale_y);
  ya_clamp_rect.Inset(0.5f * ya_tex_scale.width(),
                      0.5f * ya_tex_scale.height());
  gfx::RectF uv_clamp_rect(uv_vertex_tex_translate_x, uv_vertex_tex_translate_y,
                           uv_vertex_tex_scale_x, uv_vertex_tex_scale_y);
  uv_clamp_rect.Inset(0.5f * uv_tex_scale.width(),
                      0.5f * uv_tex_scale.height());
  gl_->Uniform4f(current_program_->ya_clamp_rect_location(), ya_clamp_rect.x(),
                 ya_clamp_rect.y(), ya_clamp_rect.right(),
                 ya_clamp_rect.bottom());
  gl_->Uniform4f(current_program_->uv_clamp_rect_location(), uv_clamp_rect.x(),
                 uv_clamp_rect.y(), uv_clamp_rect.right(),
                 uv_clamp_rect.bottom());

  gl_->Uniform1i(current_program_->y_texture_location(), 1);
  if (uv_texture_mode == UV_TEXTURE_MODE_UV) {
    gl_->Uniform1i(current_program_->uv_texture_location(), 2);
  } else {
    gl_->Uniform1i(current_program_->u_texture_location(), 2);
    gl_->Uniform1i(current_program_->v_texture_location(), 3);
  }
  if (alpha_texture_mode == YUV_HAS_ALPHA_TEXTURE)
    gl_->Uniform1i(current_program_->a_texture_location(), 4);

  gl_->Uniform1f(current_program_->resource_multiplier_location(),
                 quad->resource_multiplier);
  gl_->Uniform1f(current_program_->resource_offset_location(),
                 quad->resource_offset);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  auto tile_rect = gfx::RectF(quad->rect);

  SetShaderOpacity(quad->shared_quad_state->opacity);
  if (!clip_region && quad->rect == quad->visible_rect) {
    DrawQuadGeometry(current_frame()->projection_matrix,
                     quad->shared_quad_state->quad_to_target_transform,
                     tile_rect);
  } else {
    gfx::QuadF region_quad =
        clip_region ? *clip_region : gfx::QuadF(gfx::RectF(quad->visible_rect));
    float uvs[8] = {0};
    GetScaledUVs(quad->rect, &region_quad, uvs);
    region_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());
    region_quad -= gfx::Vector2dF(0.5f, 0.5f);
    DrawQuadGeometryClippedByQuadF(
        quad->shared_quad_state->quad_to_target_transform, tile_rect,
        region_quad, uvs);
  }

  // Track the region in the current target surface that has been drawn to.
  AccumulateDrawRects(quad->visible_rect,
                      quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);
}

void GLRenderer::DrawStreamVideoQuad(const StreamVideoDrawQuad* quad,
                                     const gfx::QuadF* clip_region) {
  std::string gpu_composite_time_string;
  if (!clip_region && quad->rect == quad->visible_rect) {
    gpu_composite_time_string = "kStreamVideoContent";
  } else {
    gpu_composite_time_string = "kStreamVideoContentClipped";
  }
  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_,
                                      gpu_composite_time_string);
  SetBlendEnabled(quad->ShouldDrawWithBlending());

  DCHECK(output_surface_->context_provider()
             ->ContextCapabilities()
             .egl_image_external);

  TexCoordPrecision tex_coord_precision = TexCoordPrecisionRequired(
      gl_, &highp_threshold_cache_, settings_->highp_threshold_min,
      quad->shared_quad_state->visible_quad_layer_rect.size());

  DisplayResourceProviderGL::ScopedReadLockGL lock(resource_provider(),
                                                   quad->resource_id());

  SetUseProgram(ProgramKey::VideoStream(tex_coord_precision,
                                        ShouldApplyRoundedCorner(quad)),
                lock.color_space(), CurrentRenderPassColorSpace());

  DCHECK_EQ(GL_TEXTURE0, GetActiveTextureUnit(gl_));
  gl_->BindTexture(GL_TEXTURE_EXTERNAL_OES, lock.texture_id());

  static float gl_matrix[16];
  gfx::Transform matrix;
  matrix.Scale(quad->uv_bottom_right.x() - quad->uv_top_left.x(),
               quad->uv_bottom_right.y() - quad->uv_top_left.y());
  matrix.Translate(quad->uv_top_left.x(), quad->uv_top_left.y());
  ToGLMatrix(&gl_matrix[0], matrix);
  gl_->UniformMatrix4fv(current_program_->tex_matrix_location(), 1, false,
                        gl_matrix);

  SetShaderOpacity(quad->shared_quad_state->opacity);
  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(
        quad->shared_quad_state->mask_filter_info.rounded_corner_bounds(),
        current_frame()->window_matrix * current_frame()->projection_matrix);
  }
  gfx::Size texture_size = lock.size();
  gfx::RectF uv_visible_rect(quad->uv_top_left.x(), quad->uv_top_left.y(),
                             quad->uv_bottom_right.x() - quad->uv_top_left.x(),
                             quad->uv_bottom_right.y() - quad->uv_top_left.y());
  const SamplerType sampler = SamplerTypeFromTextureTarget(lock.target());
  Float4 tex_clamp_rect = UVClampRect(uv_visible_rect, texture_size, sampler);
  gl_->Uniform4f(current_program_->tex_clamp_rect_location(),
                 tex_clamp_rect.data[0], tex_clamp_rect.data[1],
                 tex_clamp_rect.data[2], tex_clamp_rect.data[3]);

  auto tile_rect = gfx::RectF(quad->rect);

  if (!clip_region && quad->rect == quad->visible_rect) {
    DrawQuadGeometry(current_frame()->projection_matrix,
                     quad->shared_quad_state->quad_to_target_transform,
                     tile_rect);
  } else {
    gfx::QuadF region_quad =
        clip_region ? *clip_region : gfx::QuadF(gfx::RectF(quad->visible_rect));
    float uvs[8] = {0};
    GetScaledUVs(quad->rect, &region_quad, uvs);
    region_quad.Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());
    region_quad -= gfx::Vector2dF(0.5f, 0.5f);
    DrawQuadGeometryClippedByQuadF(
        quad->shared_quad_state->quad_to_target_transform, tile_rect,
        region_quad, uvs);
  }

  AccumulateDrawRects(quad->visible_rect,
                      quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);
}

void GLRenderer::FlushTextureQuadCache(BoundGeometry flush_binding) {
  // Check to see if we have anything to draw.
  if (draw_cache_.is_empty)
    return;
  ScopedTimerQuery scoped_timer_query(CompositeTimeTracingEnabled(), gl_,
                                      &timer_queries_, "kTextureContentFlush");

  PrepareGeometry(flush_binding);

  // Set the correct blending mode.
  SetBlendEnabled(draw_cache_.needs_blending);

  // Assume the current active textures is 0.
  DisplayResourceProviderGL::ScopedSamplerGL locked_quad(
      resource_provider(), draw_cache_.resource_id,
      draw_cache_.nearest_neighbor ? GL_NEAREST : GL_LINEAR);

  // Bind the program to the GL state.
  SetUseProgram(draw_cache_.program_key, locked_quad.color_space(),
                CurrentRenderPassColorSpace(),
                /*adjust_src_white_level=*/draw_cache_.is_video_frame);

  if (current_program_->rounded_corner_rect_location() != -1) {
    SetShaderRoundedCorner(
        draw_cache_.mask_filter_info.rounded_corner_bounds(),
        current_frame()->window_matrix * current_frame()->projection_matrix);
  }

  DCHECK_EQ(GL_TEXTURE0, GetActiveTextureUnit(gl_));
  gl_->BindTexture(locked_quad.target(), locked_quad.texture_id());

  static_assert(sizeof(Float4) == 4 * sizeof(float),
                "Float4 struct should be densely packed");
  static_assert(sizeof(Float16) == 16 * sizeof(float),
                "Float16 struct should be densely packed");

  // Upload the tranforms for both points and uvs.
  gl_->UniformMatrix4fv(
      current_program_->matrix_location(),
      static_cast<int>(draw_cache_.matrix_data.size()), false,
      reinterpret_cast<float*>(&draw_cache_.matrix_data.front()));
  gl_->Uniform4fv(current_program_->vertex_tex_transform_location(),
                  static_cast<int>(draw_cache_.uv_xform_data.size()),
                  reinterpret_cast<float*>(&draw_cache_.uv_xform_data.front()));

  if (current_program_->tint_color_matrix_location() != -1) {
    auto matrix = cc::DebugColors::TintCompositedContentColorTransformMatrix();
    gl_->UniformMatrix4fv(current_program_->tint_color_matrix_location(), 1,
                          false, matrix.data());
  }

  if (current_program_->tex_clamp_rect_location() != -1) {
    // Draw batching is not allowed with texture clamping.
    DCHECK_EQ(1u, draw_cache_.matrix_data.size());
    gl_->Uniform4f(current_program_->tex_clamp_rect_location(),
                   draw_cache_.tex_clamp_rect_data.data[0],
                   draw_cache_.tex_clamp_rect_data.data[1],
                   draw_cache_.tex_clamp_rect_data.data[2],
                   draw_cache_.tex_clamp_rect_data.data[3]);
  }

  if (draw_cache_.background_color != SK_ColorTRANSPARENT) {
    Float4 background_color =
        PremultipliedColor(draw_cache_.background_color, 1.f);
    gl_->Uniform4fv(current_program_->background_color_location(), 1,
                    background_color.data);
  }

  gl_->Uniform1fv(
      current_program_->vertex_opacity_location(),
      static_cast<int>(draw_cache_.vertex_opacity_data.size()),
      static_cast<float*>(&draw_cache_.vertex_opacity_data.front()));

  DCHECK_LE(draw_cache_.matrix_data.size(),
            static_cast<size_t>(std::numeric_limits<int>::max()) / 6u);

  // Draw the quads!
  gl_->DrawElements(GL_TRIANGLES,
                    6 * static_cast<int>(draw_cache_.matrix_data.size()),
                    GL_UNSIGNED_SHORT, nullptr);
  num_triangles_drawn_ += 2 * static_cast<int>(draw_cache_.matrix_data.size());

  // Clear the cache.
  draw_cache_.is_empty = true;
  draw_cache_.resource_id = kInvalidResourceId;
  draw_cache_.uv_xform_data.resize(0);
  draw_cache_.vertex_opacity_data.resize(0);
  draw_cache_.matrix_data.resize(0);
  draw_cache_.tex_clamp_rect_data = Float4();
  draw_cache_.is_video_frame = false;

  // If we had a clipped binding, prepare the shared binding for the
  // next inserts.
  if (flush_binding == CLIPPED_BINDING) {
    PrepareGeometry(SHARED_BINDING);
  }
}

void GLRenderer::EnqueueTextureQuad(const TextureDrawQuad* quad,
                                    const gfx::QuadF* clip_region) {
  // If we have a clip_region then we have to render the next quad
  // with dynamic geometry, therefore we must flush all pending
  // texture quads.
  if (clip_region) {
    // We send in false here because we want to flush what's currently in the
    // queue using the shared_geometry and not clipped_geometry
    FlushTextureQuadCache(SHARED_BINDING);
  }

  DisplayResourceProviderGL::ScopedReadLockGL lock(resource_provider(),
                                                   quad->resource_id());
  // ScopedReadLockGL contains the correct texture size, even when
  // quad->resource_size_in_pixels() is empty.
  const gfx::Size texture_size = lock.size();
  TexCoordPrecision tex_coord_precision =
      TexCoordPrecisionRequired(gl_, &highp_threshold_cache_,
                                settings_->highp_threshold_min, texture_size);

  const SamplerType sampler = SamplerTypeFromTextureTarget(lock.target());

  bool need_tex_clamp_rect = !quad->resource_size_in_pixels().IsEmpty() &&
                             (quad->uv_top_left != gfx::PointF(0, 0) ||
                              quad->uv_bottom_right != gfx::PointF(1, 1));

  ProgramKey program_key = ProgramKey::Texture(
      tex_coord_precision, sampler,
      quad->premultiplied_alpha ? PREMULTIPLIED_ALPHA : NON_PREMULTIPLIED_ALPHA,
      quad->background_color != SK_ColorTRANSPARENT, need_tex_clamp_rect,
      tint_gl_composited_content_, ShouldApplyRoundedCorner(quad));
  ResourceId resource_id = quad->resource_id();

  size_t max_quads = StaticGeometryBinding::NUM_QUADS;
  if (draw_cache_.is_empty || draw_cache_.program_key != program_key ||
      draw_cache_.resource_id != resource_id ||
      draw_cache_.needs_blending != quad->ShouldDrawWithBlending() ||
      draw_cache_.nearest_neighbor != quad->nearest_neighbor ||
      draw_cache_.background_color != quad->background_color ||
      draw_cache_.mask_filter_info !=
          quad->shared_quad_state->mask_filter_info ||
      draw_cache_.matrix_data.size() >= max_quads ||
      draw_cache_.is_video_frame != quad->is_video_frame) {
    FlushTextureQuadCache(SHARED_BINDING);
    draw_cache_.is_empty = false;
    draw_cache_.program_key = program_key;
    draw_cache_.resource_id = resource_id;
    draw_cache_.needs_blending = quad->ShouldDrawWithBlending();
    draw_cache_.nearest_neighbor = quad->nearest_neighbor;
    draw_cache_.background_color = quad->background_color;
    draw_cache_.mask_filter_info = quad->shared_quad_state->mask_filter_info;
    draw_cache_.is_video_frame = quad->is_video_frame;
  }

  // Generate the uv-transform
  auto uv_transform = UVTransform(quad);
  if (sampler == SAMPLER_TYPE_2D_RECT) {
    // Un-normalize the texture coordiantes for rectangle targets.
    uv_transform.data[0] *= texture_size.width();
    uv_transform.data[2] *= texture_size.width();
    uv_transform.data[1] *= texture_size.height();
    uv_transform.data[3] *= texture_size.height();
  }
  draw_cache_.uv_xform_data.push_back(uv_transform);

  if (need_tex_clamp_rect) {
    DCHECK_EQ(1u, draw_cache_.uv_xform_data.size());
    DCHECK_EQ(texture_size.ToString(),
              quad->resource_size_in_pixels().ToString());
    DCHECK(!texture_size.IsEmpty());
    gfx::RectF uv_visible_rect(
        quad->uv_top_left.x(), quad->uv_top_left.y(),
        quad->uv_bottom_right.x() - quad->uv_top_left.x(),
        quad->uv_bottom_right.y() - quad->uv_top_left.y());
    Float4 tex_clamp_rect = UVClampRect(uv_visible_rect, texture_size, sampler);
    draw_cache_.tex_clamp_rect_data = tex_clamp_rect;
  }

  // Generate the vertex opacity
  const float opacity = quad->shared_quad_state->opacity;
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[0] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[1] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[2] * opacity);
  draw_cache_.vertex_opacity_data.push_back(quad->vertex_opacity[3] * opacity);

  // Generate the transform matrix
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix,
                    quad->shared_quad_state->quad_to_target_transform,
                    gfx::RectF(quad->visible_rect));
  quad_rect_matrix = current_frame()->projection_matrix * quad_rect_matrix;

  Float16 m;
  quad_rect_matrix.matrix().asColMajorf(m.data);
  draw_cache_.matrix_data.push_back(m);

  // Track the region in the current target surface that has been drawn to.
  AccumulateDrawRects(quad->visible_rect,
                      quad->shared_quad_state->quad_to_target_transform,
                      &drawn_rects_);

  if (clip_region) {
    DCHECK_EQ(quad->rect, quad->visible_rect);
    gfx::QuadF scaled_region;
    if (!GetScaledRegion(quad->rect, clip_region, &scaled_region)) {
      scaled_region = SharedGeometryQuad().BoundingBox();
    }
    // Both the scaled region and the SharedGeomtryQuad are in the space
    // -0.5->0.5. We need to move that to the space 0->1.
    float uv[8];
    uv[0] = scaled_region.p1().x() + 0.5f;
    uv[1] = scaled_region.p1().y() + 0.5f;
    uv[2] = scaled_region.p2().x() + 0.5f;
    uv[3] = scaled_region.p2().y() + 0.5f;
    uv[4] = scaled_region.p3().x() + 0.5f;
    uv[5] = scaled_region.p3().y() + 0.5f;
    uv[6] = scaled_region.p4().x() + 0.5f;
    uv[7] = scaled_region.p4().y() + 0.5f;
    PrepareGeometry(CLIPPED_BINDING);
    clipped_geometry_->InitializeCustomQuadWithUVs(scaled_region, uv);
    FlushTextureQuadCache(CLIPPED_BINDING);
  } else if (need_tex_clamp_rect) {
    FlushTextureQuadCache(SHARED_BINDING);
  }
}

void GLRenderer::FinishDrawingFrame() {
  if (use_sync_query_) {
    sync_queries_.EndCurrentFrame();
  }

  swap_buffer_rect_.Union(current_frame()->root_damage_rect);

  if (use_swap_with_bounds_)
    swap_content_bounds_ = current_frame()->root_content_bounds;

  copier_.FreeUnusedCachedResources();

  current_framebuffer_texture_ = nullptr;

  gl_->Disable(GL_BLEND);
  blend_shadow_ = false;

  // Schedule output surface as overlay first to preserve existing ordering
  // semantics during overlay refactoring.
  ScheduleOutputSurfaceAsOverlay();

#if defined(OS_ANDROID) || defined(USE_OZONE)
  bool schedule_overlays = true;
#if defined(USE_X11)
  schedule_overlays = features::IsUsingOzonePlatform();
#endif
  if (schedule_overlays)
    ScheduleOverlays();
#elif defined(OS_APPLE)
  ScheduleCALayers();
#elif defined(OS_WIN)
  ScheduleDCLayers();
#endif

  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.triangles"), "Triangles Drawn",
                 num_triangles_drawn_);

  // Mark the end of batched read of shared images.
  gl_->EndBatchReadAccessSharedImageCHROMIUM();
}

bool GLRenderer::OverdrawTracingEnabled() {
  // Only collect trace data if we select viz.overdraw.
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("viz.overdraw"),
                                     &tracing_enabled);
  // ARB_occlusion_query is required for tracing.
  // Trace only the root render pass.
  return tracing_enabled && use_occlusion_query_ &&
         current_frame()->current_render_pass ==
             current_frame()->root_render_pass;
}

bool GLRenderer::CompositeTimeTracingEnabled() {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      TRACE_DISABLED_BY_DEFAULT("viz.gpu_composite_time"), &tracing_enabled);

  return tracing_enabled && use_timer_query_;
}

void GLRenderer::AddCompositeTimeTraces(base::TimeTicks ready_timestamp) {
  DCHECK(CompositeTimeTracingEnabled());
  DCHECK_EQ(timer_queries_.front().first, kTimerQueryDummy);

  std::size_t count = 0;
  uint64_t duration = 0;

  // List of queries to delete after their results are retrieved.
  std::vector<unsigned> queries_to_delete;

  // Queue of durations per draw call. The |second| in the pair represents the
  // draw call type as string.
  base::queue<std::pair<uint64_t, std::string>> durations;

  // Pop the fence query as it does not represent a timer query.
  timer_queries_.pop();

  // Initialize |start_time_ticks| as the end timestamp and walk backwards to
  // find the actual timestamp.
  base::TimeTicks start_time_ticks = ready_timestamp;

  while (timer_queries_.size() &&
         timer_queries_.front().first != kTimerQueryDummy) {
    count++;
    gl_->GetQueryObjectui64vEXT(timer_queries_.front().first,
                                GL_QUERY_RESULT_EXT, &duration);
    durations.emplace(duration, timer_queries_.front().second);
    queries_to_delete.push_back(timer_queries_.front().first);
    timer_queries_.pop();
    start_time_ticks -= base::TimeDelta::FromNanoseconds(duration);
  }

  // Delete all timer queries for which results have been retrieved.
  gl_->DeleteQueriesEXT(count, queries_to_delete.data());

  base::TimeDelta unique_id_delta = ready_timestamp - start_time_ticks;
  const int trace_unique_id = unique_id_delta.InMilliseconds() * count;

  TRACE_EVENT_ASYNC_BEGIN_WITH_TIMESTAMP0(
      TRACE_DISABLED_BY_DEFAULT("viz.gpu_composite_time"), "Composite Time",
      TRACE_ID_LOCAL(trace_unique_id), start_time_ticks);

  while (!durations.empty()) {
    duration = durations.front().first;

    // |duration| may be set to 0 if the timer query result was unavailable in
    // |GetQueryObjectui64vEXT| function call.
    if (!duration) {
      durations.pop();
      continue;
    }
    TRACE_EVENT_ASYNC_STEP_INTO_WITH_TIMESTAMP0(
        TRACE_DISABLED_BY_DEFAULT("viz.gpu_composite_time"), "Composite Time",
        TRACE_ID_LOCAL(trace_unique_id), durations.front().second.c_str(),
        start_time_ticks);
    start_time_ticks += base::TimeDelta::FromNanoseconds(duration);
    durations.pop();
  }

  TRACE_EVENT_ASYNC_END_WITH_TIMESTAMP0(
      TRACE_DISABLED_BY_DEFAULT("viz.gpu_composite_time"), "Composite Time",
      TRACE_ID_LOCAL(trace_unique_id), ready_timestamp);
}

void GLRenderer::FinishDrawingQuadList() {
  FlushTextureQuadCache(SHARED_BINDING);
  if (occlusion_query_) {
    // Use the current surface area as max result. The effect is that overdraw
    // is reported as a percentage of the output surface size. ie. 2x overdraw
    // for the whole screen is reported as 200.
    base::CheckedNumeric<int> surface_area =
        current_surface_size_.GetCheckedArea();
    DCHECK_GT(static_cast<int>(surface_area.ValueOrDefault(INT_MAX)), 0);

    gl_->EndQueryEXT(GL_SAMPLES_PASSED_ARB);
    context_support_->SignalQuery(
        occlusion_query_, base::BindOnce(&GLRenderer::ProcessOverdrawFeedback,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         surface_area, occlusion_query_));
    occlusion_query_ = 0;
  }
}

void GLRenderer::GenerateMipmap() {
  DCHECK(current_framebuffer_texture_);
  current_framebuffer_texture_->set_generate_mipmap();
}

bool GLRenderer::FlippedFramebuffer() const {
#if defined(OS_APPLE)
  if (force_drawing_frame_framebuffer_unflipped_)
    return false;
#endif
  if (current_frame()->current_render_pass != current_frame()->root_render_pass)
    return true;
  return FlippedRootFramebuffer();
}

bool GLRenderer::FlippedRootFramebuffer() const {
  // GL is normally flipped, so a flipped output results in an unflipping.
  return output_surface_->capabilities().output_surface_origin ==
         gfx::SurfaceOrigin::kBottomLeft;
}

void GLRenderer::EnsureScissorTestEnabled() {
  if (is_scissor_enabled_)
    return;

  FlushTextureQuadCache(SHARED_BINDING);
  gl_->Enable(GL_SCISSOR_TEST);
  is_scissor_enabled_ = true;
}

void GLRenderer::EnsureScissorTestDisabled() {
  if (!is_scissor_enabled_)
    return;

  FlushTextureQuadCache(SHARED_BINDING);
  gl_->Disable(GL_SCISSOR_TEST);
  is_scissor_enabled_ = false;
}

void GLRenderer::CopyDrawnRenderPass(
    const copy_output::RenderPassGeometry& geometry,
    std::unique_ptr<CopyOutputRequest> request) {
  TRACE_EVENT0("viz", "GLRenderer::CopyDrawnRenderPass");

  GLuint framebuffer_texture = 0;
  gfx::Size framebuffer_texture_size;
  if (current_framebuffer_texture_) {
    framebuffer_texture = current_framebuffer_texture_->id();
    framebuffer_texture_size = current_framebuffer_texture_->size();
  }
  copier_.CopyFromTextureOrFramebuffer(
      std::move(request), geometry, GetFramebufferCopyTextureFormat(),
      framebuffer_texture, framebuffer_texture_size, FlippedFramebuffer(),
      CurrentRenderPassColorSpace());

  // The copier modified texture/framebuffer bindings, shader programs, and
  // other GL state; and so this must be restored before continuing.
  RestoreGLState();

  // CopyDrawnRenderPass() can change the binding of the framebuffer target as
  // a part of its usual scaling and readback operations. It will break next
  // CopyDrawnRenderPass() call for the root render pass. Therefore, make sure
  // to restore the correct framebuffer between readbacks. (Even if it did
  // not, a Mac-specific bug requires this workaround: http://crbug.com/99393)
  const auto* render_pass = current_frame()->current_render_pass;
  if (render_pass == current_frame()->root_render_pass)
    BindFramebufferToOutputSurface();
}

void GLRenderer::ToGLMatrix(float* gl_matrix, const gfx::Transform& transform) {
  transform.matrix().asColMajorf(gl_matrix);
}

void GLRenderer::SetShaderQuadF(const gfx::QuadF& quad) {
  if (!current_program_ || current_program_->quad_location() == -1)
    return;
  float gl_quad[8];
  gl_quad[0] = quad.p1().x();
  gl_quad[1] = quad.p1().y();
  gl_quad[2] = quad.p2().x();
  gl_quad[3] = quad.p2().y();
  gl_quad[4] = quad.p3().x();
  gl_quad[5] = quad.p3().y();
  gl_quad[6] = quad.p4().x();
  gl_quad[7] = quad.p4().y();
  gl_->Uniform2fv(current_program_->quad_location(), 4, gl_quad);
}

void GLRenderer::SetShaderOpacity(float opacity) {
  if (!current_program_ || current_program_->alpha_location() == -1)
    return;
  gl_->Uniform1f(current_program_->alpha_location(), opacity);
}

void GLRenderer::SetShaderMatrix(const gfx::Transform& transform) {
  if (!current_program_ || current_program_->matrix_location() == -1)
    return;
  float gl_matrix[16];
  ToGLMatrix(gl_matrix, transform);
  gl_->UniformMatrix4fv(current_program_->matrix_location(), 1, false,
                        gl_matrix);
}

void GLRenderer::SetShaderColor(SkColor color, float opacity) {
  if (!current_program_ || current_program_->color_location() == -1)
    return;
  Float4 float_color = PremultipliedColor(color, opacity);
  gl_->Uniform4fv(current_program_->color_location(), 1, float_color.data);
}

void GLRenderer::SetStencilEnabled(bool enabled) {
  if (enabled == stencil_shadow_)
    return;

  if (enabled)
    gl_->Enable(GL_STENCIL_TEST);
  else
    gl_->Disable(GL_STENCIL_TEST);
  stencil_shadow_ = enabled;
}

void GLRenderer::SetBlendEnabled(bool enabled) {
  if (enabled == blend_shadow_)
    return;

  if (enabled)
    gl_->Enable(GL_BLEND);
  else
    gl_->Disable(GL_BLEND);
  blend_shadow_ = enabled;
}

void GLRenderer::SetShaderRoundedCorner(
    const gfx::RRectF& rounded_corner_bounds,
    const gfx::Transform& screen_transform) {
  DCHECK(current_program_);
  DCHECK(!rounded_corner_bounds.IsEmpty());
  DCHECK_NE(current_program_->rounded_corner_rect_location(), -1);
  DCHECK_NE(current_program_->rounded_corner_radius_location(), -1);
  DCHECK(screen_transform.IsScaleOrTranslation());

  const gfx::Vector2dF& translate = screen_transform.To2dTranslation();
  const gfx::Vector2dF& scale = screen_transform.Scale2d();
  gfx::RRectF bounds_in_screen = rounded_corner_bounds;
  bounds_in_screen.Scale(scale.x(), scale.y());
  bounds_in_screen.Offset(translate.x(), translate.y());

  gfx::RectF rect = bounds_in_screen.rect();

  gl_->Uniform4f(current_program_->rounded_corner_rect_location(), rect.x(),
                 rect.y(), rect.width(), rect.height());
  gl_->Uniform4f(
      current_program_->rounded_corner_radius_location(),
      bounds_in_screen.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x(),
      bounds_in_screen.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x(),
      bounds_in_screen.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x(),
      bounds_in_screen.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());
}

void GLRenderer::DrawQuadGeometryClippedByQuadF(
    const gfx::Transform& draw_transform,
    const gfx::RectF& quad_rect,
    const gfx::QuadF& clipping_region_quad,
    const float* uvs) {
  PrepareGeometry(CLIPPED_BINDING);
  if (uvs) {
    clipped_geometry_->InitializeCustomQuadWithUVs(clipping_region_quad, uvs);
  } else {
    clipped_geometry_->InitializeCustomQuad(clipping_region_quad);
  }
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, draw_transform, quad_rect);
  SetShaderMatrix(current_frame()->projection_matrix * quad_rect_matrix);

  gl_->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT,
                    reinterpret_cast<const void*>(0));
  num_triangles_drawn_ += 2;
}

void GLRenderer::DrawQuadGeometry(const gfx::Transform& projection_matrix,
                                  const gfx::Transform& draw_transform,
                                  const gfx::RectF& quad_rect) {
  PrepareGeometry(SHARED_BINDING);
  gfx::Transform quad_rect_matrix;
  QuadRectTransform(&quad_rect_matrix, draw_transform, quad_rect);
  SetShaderMatrix(projection_matrix * quad_rect_matrix);

  gl_->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
  num_triangles_drawn_ += 2;
}

void GLRenderer::DrawQuadGeometryWithAA(const DrawQuad* quad,
                                        gfx::QuadF* local_quad,
                                        const gfx::Rect& tile_rect) {
  DCHECK(local_quad);
  // Normalize to tile_rect.
  local_quad->Scale(1.0f / tile_rect.width(), 1.0f / tile_rect.height());

  SetShaderQuadF(*local_quad);

  // The transform and vertex data are used to figure out the extents that the
  // un-antialiased quad should have and which vertex this is and the float
  // quad passed in via uniform is the actual geometry that gets used to draw
  // it. This is why this centered rect is used and not the original quad_rect.
  DrawQuadGeometry(current_frame()->projection_matrix,
                   quad->shared_quad_state->quad_to_target_transform,
                   CenteredRect(tile_rect));
}

void GLRenderer::SwapBuffers(SwapFrameData swap_frame_data) {
  DCHECK(visible_);

  TRACE_EVENT0("viz", "GLRenderer::SwapBuffers");
  // We're done! Time to swapbuffers!

  gfx::Size surface_size = surface_size_for_swap_buffers();

  OutputSurfaceFrame output_frame;
  output_frame.latency_info = std::move(swap_frame_data.latency_info);
  output_frame.top_controls_visible_height_changed =
      swap_frame_data.top_controls_visible_height_changed;
  output_frame.size = surface_size;
  if (use_swap_with_bounds_) {
    output_frame.content_bounds = std::move(swap_content_bounds_);
  } else if (use_partial_swap_) {
    // If supported, we can save significant bandwidth by only swapping the
    // damaged/scissored region (clamped to the viewport).
    swap_buffer_rect_.Intersect(gfx::Rect(surface_size));
    int flipped_y_pos_of_rect_bottom = surface_size.height() -
                                       swap_buffer_rect_.y() -
                                       swap_buffer_rect_.height();
    output_frame.sub_buffer_rect =
        gfx::Rect(swap_buffer_rect_.x(),
                  FlippedRootFramebuffer() ? flipped_y_pos_of_rect_bottom
                                           : swap_buffer_rect_.y(),
                  swap_buffer_rect_.width(), swap_buffer_rect_.height());
  } else if (swap_buffer_rect_.IsEmpty() && allow_empty_swap_) {
    output_frame.sub_buffer_rect = swap_buffer_rect_;
  }

  // Record resources from viz clients that have been shipped as overlays to the
  // gpu together.
  swapping_overlay_resources_.push_back(std::move(pending_overlay_resources_));
  pending_overlay_resources_.clear();
  if (settings_->release_overlay_resources_after_gpu_query) {
    // Record RenderPass textures that have been shipped as overlays to the gpu
    // together.
    displayed_overlay_textures_.push_back(
        std::move(awaiting_swap_overlay_textures_));
    awaiting_swap_overlay_textures_.clear();
  } else {
    // If |displayed_overlay_textures_| is appended to in this case then
    // SwapBuffersComplete needs to be extended to handle it.
    DCHECK(awaiting_swap_overlay_textures_.empty());
  }

  output_surface_->SwapBuffers(std::move(output_frame));

  swap_buffer_rect_ = gfx::Rect();

  if (context_busy_) {
    output_surface_->context_provider()->CacheController()->ClientBecameNotBusy(
        std::move(context_busy_));
  }
}

void GLRenderer::SwapBuffersSkipped() {
  if (context_busy_) {
    output_surface_->context_provider()->CacheController()->ClientBecameNotBusy(
        std::move(context_busy_));
  }
}

void GLRenderer::SwapBuffersComplete() {
  if (settings_->release_overlay_resources_after_gpu_query) {
    // Once a resource has been swap-ACKed, send a query to the GPU process to
    // ask if the resource is no longer being consumed by the system compositor.
    // The response will come with the next swap-ACK.
    if (!swapping_overlay_resources_.empty()) {
      for (OverlayResourceLock& lock : swapping_overlay_resources_.front()) {
        unsigned texture = lock->texture_id();
        if (swapped_and_acked_overlay_resources_.find(texture) ==
            swapped_and_acked_overlay_resources_.end()) {
          swapped_and_acked_overlay_resources_[texture] = std::move(lock);
        }
      }
      swapping_overlay_resources_.pop_front();
    }
    if (!displayed_overlay_textures_.empty()) {
      for (auto& overlay : displayed_overlay_textures_.front())
        awaiting_release_overlay_textures_.push_back(std::move(overlay));
      displayed_overlay_textures_.erase(displayed_overlay_textures_.begin());
    }

    size_t query_texture_count = swapped_and_acked_overlay_resources_.size() +
                                 awaiting_release_overlay_textures_.size();
    if (query_texture_count) {
      std::vector<uint32_t> query_texture_ids;
      query_texture_ids.reserve(query_texture_count);

      for (auto& pair : swapped_and_acked_overlay_resources_)
        query_texture_ids.push_back(pair.first);
      for (auto& overlay : awaiting_release_overlay_textures_)
        query_texture_ids.push_back(overlay->texture.id());

      // We query for *all* outstanding texture ids, even if we previously
      // queried, as we will not hear back about things becoming available
      // until after we query again.
      gl_->ScheduleCALayerInUseQueryCHROMIUM(query_texture_count,
                                             query_texture_ids.data());
    }
  } else {
    // If a query is not needed to release the overlay buffers, we can assume
    // that once a swap buffer has completed we can remove the oldest buffers
    // from the queue, but only once we've swapped another frame afterward.
    if (swapping_overlay_resources_.size() > 1) {
      DisplayResourceProviderGL::ScopedBatchReturnResources returner(
          resource_provider());
      swapping_overlay_resources_.pop_front();
    }
    // If |displayed_overlay_textures_| has a non-empty member that means we're
    // sending RenderPassDrawQuads as an overlay. This is only supported for
    // CALayers now, where |release_overlay_resources_after_gpu_query| will be
    // true. In order to support them here, the OverlayTextures would need to
    // move to |awaiting_release_overlay_textures_| and stay there until the
    // ResourceFence that was in use for the frame they were submitted is
    // passed.
    DCHECK(displayed_overlay_textures_.empty());
  }
}

void GLRenderer::DidReceiveTextureInUseResponses(
    const gpu::TextureInUseResponses& responses) {
  DCHECK(settings_->release_overlay_resources_after_gpu_query);
  DisplayResourceProviderGL::ScopedBatchReturnResources returner(
      resource_provider());
  for (const gpu::TextureInUseResponse& response : responses) {
    if (response.in_use)
      continue;

    // Returned texture ids may be for resources from clients of the
    // display compositor, in |swapped_and_acked_overlay_resources_|. In that
    // case we remove the lock from the map, allowing them to be returned to the
    // client if the resource has been deleted from the
    // DisplayResourceProviderGL.
    if (swapped_and_acked_overlay_resources_.erase(response.texture))
      continue;
    // If not, then they would be a RenderPass copy texture, which is held in
    // |awaiting_release_overlay_textures_|. We move it back to the available
    // texture list to use it for the next frame.
    auto it = std::find_if(
        awaiting_release_overlay_textures_.begin(),
        awaiting_release_overlay_textures_.end(),
        [&response](const std::unique_ptr<OverlayTexture>& overlay) {
          return overlay->texture.id() == response.texture;
        });
    if (it != awaiting_release_overlay_textures_.end()) {
      // Mark the OverlayTexture as newly returned to the available set.
      (*it)->frames_waiting_for_reuse = 0;
      available_overlay_textures_.push_back(std::move(*it));
      awaiting_release_overlay_textures_.erase(it);
    }
  }
}

void GLRenderer::BindFramebufferToOutputSurface() {
  current_framebuffer_texture_ = nullptr;
  output_surface_->BindFramebuffer();
  tint_gl_composited_content_ = debug_settings_->tint_composited_content;
  if (overdraw_feedback_) {
    // Output surfaces that require an external stencil test should not allow
    // overdraw feedback by setting |supports_stencil| to false.
    DCHECK(!output_surface_->HasExternalStencilTest());
    SetupOverdrawFeedback();
    SetStencilEnabled(true);
  } else if (output_surface_->HasExternalStencilTest()) {
    output_surface_->ApplyExternalStencil();
    SetStencilEnabled(true);
  } else {
    SetStencilEnabled(false);
  }
}

void GLRenderer::BindFramebufferToTexture(
    const AggregatedRenderPassId render_pass_id) {
  tint_gl_composited_content_ = false;
  gl_->BindFramebuffer(GL_FRAMEBUFFER, offscreen_framebuffer_id_);

  auto contents_texture_it = render_pass_textures_.find(render_pass_id);
  current_framebuffer_texture_ = &contents_texture_it->second;
  GLuint texture_id = current_framebuffer_texture_->id();
  DCHECK(texture_id);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                            texture_id, 0);
  if (overdraw_feedback_) {
    if (!offscreen_stencil_renderbuffer_id_)
      gl_->GenRenderbuffers(1, &offscreen_stencil_renderbuffer_id_);
    if (current_framebuffer_texture_->size() !=
        offscreen_stencil_renderbuffer_size_) {
      gl_->BindRenderbuffer(GL_RENDERBUFFER,
                            offscreen_stencil_renderbuffer_id_);
      gl_->RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                               current_framebuffer_texture_->size().width(),
                               current_framebuffer_texture_->size().height());
      gl_->BindRenderbuffer(GL_RENDERBUFFER, 0);
      offscreen_stencil_renderbuffer_size_ =
          current_framebuffer_texture_->size();
    }
    gl_->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                 GL_RENDERBUFFER,
                                 offscreen_stencil_renderbuffer_id_);
  }

  DCHECK(gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
             GL_FRAMEBUFFER_COMPLETE ||
         IsContextLost());

  if (overdraw_feedback_) {
    SetupOverdrawFeedback();
    SetStencilEnabled(true);
  } else {
    SetStencilEnabled(false);
  }
}

void GLRenderer::SetScissorTestRect(const gfx::Rect& scissor_rect) {
  EnsureScissorTestEnabled();

  // Don't unnecessarily ask the context to change the scissor, because it
  // may cause undesired GPU pipeline flushes.
  if (scissor_rect == scissor_rect_)
    return;

  scissor_rect_ = scissor_rect;
  FlushTextureQuadCache(SHARED_BINDING);
  gl_->Scissor(scissor_rect.x(), scissor_rect.y(), scissor_rect.width(),
               scissor_rect.height());
}

void GLRenderer::SetViewport() {
  gl_->Viewport(current_window_space_viewport_.x(),
                current_window_space_viewport_.y(),
                current_window_space_viewport_.width(),
                current_window_space_viewport_.height());
}

void GLRenderer::InitializeSharedObjects() {
  TRACE_EVENT0("viz", "GLRenderer::InitializeSharedObjects");

  // Create an FBO for doing offscreen rendering.
  gl_->GenFramebuffers(1, &offscreen_framebuffer_id_);

  shared_geometry_ =
      std::make_unique<StaticGeometryBinding>(gl_, QuadVertexRect());
  clipped_geometry_ = std::make_unique<DynamicGeometryBinding>(gl_);
}

void GLRenderer::PrepareGeometry(BoundGeometry binding) {
  if (binding == bound_geometry_) {
    return;
  }

  switch (binding) {
    case SHARED_BINDING:
      shared_geometry_->PrepareForDraw();
      break;
    case CLIPPED_BINDING:
      clipped_geometry_->PrepareForDraw();
      break;
    case NO_BINDING:
      break;
  }
  bound_geometry_ = binding;
}

void GLRenderer::SetUseProgram(const ProgramKey& program_key_no_color,
                               const gfx::ColorSpace& src_color_space,
                               const gfx::ColorSpace& dst_color_space,
                               bool adjust_src_white_level) {
  DCHECK(dst_color_space.IsValid());
  gfx::ColorSpace adjusted_src_color_space = src_color_space;
  if (adjust_src_white_level) {
    // If the input color space is HDR, and it did not specify a white level,
    // override it with the frame's white level.
    adjusted_src_color_space = src_color_space.GetWithSDRWhiteLevel(
        current_frame()->display_color_spaces.GetSDRWhiteLevel());
  }

  ProgramKey program_key = program_key_no_color;
  const gfx::ColorTransform* color_transform =
      GetColorTransform(adjusted_src_color_space, dst_color_space);
  program_key.SetColorTransform(color_transform);

  bool has_output_color_matrix = false;
  if (program_key.type() != ProgramType::PROGRAM_TYPE_SOLID_COLOR)
    has_output_color_matrix = HasOutputColorMatrix();
  program_key.set_has_output_color_matrix(has_output_color_matrix);

  // Create and set the program if needed.
  std::unique_ptr<Program>& program = program_cache_[program_key];
  if (!program) {
    program.reset(new Program);
    program->Initialize(output_surface_->context_provider(), program_key);
  }
  DCHECK(program);
  if (current_program_ != program.get()) {
    current_program_ = program.get();
    gl_->UseProgram(current_program_->program());
  }
  if (!current_program_->initialized()) {
    DCHECK(IsContextLost());
    return;
  }

  // Set uniforms that are common to all programs.
  if (current_program_->sampler_location() != -1)
    gl_->Uniform1i(current_program_->sampler_location(), 0);
  if (current_program_->viewport_location() != -1) {
    float viewport[4] = {
        static_cast<float>(current_window_space_viewport_.x()),
        static_cast<float>(current_window_space_viewport_.y()),
        static_cast<float>(current_window_space_viewport_.width()),
        static_cast<float>(current_window_space_viewport_.height()),
    };
    gl_->Uniform4fv(current_program_->viewport_location(), 1, viewport);
  }

  if (has_output_color_matrix) {
    DCHECK_NE(current_program_->output_color_matrix_location(), -1);
    float matrix[16];
    output_surface_->color_matrix().asColMajorf(matrix);
    gl_->UniformMatrix4fv(current_program_->output_color_matrix_location(), 1,
                          false, matrix);
  }
}

const Program* GLRenderer::GetProgramIfInitialized(
    const ProgramKey& desc) const {
  const auto found = program_cache_.find(desc);
  if (found == program_cache_.end())
    return nullptr;
  return found->second.get();
}

const gfx::ColorTransform* GLRenderer::GetColorTransform(
    const gfx::ColorSpace& src,
    const gfx::ColorSpace& dst) {
  std::unique_ptr<gfx::ColorTransform>& transform =
      color_transform_cache_[dst][src];
  if (!transform) {
    transform = gfx::ColorTransform::NewColorTransform(
        src, dst, gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
  }
  return transform.get();
}

void GLRenderer::CleanupSharedObjects() {
  shared_geometry_ = nullptr;

  gl_->ReleaseShaderCompiler();
  for (auto& iter : program_cache_)
    iter.second->Cleanup(gl_);
  program_cache_.clear();
  color_transform_cache_.clear();

  if (offscreen_framebuffer_id_)
    gl_->DeleteFramebuffers(1, &offscreen_framebuffer_id_);

  if (offscreen_stencil_renderbuffer_id_)
    gl_->DeleteRenderbuffers(1, &offscreen_stencil_renderbuffer_id_);
}

void GLRenderer::ReinitializeGLState() {
  is_scissor_enabled_ = false;
  scissor_rect_ = gfx::Rect();
  stencil_shadow_ = false;
  blend_shadow_ = true;
  current_program_ = nullptr;

  RestoreGLState();
}

void GLRenderer::RestoreGLStateAfterSkia() {
  // After using Skia we need to disable vertex attributes we don't use
  int attribs_count = output_surface_->context_provider()
                          ->ContextCapabilities()
                          .max_vertex_attribs;
  for (int i = 0; i < attribs_count; i++)
    gl_->DisableVertexAttribArray(i);

  RestoreGLState();
}

void GLRenderer::RestoreGLState() {
  // This restores the current GLRenderer state to the GL context.
  bound_geometry_ = NO_BINDING;
  PrepareGeometry(SHARED_BINDING);

  gl_->Disable(GL_DEPTH_TEST);
  gl_->Disable(GL_CULL_FACE);
  gl_->ColorMask(true, true, true, true);
  gl_->BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  gl_->ActiveTexture(GL_TEXTURE0);

  if (current_program_)
    gl_->UseProgram(current_program_->program());

  if (stencil_shadow_)
    gl_->Enable(GL_STENCIL_TEST);
  else
    gl_->Disable(GL_STENCIL_TEST);

  if (blend_shadow_)
    gl_->Enable(GL_BLEND);
  else
    gl_->Disable(GL_BLEND);

  if (is_scissor_enabled_)
    gl_->Enable(GL_SCISSOR_TEST);
  else
    gl_->Disable(GL_SCISSOR_TEST);

  gl_->Scissor(scissor_rect_.x(), scissor_rect_.y(), scissor_rect_.width(),
               scissor_rect_.height());
}

bool GLRenderer::IsContextLost() {
  return gl_->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

#if defined(OS_APPLE)
void GLRenderer::ScheduleCALayers() {
  // The use of OverlayTextures for RenderPasses is only supported on the code
  // paths for |release_overlay_resources_after_gpu_query| at the moment. See
  // SwapBuffersComplete for notes on the missing support for other paths. This
  // method uses ScheduleRenderPassDrawQuad to send RenderPass outputs as
  // overlays, so it can only be used because this setting is true.
  if (!settings_->release_overlay_resources_after_gpu_query)
    return;

  scoped_refptr<CALayerOverlaySharedState> shared_state;
  size_t copied_render_pass_count = 0;

  for (const CALayerOverlay& ca_layer_overlay : current_frame()->overlay_list) {
    if (ca_layer_overlay.rpdq) {
      std::unique_ptr<OverlayTexture> overlay_texture =
          ScheduleRenderPassDrawQuad(&ca_layer_overlay);
      if (overlay_texture)
        awaiting_swap_overlay_textures_.push_back(std::move(overlay_texture));
      shared_state = nullptr;
      ++copied_render_pass_count;
      continue;
    }

    ResourceId contents_resource_id = ca_layer_overlay.contents_resource_id;
    unsigned texture_id = 0;
    if (contents_resource_id) {
      pending_overlay_resources_.push_back(
          std::make_unique<DisplayResourceProviderGL::ScopedOverlayLockGL>(
              resource_provider(), contents_resource_id));
      texture_id = pending_overlay_resources_.back()->texture_id();
    }
    GLfloat contents_rect[4] = {
        ca_layer_overlay.contents_rect.x(), ca_layer_overlay.contents_rect.y(),
        ca_layer_overlay.contents_rect.width(),
        ca_layer_overlay.contents_rect.height(),
    };
    GLfloat bounds_rect[4] = {
        ca_layer_overlay.bounds_rect.x(), ca_layer_overlay.bounds_rect.y(),
        ca_layer_overlay.bounds_rect.width(),
        ca_layer_overlay.bounds_rect.height(),
    };
    GLboolean is_clipped = ca_layer_overlay.shared_state->is_clipped;
    GLfloat clip_rect[4] = {ca_layer_overlay.shared_state->clip_rect.x(),
                            ca_layer_overlay.shared_state->clip_rect.y(),
                            ca_layer_overlay.shared_state->clip_rect.width(),
                            ca_layer_overlay.shared_state->clip_rect.height()};

    const gfx::RectF& rect =
        ca_layer_overlay.shared_state->rounded_corner_bounds.rect();
    GLfloat rounded_corner_bounds[5] = {
        rect.x(), rect.y(), rect.width(), rect.height(),
        ca_layer_overlay.shared_state->rounded_corner_bounds.GetSimpleRadius()};

    GLint sorting_context_id =
        ca_layer_overlay.shared_state->sorting_context_id;
    GLfloat transform[16];
    ca_layer_overlay.shared_state->transform.asColMajorf(transform);
    unsigned filter = ca_layer_overlay.filter;

    if (ca_layer_overlay.shared_state != shared_state) {
      shared_state = ca_layer_overlay.shared_state;
      gl_->ScheduleCALayerSharedStateCHROMIUM(
          ca_layer_overlay.shared_state->opacity, is_clipped, clip_rect,
          rounded_corner_bounds, sorting_context_id, transform);
    }
    gl_->ScheduleCALayerCHROMIUM(
        texture_id, contents_rect, ca_layer_overlay.background_color,
        ca_layer_overlay.edge_aa_mask, bounds_rect, filter);
  }

  ReduceAvailableOverlayTextures();
}
#endif  // defined(OS_APPLE)

#if defined(OS_WIN)
void GLRenderer::ScheduleDCLayers() {
  for (DCLayerOverlay& dc_layer_overlay : current_frame()->overlay_list) {
    DCHECK_EQ(DCLayerOverlay::kNumResources, 2u);
    GLuint texture_ids[DCLayerOverlay::kNumResources] = {};
    for (size_t i = 0; i < DCLayerOverlay::kNumResources; i++) {
      ResourceId resource_id = dc_layer_overlay.resources[i];
      if (resource_id == kInvalidResourceId)
        break;
      pending_overlay_resources_.push_back(
          std::make_unique<DisplayResourceProviderGL::ScopedOverlayLockGL>(
              resource_provider(), resource_id));
      texture_ids[i] = pending_overlay_resources_.back()->texture_id();
    }
    DCHECK(texture_ids[0]);
    // TODO(sunnyps): Set color space in renderer like we do for tiles.
    gl_->SetColorSpaceMetadataCHROMIUM(
        texture_ids[0], dc_layer_overlay.color_space.AsGLColorSpace());

    int z_order = dc_layer_overlay.z_order;
    const gfx::Rect& content_rect = dc_layer_overlay.content_rect;
    const gfx::Rect& quad_rect = dc_layer_overlay.quad_rect;
    DCHECK(dc_layer_overlay.transform.IsFlat());
    const SkMatrix44& transform = dc_layer_overlay.transform.matrix();
    bool is_clipped = dc_layer_overlay.is_clipped;
    const gfx::Rect& clip_rect = dc_layer_overlay.clip_rect;
    unsigned protected_video_type =
        static_cast<unsigned>(dc_layer_overlay.protected_video_type);

    gl_->ScheduleDCLayerCHROMIUM(
        texture_ids[0], texture_ids[1], z_order, content_rect.x(),
        content_rect.y(), content_rect.width(), content_rect.height(),
        quad_rect.x(), quad_rect.y(), quad_rect.width(), quad_rect.height(),
        transform.get(0, 0), transform.get(0, 1), transform.get(1, 0),
        transform.get(1, 1), transform.get(0, 3), transform.get(1, 3),
        is_clipped, clip_rect.x(), clip_rect.y(), clip_rect.width(),
        clip_rect.height(), protected_video_type);
  }
}
#endif  // defined (OS_WIN)

#if defined(OS_ANDROID) || defined(USE_OZONE)
void GLRenderer::ScheduleOverlays() {
  if (current_frame()->overlay_list.empty())
    return;

  OverlayCandidateList& overlays = current_frame()->overlay_list;
  for (const auto& overlay_candidate : overlays) {
    pending_overlay_resources_.push_back(
        std::make_unique<DisplayResourceProviderGL::ScopedOverlayLockGL>(
            resource_provider(), overlay_candidate.resource_id));
    unsigned texture_id = pending_overlay_resources_.back()->texture_id();

    context_support_->ScheduleOverlayPlane(
        overlay_candidate.plane_z_order, overlay_candidate.transform,
        texture_id, ToNearestRect(overlay_candidate.display_rect),
        overlay_candidate.uv_rect, !overlay_candidate.is_opaque,
        overlay_candidate.gpu_fence_id);
  }
}
#endif  // defined(OS_ANDROID) || defined(USE_OZONE)

void GLRenderer::ScheduleOutputSurfaceAsOverlay() {
  if (!current_frame()->output_surface_plane)
    return;

  // Initialize correct values to use an output surface as overlay candidate.
  auto& overlay_candidate = *(current_frame()->output_surface_plane);
  unsigned texture_id = output_surface_->GetOverlayTextureId();
  DCHECK(texture_id || IsContextLost());
  // Output surface is also z-order 0.
  int plane_z_order = 0;
  // Output surface always uses the full texture.
  gfx::RectF uv_rect(0.f, 0.f, 1.f, 1.f);

  context_support_->ScheduleOverlayPlane(
      plane_z_order, overlay_candidate.transform, texture_id,
      ToNearestRect(overlay_candidate.display_rect), uv_rect,
      overlay_candidate.enable_blending, overlay_candidate.gpu_fence_id);
}

#if defined(OS_APPLE)
// This function draws the CompositorRenderPassDrawQuad into a temporary
// texture/framebuffer, and then copies the result into an IOSurface. The
// inefficient (but simple) way to do this would be to:
//   1. Allocate a framebuffer the size of the screen.
//   2. Draw using all the normal RPDQ draw logic.
//
// Instead, this method does the following:
//   1. Configure parameters as if drawing to a framebuffer the size of the
//   screen. This reuses most of the RPDQ draw logic.
//   2. Update parameters to draw into a framebuffer only as large as needed.
//   3. Fix shader uniforms that were broken by (2).
//
// Then:
//   4. Allocate an IOSurface as the drawing destination.
//   5. Draw the RPDQ.
void GLRenderer::CopyRenderPassDrawQuadToOverlayResource(
    const CALayerOverlay* ca_layer_overlay,
    std::unique_ptr<OverlayTexture>* overlay_texture,
    gfx::RectF* new_bounds) {
  // Don't carry over any GL state from previous RenderPass draw operations.
  ReinitializeGLState();
  auto contents_texture_it =
      render_pass_textures_.find(ca_layer_overlay->rpdq->render_pass_id);
  DCHECK(contents_texture_it != render_pass_textures_.end());

  // Configure parameters as if drawing to a framebuffer the size of the
  // screen.
  DrawRenderPassDrawQuadParams params;
  params.quad = ca_layer_overlay->rpdq;
  params.flip_texture = true;
  params.contents_texture = &contents_texture_it->second;
  params.quad_to_target_transform =
      params.quad->shared_quad_state->quad_to_target_transform;
  params.tex_coord_rect = params.quad->tex_coord_rect;

  // Calculate projection and window matrices using InitializeViewport(). This
  // requires creating a dummy DrawingFrame.
  {
    DrawingFrame dummy_frame;
    gfx::Rect frame_rect(current_frame()->device_viewport_size);
    force_drawing_frame_framebuffer_unflipped_ = true;
    InitializeViewport(&dummy_frame, frame_rect, frame_rect, frame_rect.size());
    force_drawing_frame_framebuffer_unflipped_ = false;
    params.projection_matrix = dummy_frame.projection_matrix;
    params.window_matrix = dummy_frame.window_matrix;
  }

  // Perform basic initialization with the screen-sized viewport.
  if (!InitializeRPDQParameters(&params))
    return;

  if (!UpdateRPDQWithSkiaFilters(&params))
    return;

  // |params.dst_rect| now contain values that reflect a potentially increased
  // size quad.
  gfx::RectF updated_dst_rect = params.dst_rect;
  gfx::Size dst_pixel_size = gfx::ToCeiledSize(updated_dst_rect.size());

  int iosurface_width = dst_pixel_size.width();
  int iosurface_height = dst_pixel_size.height();
  if (!settings_->dont_round_texture_sizes_for_pixel_tests) {
    // Round the size of the IOSurface to a multiple of 64 pixels. This reduces
    // memory fragmentation. https://crbug.com/146070. This also allows
    // IOSurfaces to be more easily reused during a resize operation.
    int iosurface_multiple = 64;
    iosurface_width =
        cc::MathUtil::CheckedRoundUp(iosurface_width, iosurface_multiple);
    iosurface_height =
        cc::MathUtil::CheckedRoundUp(iosurface_height, iosurface_multiple);
  }

  *overlay_texture =
      FindOrCreateOverlayTexture(params.quad->render_pass_id, iosurface_width,
                                 iosurface_height, RootRenderPassColorSpace());
  *new_bounds = gfx::RectF(updated_dst_rect.origin(),
                           gfx::SizeF((*overlay_texture)->texture.size()));

  // Calculate new projection and window matrices for a minimally sized viewport
  // using InitializeViewport(). This requires creating a dummy DrawingFrame.
  {
    DrawingFrame dummy_frame;
    force_drawing_frame_framebuffer_unflipped_ = true;
    gfx::Rect frame_rect =
        gfx::Rect(0, 0, updated_dst_rect.width(), updated_dst_rect.height());
    InitializeViewport(&dummy_frame, frame_rect, frame_rect, frame_rect.size());
    force_drawing_frame_framebuffer_unflipped_ = false;
    params.projection_matrix = dummy_frame.projection_matrix;
    params.window_matrix = dummy_frame.window_matrix;
  }

  // Calculate a new quad_to_target_transform.
  params.quad_to_target_transform = gfx::Transform();
  params.quad_to_target_transform.Translate(-updated_dst_rect.x(),
                                            -updated_dst_rect.y());

  // Antialiasing works by fading out content that is close to the edge of the
  // viewport. All of these values need to be recalculated.
  if (params.use_aa) {
    current_window_space_viewport_ =
        gfx::Rect(0, 0, updated_dst_rect.width(), updated_dst_rect.height());
    gfx::Transform quad_rect_matrix;
    QuadRectTransform(&quad_rect_matrix, params.quad_to_target_transform,
                      updated_dst_rect);
    params.contents_device_transform =
        params.window_matrix * params.projection_matrix * quad_rect_matrix;
    bool clipped = false;
    params.contents_device_transform.FlattenTo2d();
    gfx::QuadF device_layer_quad = cc::MathUtil::MapQuad(
        params.contents_device_transform, SharedGeometryQuad(), &clipped);
    LayerQuad device_layer_edges(device_layer_quad);
    InflateAntiAliasingDistances(device_layer_quad, &device_layer_edges,
                                 params.edge);
  }

  // Establish destination texture.
  GLuint temp_fbo;
  gl_->GenFramebuffers(1, &temp_fbo);
  gl_->BindFramebuffer(GL_FRAMEBUFFER, temp_fbo);
  gl_->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            (*overlay_texture)->texture.target(),
                            (*overlay_texture)->texture.id(), 0);
  DCHECK(gl_->CheckFramebufferStatus(GL_FRAMEBUFFER) ==
             GL_FRAMEBUFFER_COMPLETE ||
         IsContextLost());

  // Clear to 0 to ensure the background is transparent.
  gl_->ClearColor(0, 0, 0, 0);
  gl_->Clear(GL_COLOR_BUFFER_BIT);

  UpdateRPDQTexturesForSampling(&params);
  UpdateRPDQBlendMode(&params);
  // The code in this method (CopyRenderPassDrawQuadToOverlayResource) is
  // only called when we are drawing for the purpose of copying to
  // a CALayerOverlay. In such cases, the CALayerOverlay applies rounded
  // corners via CALayer parameters, so the shader-based rounded corners
  // should be disabled here.
  params.apply_shader_based_rounded_corner = false;
  ChooseRPDQProgram(&params, (*overlay_texture)->texture.color_space());
  UpdateRPDQUniforms(&params);

  // Prior to drawing, set up the destination framebuffer and viewport.
  gl_->BindFramebuffer(GL_FRAMEBUFFER, temp_fbo);
  gl_->Viewport(0, 0, updated_dst_rect.width(), updated_dst_rect.height());

  DrawRPDQ(params);
  if (params.background_texture) {
    gl_->DeleteTextures(1, &params.background_texture);
    params.background_texture = 0;
  }
  gl_->DeleteFramebuffers(1, &temp_fbo);
}

std::unique_ptr<GLRenderer::OverlayTexture>
GLRenderer::FindOrCreateOverlayTexture(
    const AggregatedRenderPassId& render_pass_id,
    int width,
    int height,
    const gfx::ColorSpace& color_space) {
  // First try to use a texture for the same CompositorRenderPassId, to keep
  // things more stable and less likely to clobber each others textures.
  auto match_with_id = [&](const std::unique_ptr<OverlayTexture>& overlay) {
    return overlay->render_pass_id == render_pass_id &&
           overlay->texture.size().width() >= width &&
           overlay->texture.size().height() >= height &&
           overlay->texture.size().width() <= width * 2 &&
           overlay->texture.size().height() <= height * 2;
  };
  auto it = std::find_if(available_overlay_textures_.begin(),
                         available_overlay_textures_.end(), match_with_id);
  if (it != available_overlay_textures_.end()) {
    std::unique_ptr<OverlayTexture> result = std::move(*it);
    available_overlay_textures_.erase(it);

    result->render_pass_id = render_pass_id;
    return result;
  }

  // Then fallback to trying other textures that still match.
  auto match = [&](const std::unique_ptr<OverlayTexture>& overlay) {
    return overlay->texture.size().width() >= width &&
           overlay->texture.size().height() >= height &&
           overlay->texture.size().width() <= width * 2 &&
           overlay->texture.size().height() <= height * 2;
  };
  it = std::find_if(available_overlay_textures_.begin(),
                    available_overlay_textures_.end(), match);
  if (it != available_overlay_textures_.end()) {
    std::unique_ptr<OverlayTexture> result = std::move(*it);
    available_overlay_textures_.erase(it);

    result->render_pass_id = render_pass_id;
    return result;
  }

  // Make a new texture if we could not find a match. Sadtimes.
  auto result = std::make_unique<OverlayTexture>();
  result->texture = ScopedGpuMemoryBufferTexture(
      output_surface_->context_provider(),
      gfx::Size(width, height), color_space);
  result->render_pass_id = render_pass_id;
  return result;
}

void GLRenderer::ReduceAvailableOverlayTextures() {
  // Overlay resources may get returned back to the compositor at varying rates,
  // so we may get a number of resources returned at once, then none for a
  // while. As such, we want to hold onto enough resources to not have to create
  // any when none are released for a while. Emperical study by erikchen@ on
  // crbug.com/636884 found that saving 5 spare textures per RenderPass was
  // sufficient for important benchmarks. This seems to imply that the OS may
  // hold up to 5 frames of textures before releasing them.
  static const int kKeepCountPerRenderPass = 5;

  // In order to accomodate the above requirements, we hold any released texture
  // in the |available_overlay_textures_| set for up to 5 frames before
  // discarding it.
  for (const auto& overlay : available_overlay_textures_)
    overlay->frames_waiting_for_reuse++;
  base::EraseIf(available_overlay_textures_,
                [](const std::unique_ptr<OverlayTexture>& overlay) {
                  return overlay->frames_waiting_for_reuse >=
                         kKeepCountPerRenderPass;
                });
}

std::unique_ptr<GLRenderer::OverlayTexture>
GLRenderer::ScheduleRenderPassDrawQuad(const CALayerOverlay* ca_layer_overlay) {
  DCHECK(ca_layer_overlay->rpdq);

  std::unique_ptr<OverlayTexture> overlay_texture;
  gfx::RectF new_bounds;
  CopyRenderPassDrawQuadToOverlayResource(ca_layer_overlay, &overlay_texture,
                                          &new_bounds);
  if (!overlay_texture)
    return {};

  GLfloat contents_rect[4] = {
      ca_layer_overlay->contents_rect.x(), ca_layer_overlay->contents_rect.y(),
      ca_layer_overlay->contents_rect.width(),
      ca_layer_overlay->contents_rect.height(),
  };
  GLfloat bounds_rect[4] = {
      new_bounds.x(), new_bounds.y(), new_bounds.width(), new_bounds.height(),
  };
  GLboolean is_clipped = ca_layer_overlay->shared_state->is_clipped;
  GLfloat clip_rect[4] = {ca_layer_overlay->shared_state->clip_rect.x(),
                          ca_layer_overlay->shared_state->clip_rect.y(),
                          ca_layer_overlay->shared_state->clip_rect.width(),
                          ca_layer_overlay->shared_state->clip_rect.height()};

  const gfx::RectF& rect =
      ca_layer_overlay->shared_state->rounded_corner_bounds.rect();
  GLfloat rounded_corner_rect[5] = {
      rect.x(), rect.y(), rect.width(), rect.height(),
      ca_layer_overlay->shared_state->rounded_corner_bounds.GetSimpleRadius()};

  GLint sorting_context_id = ca_layer_overlay->shared_state->sorting_context_id;
  SkMatrix44 transform = ca_layer_overlay->shared_state->transform;
  GLfloat gl_transform[16];
  transform.asColMajorf(gl_transform);
  unsigned filter = ca_layer_overlay->filter;

  // The alpha has already been applied when copying the RPDQ to an IOSurface.
  GLfloat alpha = 1;
  gl_->ScheduleCALayerSharedStateCHROMIUM(alpha, is_clipped, clip_rect,
                                          rounded_corner_rect,
                                          sorting_context_id, gl_transform);
  gl_->ScheduleCALayerCHROMIUM(overlay_texture->texture.id(), contents_rect,
                               ca_layer_overlay->background_color,
                               ca_layer_overlay->edge_aa_mask, bounds_rect,
                               filter);
  return overlay_texture;
}
#endif  // defined(OS_APPLE)

void GLRenderer::SetupOverdrawFeedback() {
  gl_->StencilFunc(GL_ALWAYS, 1, 0xffffffff);
  // First two values are ignored as test always passes.
  gl_->StencilOp(GL_KEEP, GL_KEEP, GL_INCR);
  gl_->StencilMask(0xffffffff);
}

void GLRenderer::FlushOverdrawFeedback(const gfx::Rect& output_rect) {
  DCHECK(stencil_shadow_);

  // Test only, keep everything.
  gl_->StencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

  EnsureScissorTestDisabled();
  SetBlendEnabled(true);

  PrepareGeometry(SHARED_BINDING);

  SetUseProgram(ProgramKey::DebugBorder(), gfx::ColorSpace::CreateSRGB(),
                CurrentRenderPassColorSpace());

  gfx::Transform render_matrix;
  render_matrix.Translate(0.5 * output_rect.width() + output_rect.x(),
                          0.5 * output_rect.height() + output_rect.y());
  render_matrix.Scale(output_rect.width(), output_rect.height());
  SetShaderMatrix(current_frame()->projection_matrix * render_matrix);

  // Produce hinting for the amount of overdraw on screen for each pixel by
  // drawing hint colors to the framebuffer based on the current stencil value.
  struct {
    int multiplier;
    GLenum func;
    GLint ref;
    SkColor color;
  } stencil_tests[] = {
      {1, GL_EQUAL, 2, 0x2f0000ff},  // Blue: Overdrawn once.
      {2, GL_EQUAL, 3, 0x2f00ff00},  // Green: Overdrawn twice.
      {3, GL_EQUAL, 4, 0x3fff0000},  // Pink: Overdrawn three times.
      {4, GL_LESS, 4, 0x7fff0000},   // Red: Overdrawn four or more times.
  };

  for (const auto& test : stencil_tests) {
    gl_->StencilFunc(test.func, test.ref, 0xffffffff);
    // Transparent color unless color-coding of overdraw is enabled.
    SetShaderColor(debug_settings_->show_overdraw_feedback ? test.color : 0,
                   1.f);
    gl_->DrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
  }
}

void GLRenderer::ProcessOverdrawFeedback(base::CheckedNumeric<int> surface_area,
                                         unsigned occlusion_query) {
  unsigned result = 0;
  DCHECK(occlusion_query);
  gl_->GetQueryObjectuivEXT(occlusion_query, GL_QUERY_RESULT_EXT, &result);
  gl_->DeleteQueriesEXT(1, &occlusion_query);

  // Report GPU overdraw as a percentage of |surface_area|.
  TRACE_COUNTER1(TRACE_DISABLED_BY_DEFAULT("viz.overdraw"), "GPU Overdraw",
                 (result * 100.0 /
                  static_cast<int>(surface_area.ValueOrDefault(INT_MAX))));
}

void GLRenderer::UpdateRenderPassTextures(
    const AggregatedRenderPassList& render_passes_in_draw_order,
    const base::flat_map<AggregatedRenderPassId, RenderPassRequirements>&
        render_passes_in_frame) {
  // Collect RenderPass textures that should be deleted.
  std::vector<AggregatedRenderPassId> passes_to_delete;
  for (const auto& pair : render_pass_textures_) {
    auto render_pass_it = render_passes_in_frame.find(pair.first);
    if (render_pass_it == render_passes_in_frame.end()) {
      passes_to_delete.push_back(pair.first);
      continue;
    }
    const RenderPassRequirements& requirements = render_pass_it->second;
    const ScopedRenderPassTexture& texture = pair.second;
    bool size_appropriate =
        texture.size().width() >= requirements.size.width() &&
        texture.size().height() >= requirements.size.height();
    bool mipmap_appropriate = !requirements.generate_mipmap || texture.mipmap();
    if (!size_appropriate || !mipmap_appropriate)
      passes_to_delete.push_back(pair.first);
  }
  // Delete RenderPass textures from the previous frame that will not be used
  // again.
  for (auto& pass_to_delete : passes_to_delete) {
    auto rp_backdrop_texture_it =
        render_pass_backdrop_textures_.find(pass_to_delete);
    if (rp_backdrop_texture_it != render_pass_backdrop_textures_.end())
      render_pass_backdrop_textures_.erase(pass_to_delete);
    render_pass_textures_.erase(pass_to_delete);
  }
}

ResourceFormat GLRenderer::CurrentRenderPassResourceFormat() const {
  const auto& caps = output_surface_->context_provider()->ContextCapabilities();
  if (CurrentRenderPassColorSpace().IsHDR()) {
    // If a platform does not support half-float renderbuffers then it should
    // not should request HDR rendering.
    DCHECK(caps.texture_half_float_linear);
    DCHECK(caps.color_buffer_half_float_rgba);
    return RGBA_F16;
  }
  return PlatformColor::BestSupportedTextureFormat(caps);
}

bool GLRenderer::HasOutputColorMatrix() const {
  const bool is_root_render_pass =
      current_frame()->current_render_pass == current_frame()->root_render_pass;
  const SkMatrix44& output_color_matrix = output_surface_->color_matrix();
  return is_root_render_pass && !output_color_matrix.isIdentity();
}

bool GLRenderer::CanUseFastSolidColorDraw(
    const SolidColorDrawQuad* quad) const {
  const SharedQuadState* sqs = quad->shared_quad_state;

  if (!use_fast_path_solid_color_quad_)
    return false;

  // Mask filters require blending with the background, which is not possible
  // with the glClear draw method.
  if (!sqs->mask_filter_info.IsEmpty())
    return false;

  // 3D transforms need vertex computation in 3D and cannot be handled using
  // glClear().
  if (!sqs->quad_to_target_transform.IsFlat())
    return false;

  // glClear ignores stencil buffer.
  if (stencil_shadow_)
    return false;

  // Any non axis aligned transform cannot be handled by glClear.
  if (!sqs->quad_to_target_transform.Preserves2dAxisAlignment())
    return false;

  // If no blending is needed for the quad, then fast draw can be safely used.
  if (!quad->ShouldDrawWithBlending() && SkColorGetA(quad->color) == 255)
    return true;

  // It is safe to use glClearColor with alpha blending when the render
  // pass has transparent background because the blending happens against
  // (0, 0, 0, 0) which is the same as replacing the destination color & alpha.
  // However, if the render pass does not have a transparent background, using
  // glClear with a color that has alpha or opacity, would end up punching an
  // unwanted hole in the frame buffer.
  if (!current_frame()->current_render_pass->has_transparent_background)
    return false;

  // If the color has any alpha and blending is needed, ensure the blend mode
  // allows replacing destination color & alpha.
  const bool is_translucent =
      SkColorGetA(quad->color) != 255 || quad->shared_quad_state->opacity < 1.f;
  if (is_translucent &&
      !(quad->shared_quad_state->blend_mode == SkBlendMode::kSrc ||
        quad->shared_quad_state->blend_mode == SkBlendMode::kSrcOver)) {
    return false;
  }

  gfx::RectF quad_rect_in_target(quad->visible_rect);
  sqs->quad_to_target_transform.TransformRect(&quad_rect_in_target);
  const gfx::Rect quad_rect_in_target_rounded =
      gfx::ToRoundedRect(quad_rect_in_target);

  // If the quad does not intersect with any region that has already been drawn
  // to, then blending is not an issue and fast draw path can be used.
  for (const auto& rect : drawn_rects_)
    if (quad_rect_in_target_rounded.Intersects(rect))
      return false;
  return true;
}

void GLRenderer::AllocateRenderPassResourceIfNeeded(
    const AggregatedRenderPassId& render_pass_id,
    const RenderPassRequirements& requirements) {
  auto contents_texture_it = render_pass_textures_.find(render_pass_id);
  if (contents_texture_it != render_pass_textures_.end()) {
    DCHECK(gfx::Rect(contents_texture_it->second.size())
               .Contains(gfx::Rect(requirements.size)));
    return;
  }

  ScopedRenderPassTexture contents_texture(
      output_surface_->context_provider(), requirements.size,
      CurrentRenderPassResourceFormat(), CurrentRenderPassColorSpace(),
      requirements.generate_mipmap);
  render_pass_textures_[render_pass_id] = std::move(contents_texture);
}

bool GLRenderer::IsRenderPassResourceAllocated(
    const AggregatedRenderPassId& render_pass_id) const {
  auto texture_it = render_pass_textures_.find(render_pass_id);
  return texture_it != render_pass_textures_.end();
}

gfx::Size GLRenderer::GetRenderPassBackingPixelSize(
    const AggregatedRenderPassId& render_pass_id) {
  auto texture_it = render_pass_textures_.find(render_pass_id);
  DCHECK(texture_it != render_pass_textures_.end());
  return texture_it->second.size();
}

GLRenderer::OverlayTexture::OverlayTexture() = default;
GLRenderer::OverlayTexture::~OverlayTexture() = default;

}  // namespace viz
