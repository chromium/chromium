// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/command_line.h"
#include "base/cxx17_backports.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/aligned_memory.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "cc/test/test_types.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/features.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/quads/yuv_video_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/switches.h"
#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "components/viz/test/buildflags.h"
#include "components/viz/test/test_in_process_context_provider.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "components/viz/test/test_types.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/renderers/video_resource_updater.h"
#include "media/video/half_float_maker.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/test/icc_profiles.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_implementation.h"

namespace viz {
namespace {
const gfx::DisplayColorSpaces kRec601DisplayColorSpaces(
    gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                    gfx::ColorSpace::TransferID::SMPTE170M));

#if !BUILDFLAG(IS_ANDROID)

constexpr char kANGLEMetalStr[] = "_angle_metal";

bool IsANGLEMetal() {
  return gl::GetGLImplementationParts() ==
         gl::GLImplementationParts(gl::ANGLEImplementation::kMetal);
}

template <typename T>
base::span<const uint8_t> MakePixelSpan(const std::vector<T>& vec) {
  return base::make_span(reinterpret_cast<const uint8_t*>(vec.data()),
                         vec.size() * sizeof(T));
}

base::span<const uint8_t> MakePixelSpan(const SkBitmap& bitmap) {
  return base::make_span(static_cast<const uint8_t*>(bitmap.getPixels()),
                         bitmap.computeByteSize());
}

base::WritableSharedMemoryMapping AllocateAndRegisterSharedBitmapMemory(
    const SharedBitmapId& id,
    const gfx::Size& size,
    SharedBitmapManager* shared_bitmap_manager) {
  base::MappedReadOnlyRegion shm =
      bitmap_allocation::AllocateSharedBitmap(size, RGBA_8888);
  shared_bitmap_manager->ChildAllocatedSharedBitmap(shm.region.Map(), id);
  return std::move(shm.mapping);
}

void DeleteSharedImage(scoped_refptr<ContextProvider> context_provider,
                       gpu::Mailbox mailbox,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  sii->DestroySharedImage(sync_token, mailbox);
}

ResourceId CreateGpuResource(scoped_refptr<ContextProvider> context_provider,
                             ClientResourceProvider* resource_provider,
                             const gfx::Size& size,
                             SharedImageFormat format,
                             gfx::ColorSpace color_space,
                             base::span<const uint8_t> pixels) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  gpu::Mailbox mailbox = sii->CreateSharedImage(
      format, size, color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
      gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, pixels);
  gpu::SyncToken sync_token = sii->GenUnverifiedSyncToken();

  TransferableResource gl_resource = TransferableResource::MakeGpu(
      mailbox, GL_LINEAR, GL_TEXTURE_2D, sync_token, size,
      format.resource_format(), false /* is_overlay_candidate */);
  gl_resource.color_space = std::move(color_space);
  auto release_callback =
      base::BindOnce(&DeleteSharedImage, std::move(context_provider), mailbox);
  return resource_provider->ImportResource(gl_resource,
                                           std::move(release_callback));
}

std::unique_ptr<AggregatedRenderPass> CreateTestRootRenderPass(
    AggregatedRenderPassId id,
    const gfx::Rect& rect) {
  auto pass = std::make_unique<AggregatedRenderPass>();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass;
}

std::unique_ptr<AggregatedRenderPass> CreateTestRenderPass(
    AggregatedRenderPassId id,
    const gfx::Rect& rect,
    const gfx::Transform& transform_to_root_target) {
  auto pass = std::make_unique<AggregatedRenderPass>();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass;
}

SharedQuadState* CreateTestSharedQuadState(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    AggregatedRenderPass* render_pass,
    const gfx::MaskFilterInfo& mask_filter_info) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       mask_filter_info, /**clip_rect=*/absl::nullopt,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id);
  return shared_state;
}

SharedQuadState* CreateTestSharedQuadStateClipped(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    const gfx::Rect& clip_rect,
    AggregatedRenderPass* render_pass) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = clip_rect;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       /*mask_filter_info=*/gfx::MaskFilterInfo(), clip_rect,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id);

  return shared_state;
}

void CreateTestRenderPassDrawQuad(const SharedQuadState* shared_state,
                                  const gfx::Rect& rect,
                                  AggregatedRenderPassId pass_id,
                                  AggregatedRenderPass* render_pass) {
  auto* quad =
      render_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_state, rect, rect, pass_id,
               kInvalidResourceId,  // mask_resource_id
               gfx::RectF(),        // mask_uv_rect
               gfx::Size(),         // mask_texture_size
               gfx::Vector2dF(),    // filters scale
               gfx::PointF(),       // filter origin
               gfx::RectF(rect),    // tex_coord_rect
               false,               // force_anti_aliasing_off
               1.0f);               // backdrop_filter_quality
}

// Create a TextureDrawDrawQuad with two given colors.
// flipped_texture_quad: The TextureDrawDrawQuad is y flipped.
// half_and_half: if true, the upper half part of the texture is filled with
//   texel_color_one, other part of the texture is filled with texel_color_two.
//   if false, a 1/2 width and height rectangle in the middle of the quad will
//   be filled with texel_color_two, other part of the texture is filled with
//   texel_color_one,
// TODO(crbug.com/1308932): Make this function use SkColor4f
void CreateTestTwoColoredTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor4f texel_color_one,
    SkColor4f texel_color_two,
    SkColor4f background_color,
    bool premultiplied_alpha,
    bool flipped_texture_quad,
    bool half_and_half,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    AggregatedRenderPass* render_pass) {
  // As this function renders to an RGBA_8888 texture, it makes sense to use
  // integer colors
  SkPMColor pixel_color_one =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color_one.toSkColor())
          : SkPackARGB32NoCheck(
                255 * texel_color_one.fA, 255 * texel_color_one.fR,
                255 * texel_color_one.fG, 255 * texel_color_one.fB);
  SkPMColor pixel_color_two =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color_two.toSkColor())
          : SkPackARGB32NoCheck(
                255 * texel_color_two.fA, 255 * texel_color_two.fR,
                255 * texel_color_two.fG, 255 * texel_color_two.fB);
  // The default color is texel_color_one
  std::vector<uint32_t> pixels(rect.size().GetArea(), pixel_color_one);
  if (half_and_half) {
    // Fill the bottom half part of the texture with texel_color_two.
    for (int i = rect.height() / 2; i < rect.height(); ++i) {
      for (int k = 0; k < rect.width(); ++k) {
        pixels[i * rect.width() + k] = pixel_color_two;
      }
    }
  } else {
    // Fill a 1/2 width and height rectangle with pixel_color_two.
    for (int i = rect.height() / 4; i < (rect.height() * 3 / 4); ++i) {
      for (int k = rect.width() / 4; k < (rect.width() * 3 / 4); ++k) {
        pixels[i * rect.width() + k] = pixel_color_two;
      }
    }
  }

  ResourceId resource;
  if (gpu_resource) {
    resource =
        CreateGpuResource(child_context_provider, child_resource_provider,
                          rect.size(), SinglePlaneFormat::kBGRA_8888,
                          gfx::ColorSpace(), MakePixelSpan(pixels));
  } else {
    SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
    base::WritableSharedMemoryMapping mapping =
        AllocateAndRegisterSharedBitmapMemory(shared_bitmap_id, rect.size(),
                                              shared_bitmap_manager);
    resource = child_resource_provider->ImportResource(
        TransferableResource::MakeSoftware(shared_bitmap_id, rect.size(),
                                           RGBA_8888),
        base::DoNothing());

    auto span = mapping.GetMemoryAsSpan<uint32_t>(pixels.size());
    base::ranges::copy(pixels, span.begin());
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider.get());
  ResourceId mapped_resource = resource_map[resource];

  bool needs_blending = true;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, vertex_opacity, flipped_texture_quad,
               nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
}

void CreateTestTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor4f texel_color,
    float vertex_opacity[4],
    SkColor4f background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    AggregatedRenderPass* render_pass) {
  // As this function renders to an RGBA_8888 texture, it makes sense to use
  // integer colors
  SkPMColor pixel_color =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color.toSkColor())
          : SkPackARGB32NoCheck(texel_color.fA * 255, texel_color.fR * 255,
                                texel_color.fG * 255, texel_color.fB * 255);
  size_t num_pixels = static_cast<size_t>(rect.width()) * rect.height();
  std::vector<uint32_t> pixels(num_pixels, pixel_color);

  ResourceId resource;
  if (gpu_resource) {
    resource =
        CreateGpuResource(child_context_provider, child_resource_provider,
                          rect.size(), SinglePlaneFormat::kRGBA_8888,
                          gfx::ColorSpace(), MakePixelSpan(pixels));
  } else {
    SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
    base::WritableSharedMemoryMapping mapping =
        AllocateAndRegisterSharedBitmapMemory(shared_bitmap_id, rect.size(),
                                              shared_bitmap_manager);
    resource = child_resource_provider->ImportResource(
        TransferableResource::MakeSoftware(shared_bitmap_id, rect.size(),
                                           RGBA_8888),
        base::DoNothing());

    auto span = mapping.GetMemoryAsSpan<uint32_t>(pixels.size());
    base::ranges::copy(pixels, span.begin());
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider.get());
  ResourceId mapped_resource = resource_map[resource];

  bool needs_blending = true;
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool flipped = false;
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, vertex_opacity, flipped, nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear);
}

// TODO(crbug.com/1308932): Make this function use SkColor4f
void CreateTestTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor4f texel_color,
    SkColor4f background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    AggregatedRenderPass* render_pass) {
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  CreateTestTextureDrawQuad(gpu_resource, rect, texel_color, vertex_opacity,
                            background_color, premultiplied_alpha, shared_state,
                            resource_provider, child_resource_provider,
                            shared_bitmap_manager,
                            std::move(child_context_provider), render_pass);
}

void CreateTestYUVVideoDrawQuad_FromVideoFrame(
    const SharedQuadState* shared_state,
    scoped_refptr<media::VideoFrame> video_frame,
    uint8_t alpha_value,
    const gfx::RectF& tex_coord_rect,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  const bool with_alpha = (video_frame->format() == media::PIXEL_FORMAT_I420A);

  gfx::ColorSpace video_color_space = video_frame->ColorSpace();
  DCHECK(video_color_space.IsValid());

  bool needs_blending = true;

  if (with_alpha) {
    memset(video_frame->writable_data(media::VideoFrame::kAPlane), alpha_value,
           video_frame->stride(media::VideoFrame::kAPlane) *
               video_frame->rows(media::VideoFrame::kAPlane));
  }

  media::VideoFrameExternalResources resources =
      video_resource_updater->CreateExternalResourcesFromVideoFrame(
          video_frame);

  EXPECT_EQ(media::VideoFrameResourceType::YUV, resources.type);
  EXPECT_EQ(media::VideoFrame::NumPlanes(video_frame->format()),
            resources.resources.size());
  EXPECT_EQ(media::VideoFrame::NumPlanes(video_frame->format()),
            resources.release_callbacks.size());

  ResourceId resource_y = child_resource_provider->ImportResource(
      resources.resources[media::VideoFrame::kYPlane],
      std::move(resources.release_callbacks[media::VideoFrame::kYPlane]));
  ResourceId resource_u = child_resource_provider->ImportResource(
      resources.resources[media::VideoFrame::kUPlane],
      std::move(resources.release_callbacks[media::VideoFrame::kUPlane]));
  ResourceId resource_v = child_resource_provider->ImportResource(
      resources.resources[media::VideoFrame::kVPlane],
      std::move(resources.release_callbacks[media::VideoFrame::kVPlane]));
  ResourceId resource_a = kInvalidResourceId;
  if (with_alpha) {
    resource_a = child_resource_provider->ImportResource(
        resources.resources[media::VideoFrame::kAPlane],
        std::move(resources.release_callbacks[media::VideoFrame::kAPlane]));
  }

  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_y);
  resource_ids_to_transfer.push_back(resource_u);
  resource_ids_to_transfer.push_back(resource_v);
  if (with_alpha)
    resource_ids_to_transfer.push_back(resource_a);
  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          resource_ids_to_transfer, resource_provider, child_resource_provider,
          child_context_provider);

  ResourceId mapped_resource_y = resource_map[resource_y];
  ResourceId mapped_resource_u = resource_map[resource_u];
  ResourceId mapped_resource_v = resource_map[resource_v];
  ResourceId mapped_resource_a = kInvalidResourceId;

  if (with_alpha) {
    mapped_resource_a = resource_map[resource_a];
  }
  const gfx::Size uv_sample_size = media::VideoFrame::SampleSize(
      video_frame->format(), media::VideoFrame::kUPlane);
  const gfx::Size coded_size = video_frame->coded_size();
  DCHECK_EQ(uv_sample_size,
            media::VideoFrame::SampleSize(video_frame->format(),
                                          media::VideoFrame::kVPlane));
  if (with_alpha) {
    DCHECK_EQ(gfx::Size(1, 1),
              media::VideoFrame::SampleSize(video_frame->format(),
                                            media::VideoFrame::kAPlane))
        << "Expected A plane to have same size as Y plane.";
  }

  const gfx::Rect video_visible_rect = gfx::ToNearestRect(
      gfx::RectF(tex_coord_rect.x() * coded_size.width(),
                 tex_coord_rect.y() * coded_size.height(),
                 tex_coord_rect.width() * coded_size.width(),
                 tex_coord_rect.height() * coded_size.height()));

  auto* yuv_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  uint32_t bits_per_channel = 8;
  if (video_frame->format() == media::PIXEL_FORMAT_YUV420P10 ||
      video_frame->format() == media::PIXEL_FORMAT_YUV422P10 ||
      video_frame->format() == media::PIXEL_FORMAT_YUV444P10) {
    bits_per_channel = 10;
  }

  SharedImageFormat yuv_highbit_resource_format =
      video_resource_updater->YuvSharedImageFormat(bits_per_channel);

  float offset = 0.0f;
  float multiplier = 1.0f;

  if (yuv_highbit_resource_format == SinglePlaneFormat::kR_16) {
    multiplier = 65535.0f / ((1 << bits_per_channel) - 1);
  } else if (yuv_highbit_resource_format == SinglePlaneFormat::kLUMINANCE_F16) {
    std::unique_ptr<media::HalfFloatMaker> half_float_maker =
        media::HalfFloatMaker::NewHalfFloatMaker(bits_per_channel);
    offset = half_float_maker->Offset();
    multiplier = half_float_maker->Multiplier();
  } else {
    bits_per_channel = 8;
  }

  yuv_quad->SetNew(shared_state, rect, visible_rect, needs_blending, coded_size,
                   video_visible_rect, uv_sample_size, mapped_resource_y,
                   mapped_resource_u, mapped_resource_v, mapped_resource_a,
                   video_color_space, offset, multiplier, bits_per_channel,
                   gfx::ProtectedVideoType::kClear, absl::nullopt);
}

void CreateTestY16TextureDrawQuad_FromVideoFrame(
    const SharedQuadState* shared_state,
    scoped_refptr<media::VideoFrame> video_frame,
    const gfx::RectF& tex_coord_rect,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  media::VideoFrameExternalResources resources =
      video_resource_updater->CreateExternalResourcesFromVideoFrame(
          video_frame);

  EXPECT_EQ(media::VideoFrameResourceType::RGBA, resources.type);
  EXPECT_EQ(1u, resources.resources.size());
  EXPECT_EQ(1u, resources.release_callbacks.size());

  ResourceId resource_y = child_resource_provider->ImportResource(
      resources.resources[0], std::move(resources.release_callbacks[0]));

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource_y}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider);
  ResourceId mapped_resource_y = resource_map[resource_y];

  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  bool needs_blending = true;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource_y,
               false, tex_coord_rect.origin(), tex_coord_rect.bottom_right(),
               SkColors::kBlack, vertex_opacity, false, false,
               /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);
}

// Upshift video frame to 10 bit.
scoped_refptr<media::VideoFrame> CreateHighbitVideoFrame(
    media::VideoFrame* video_frame) {
  media::VideoPixelFormat format;
  switch (video_frame->format()) {
    case media::PIXEL_FORMAT_I420:
      format = media::PIXEL_FORMAT_YUV420P10;
      break;
    case media::PIXEL_FORMAT_I422:
      format = media::PIXEL_FORMAT_YUV422P10;
      break;
    case media::PIXEL_FORMAT_I444:
      format = media::PIXEL_FORMAT_YUV444P10;
      break;

    default:
      NOTREACHED();
      return nullptr;
  }
  scoped_refptr<media::VideoFrame> ret = media::VideoFrame::CreateFrame(
      format, video_frame->coded_size(), video_frame->visible_rect(),
      video_frame->natural_size(), video_frame->timestamp());

  // Copy all metadata.
  ret->metadata().MergeMetadataFrom(video_frame->metadata());

  for (int plane = media::VideoFrame::kYPlane;
       plane <= media::VideoFrame::kVPlane; ++plane) {
    int width = video_frame->row_bytes(plane);
    const uint8_t* src = video_frame->data(plane);
    uint16_t* dst = reinterpret_cast<uint16_t*>(ret->writable_data(plane));
    for (int row = 0; row < video_frame->rows(plane); row++) {
      for (int x = 0; x < width; x++) {
        // Replicate the top bits into the lower bits, this way
        // 0xFF becomes 0x3FF.
        dst[x] = (src[x] << 2) | (src[x] >> 6);
      }
      src += video_frame->stride(plane);
      dst += ret->stride(plane) / 2;
    }
  }
  return ret;
}

void CreateTestYUVVideoDrawQuad_Striped(
    const SharedQuadState* shared_state,
    media::VideoPixelFormat format,
    gfx::ColorSpace color_space,
    bool is_transparent,
    bool highbit,
    const gfx::RectF& tex_coord_rect,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      format, rect.size(), rect, rect.size(), base::TimeDelta());

  // YUV values representing a striped pattern, for validating texture
  // coordinates for sampling.
  uint8_t y_value = 0;
  uint8_t u_value = 0;
  uint8_t v_value = 0;
  for (int i = 0; i < video_frame->rows(media::VideoFrame::kYPlane); ++i) {
    uint8_t* y_row = video_frame->writable_data(media::VideoFrame::kYPlane) +
                     video_frame->stride(media::VideoFrame::kYPlane) * i;
    for (int j = 0; j < video_frame->row_bytes(media::VideoFrame::kYPlane);
         ++j) {
      y_row[j] = (y_value += 1);
    }
  }
  for (int i = 0; i < video_frame->rows(media::VideoFrame::kUPlane); ++i) {
    uint8_t* u_row = video_frame->writable_data(media::VideoFrame::kUPlane) +
                     video_frame->stride(media::VideoFrame::kUPlane) * i;
    uint8_t* v_row = video_frame->writable_data(media::VideoFrame::kVPlane) +
                     video_frame->stride(media::VideoFrame::kVPlane) * i;
    for (int j = 0; j < video_frame->row_bytes(media::VideoFrame::kUPlane);
         ++j) {
      u_row[j] = (u_value += 3);
      v_row[j] = (v_value += 5);
    }
  }
  uint8_t alpha_value = is_transparent ? 0 : 128;

  if (highbit)
    video_frame = CreateHighbitVideoFrame(video_frame.get());
  video_frame->set_color_space(color_space);

  CreateTestYUVVideoDrawQuad_FromVideoFrame(
      shared_state, video_frame, alpha_value, tex_coord_rect, render_pass,
      video_resource_updater, rect, visible_rect, resource_provider,
      child_resource_provider, child_context_provider);
}

// Creates a video frame of size background_size filled with yuv_background,
// and then draws a foreground rectangle in a different color on top of
// that. The foreground rectangle must have coordinates that are divisible
// by 2 because YUV is a block format.
void CreateTestYUVVideoDrawQuad_TwoColor(
    const SharedQuadState* shared_state,
    media::VideoPixelFormat format,
    gfx::ColorSpace color_space,
    bool is_transparent,
    const gfx::RectF& tex_coord_rect,
    const gfx::Size& background_size,
    const gfx::Rect& visible_rect,
    uint8_t y_background,
    uint8_t u_background,
    uint8_t v_background,
    const gfx::Rect& foreground_rect,
    uint8_t y_foreground,
    uint8_t u_foreground,
    uint8_t v_foreground,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  const gfx::Rect rect(background_size);

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateFrame(format, background_size, foreground_rect,
                                     foreground_rect.size(), base::TimeDelta());
  video_frame->set_color_space(color_space);

  int planes[] = {media::VideoFrame::kYPlane, media::VideoFrame::kUPlane,
                  media::VideoFrame::kVPlane};
  uint8_t yuv_background[] = {y_background, u_background, v_background};
  uint8_t yuv_foreground[] = {y_foreground, u_foreground, v_foreground};
  int sample_size[] = {1, 2, 2};

  for (int i = 0; i < 3; ++i) {
    memset(video_frame->writable_data(planes[i]), yuv_background[i],
           video_frame->stride(planes[i]) * video_frame->rows(planes[i]));
  }

  for (int i = 0; i < 3; ++i) {
    // Since yuv encoding uses block encoding, widths have to be divisible
    // by the sample size in order for this function to behave properly.
    DCHECK_EQ(foreground_rect.x() % sample_size[i], 0);
    DCHECK_EQ(foreground_rect.y() % sample_size[i], 0);
    DCHECK_EQ(foreground_rect.width() % sample_size[i], 0);
    DCHECK_EQ(foreground_rect.height() % sample_size[i], 0);

    gfx::Rect sample_rect(foreground_rect.x() / sample_size[i],
                          foreground_rect.y() / sample_size[i],
                          foreground_rect.width() / sample_size[i],
                          foreground_rect.height() / sample_size[i]);
    for (int y = sample_rect.y(); y < sample_rect.bottom(); ++y) {
      for (int x = sample_rect.x(); x < sample_rect.right(); ++x) {
        size_t offset = y * video_frame->stride(planes[i]) + x;
        video_frame->writable_data(planes[i])[offset] = yuv_foreground[i];
      }
    }
  }

  uint8_t alpha_value = 255;
  CreateTestYUVVideoDrawQuad_FromVideoFrame(
      shared_state, video_frame, alpha_value, tex_coord_rect, render_pass,
      video_resource_updater, rect, visible_rect, resource_provider,
      child_resource_provider, child_context_provider);
}

void CreateTestYUVVideoDrawQuad_Solid(
    const SharedQuadState* shared_state,
    media::VideoPixelFormat format,
    const gfx::ColorSpace& color_space,
    bool is_transparent,
    const gfx::RectF& tex_coord_rect,
    uint8_t y,
    uint8_t u,
    uint8_t v,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  scoped_refptr<media::VideoFrame> video_frame = media::VideoFrame::CreateFrame(
      format, rect.size(), rect, rect.size(), base::TimeDelta());
  video_frame->set_color_space(color_space);

  // YUV values of a solid, constant, color. Useful for testing that color
  // space/color range are being handled properly.
  memset(video_frame->writable_data(media::VideoFrame::kYPlane), y,
         video_frame->stride(media::VideoFrame::kYPlane) *
             video_frame->rows(media::VideoFrame::kYPlane));
  memset(video_frame->writable_data(media::VideoFrame::kUPlane), u,
         video_frame->stride(media::VideoFrame::kUPlane) *
             video_frame->rows(media::VideoFrame::kUPlane));
  memset(video_frame->writable_data(media::VideoFrame::kVPlane), v,
         video_frame->stride(media::VideoFrame::kVPlane) *
             video_frame->rows(media::VideoFrame::kVPlane));

  uint8_t alpha_value = is_transparent ? 0 : 128;
  CreateTestYUVVideoDrawQuad_FromVideoFrame(
      shared_state, video_frame, alpha_value, tex_coord_rect, render_pass,
      video_resource_updater, rect, visible_rect, resource_provider,
      child_resource_provider, child_context_provider);
}

void CreateTestYUVVideoDrawQuad_NV12(
    const SharedQuadState* shared_state,
    const gfx::ColorSpace& color_space,
    const gfx::RectF& tex_coord_rect,
    uint8_t y,
    uint8_t u,
    uint8_t v,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    scoped_refptr<ContextProvider> child_context_provider) {
  bool needs_blending = true;
  const gfx::Size ya_tex_size = rect.size();
  const gfx::Size uv_tex_size = media::VideoFrame::PlaneSizeInSamples(
      media::PIXEL_FORMAT_NV12, media::VideoFrame::kUVPlane, rect.size());
  const gfx::Size uv_sample_size = media::VideoFrame::SampleSize(
      media::PIXEL_FORMAT_NV12, media::VideoFrame::kUVPlane);

  std::vector<uint8_t> y_pixels(ya_tex_size.GetArea(), y);
  ResourceId resource_y = CreateGpuResource(
      child_context_provider, child_resource_provider, ya_tex_size,
      video_resource_updater->YuvSharedImageFormat(8), color_space, y_pixels);

  // U goes in the R component and V goes in the G component.
  uint32_t rgba_pixel = (u << 24) | (v << 16);
  std::vector<uint32_t> uv_pixels(uv_tex_size.GetArea(), rgba_pixel);
  ResourceId resource_u = CreateGpuResource(
      child_context_provider, child_resource_provider, uv_tex_size,
      SinglePlaneFormat::kRGBA_8888, color_space, MakePixelSpan(uv_pixels));
  ResourceId resource_v = resource_u;
  ResourceId resource_a = kInvalidResourceId;

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_y, resource_u, resource_v}, resource_provider,
          child_resource_provider, child_context_provider.get());

  ResourceId mapped_resource_y = resource_map[resource_y];
  ResourceId mapped_resource_u = resource_map[resource_u];
  ResourceId mapped_resource_v = resource_map[resource_v];

  const gfx::Rect video_frame_visible_rect = gfx::ToNearestRect(
      gfx::RectF(tex_coord_rect.x() * ya_tex_size.width(),
                 tex_coord_rect.y() * ya_tex_size.height(),
                 tex_coord_rect.width() * ya_tex_size.width(),
                 tex_coord_rect.height() * ya_tex_size.height()));

  auto* yuv_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  yuv_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                   ya_tex_size, video_frame_visible_rect, uv_sample_size,
                   mapped_resource_y, mapped_resource_u, mapped_resource_v,
                   resource_a, color_space, 0.0f, 1.0f, 8,
                   gfx::ProtectedVideoType::kClear, absl::nullopt);
}

void CreateTestY16TextureDrawQuad_TwoColor(
    const SharedQuadState* shared_state,
    const gfx::RectF& tex_coord_rect,
    uint8_t g_foreground,
    uint8_t g_background,
    AggregatedRenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    const gfx::Rect& foreground_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    ContextProvider* child_context_provider) {
  std::unique_ptr<unsigned char, base::AlignedFreeDeleter> memory(
      static_cast<unsigned char*>(
          base::AlignedAlloc(rect.size().GetArea() * 2,
                             media::VideoFrame::kFrameAddressAlignment)));
  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::WrapExternalData(
          media::PIXEL_FORMAT_Y16, rect.size(), visible_rect,
          visible_rect.size(), memory.get(), rect.size().GetArea() * 2,
          base::TimeDelta());
  DCHECK_EQ(video_frame->rows(0) % 2, 0);
  DCHECK_EQ(video_frame->stride(0) % 2, 0);

  for (int j = 0; j < video_frame->rows(0); ++j) {
    uint8_t* row = video_frame->writable_data(0) + j * video_frame->stride(0);
    if (j < foreground_rect.y() || j >= foreground_rect.bottom()) {
      for (int i = 0; i < video_frame->stride(0) / 2; ++i) {
        *row++ = i & 0xFF;  // Fill R with anything. It is not rendered.
        *row++ = g_background;
      }
    } else {
      for (int i = 0;
           i < std::min(video_frame->stride(0) / 2, foreground_rect.x()); ++i) {
        *row++ = i & 0xFF;
        *row++ = g_background;
      }
      for (int i = foreground_rect.x();
           i < std::min(video_frame->stride(0) / 2, foreground_rect.right());
           ++i) {
        *row++ = i & 0xFF;
        *row++ = g_foreground;
      }
      for (int i = foreground_rect.right(); i < video_frame->stride(0) / 2;
           ++i) {
        *row++ = i & 0xFF;
        *row++ = g_background;
      }
    }
  }

  CreateTestY16TextureDrawQuad_FromVideoFrame(
      shared_state, video_frame, tex_coord_rect, render_pass,
      video_resource_updater, rect, visible_rect, resource_provider,
      child_resource_provider, child_context_provider);
}

// Create two quads of specified colors on half-pixel boundaries.
void CreateTestAxisAlignedQuads(const gfx::Rect& rect,
                                SkColor4f front_color,
                                SkColor4f back_color,
                                bool needs_blending,
                                bool force_aa_off,
                                AggregatedRenderPass* pass) {
  gfx::Transform front_quad_to_target_transform;
  front_quad_to_target_transform.Translate(50, 50);
  front_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                       0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* front_shared_state =
      CreateTestSharedQuadState(front_quad_to_target_transform, rect, pass,
                                gfx::MaskFilterInfo());

  auto* front = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  front->SetAll(front_shared_state, rect, rect, needs_blending, front_color,
                force_aa_off);

  gfx::Transform back_quad_to_target_transform;
  back_quad_to_target_transform.Translate(25.5f, 25.5f);
  back_quad_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* back_shared_state =
      CreateTestSharedQuadState(back_quad_to_target_transform, rect, pass,
                                gfx::MaskFilterInfo());

  auto* back = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  back->SetAll(back_shared_state, rect, rect, needs_blending, back_color,
               force_aa_off);
}

using RendererPixelTest = VizPixelTestWithParam;
INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTest,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

using GPURendererPixelTest = VizPixelTestWithParam;
INSTANTIATE_TEST_SUITE_P(,
                         GPURendererPixelTest,
                         // TODO(crbug.com/1021566): Enable these tests for
                         // SkiaRenderer Dawn once video is supported.
                         testing::ValuesIn(GetGpuRendererTypesNoDawn()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypesNoDawn() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(GPURendererPixelTest);

TEST_P(RendererPixelTest, SimpleGreenRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, SimpleGreenRectNonRootRenderPass) {
  gfx::Rect rect(this->device_viewport_size_);
  gfx::Rect small_rect(100, 100);

  AggregatedRenderPassId child_id{2};
  auto child_pass =
      CreateTestRenderPass(child_id, small_rect, gfx::Transform());

  SharedQuadState* child_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), small_rect, child_pass.get(),
                                gfx::MaskFilterInfo());

  auto* color_quad = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(child_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassId root_id{1};
  auto root_pass = CreateTestRenderPass(root_id, rect, gfx::Transform());

  SharedQuadState* root_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  CreateTestRenderPassDrawQuad(root_shared_state, small_rect, child_id,
                               root_pass.get());

  auto* child_pass_ptr = child_pass.get();

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list, child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("green_small.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, PremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      {0.0f, 1.0f, 0.0f, 0.5f},  // Texel color.
      SkColors::kTransparent,    // Background color.
      true,                      // Premultiplied alpha.
      shared_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, PremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 255, 120)),  // Texel color.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, TextureDrawQuadVisibleRectInsetTopLeft) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());
  pass->quad_list.front()->visible_rect.Inset(gfx::Insets::TLBR(50, 30, 0, 0));
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("inset_top_left.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This tests drawing a TextureDrawQuad with a visible_rect strictly included in
// rect, custom UVs, and rect.origin() that is not in the origin.
TEST_P(RendererPixelTest,
       TextureDrawQuadTranslatedAndVisibleRectInsetTopLeftAndCustomUV) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());
  auto* quad = static_cast<TextureDrawQuad*>(pass->quad_list.front());
  quad->rect.Offset(10, 10);
  quad->visible_rect.Offset(10, 10);
  quad->visible_rect.Inset(gfx::Insets::TLBR(50, 30, 12, 12));
  quad->uv_top_left.SetPoint(.2, .3);
  quad->uv_bottom_right.SetPoint(.4, .7);
  quad->nearest_neighbor = true;  // To avoid bilinear filter differences.
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("offset_inset_top_left.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, TextureDrawQuadVisibleRectInsetBottomRight) {
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
  // Test is flaking with failed large allocations under TSAN when using
  // SkiaRenderer with GL backend. See https://crbug.com/1320955.
  if (renderer_type() == RendererType::kSkiaGL)
    return;
#endif

  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(0, 120, 255, 255)),  // Texel color 1.
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 0, 255)),  // Texel color 2.
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      false,             // flipped_texture_quad.
      false,             // Half and half.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());
  pass->quad_list.front()->visible_rect.Inset(gfx::Insets::TLBR(0, 0, 60, 40));
  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("inset_bottom_right.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorBlend) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  pass->has_transparent_background = false;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 1 - 16.0f / 255;
  shared_state->blend_mode = SkBlendMode::kDstOut;

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kRed, false);

  SharedQuadState* shared_state_background = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  SkColor4f background_color =
      SkColor4f::FromColor(SkColorSetRGB(0xff, 0xff * 14 / 16, 0xff));
  auto* color_quad_background =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad_background->SetNew(shared_state_background, rect, rect,
                                background_color, false);
  // Result should be r=16, g=14, b=16.

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorWithTemperature) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kYellow, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("temperature_brown.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, SolidColorWithTemperatureNonRootRenderPass) {
  // Create a root and a child passes with two different solid color quads.
  AggregatedRenderPassList render_passes_in_draw_order;
  gfx::Rect viewport_rect(this->device_viewport_size_);
  gfx::Rect root_rect(0, 0, viewport_rect.width(), viewport_rect.height() / 2);
  gfx::Rect child_rect(0, root_rect.bottom(), viewport_rect.width(),
                       root_rect.height());

  // Child pass.
  AggregatedRenderPassId child_pass_id{2};
  AggregatedRenderPass* child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order, child_pass_id, viewport_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, child_rect, SkColors::kGreen);

  // Root pass.
  AggregatedRenderPassId root_pass_id{1};
  AggregatedRenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order, root_pass_id, viewport_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, root_rect, SkColors::kYellow);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), viewport_rect, root_pass,
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, viewport_rect, child_pass_id,
                               root_pass);

  // Set a non-identity output color matrix on the output surface, and expect
  // that the colors will be transformed.
  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &render_passes_in_draw_order,
      base::FilePath(FILE_PATH_LITERAL("temperature_brown_non_root.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest,
       PremultipliedTextureWithBackgroundAndVertexOpacity) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  texture_quad_state->opacity = 0.8f;

  float vertex_opacity[4] = {1.f, 1.f, 0.f, 0.f};
  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 255, 120)),  // SK_ColorT
      vertex_opacity,
      SkColors::kGreen,  // Background color.
      true,              // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha_vertex_opacity.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

class IntersectingQuadPixelTest : public VizPixelTestWithParam {
 protected:
  void SetupQuadStateAndRenderPass() {
    // This sets up a pair of draw quads. They are both rotated
    // relative to the root plane, they are also rotated relative to each other.
    // The intersect in the middle at a non-perpendicular angle so that any
    // errors are hopefully magnified.
    // The quads should intersect correctly, as in the front quad should only
    // be partially in front of the back quad, and partially behind.

    viewport_rect_ = gfx::Rect(this->device_viewport_size_);
    quad_rect_ = gfx::Rect(0, 0, this->device_viewport_size_.width(),
                           this->device_viewport_size_.height() / 2.0);

    AggregatedRenderPassId id{1};
    render_pass_ = CreateTestRootRenderPass(id, viewport_rect_);

    // Create the front quad rotated on the Z and Y axis.
    gfx::Transform trans;
    trans.Translate3d(0, 0, 0.707 * this->device_viewport_size_.width() / 2.0);
    trans.RotateAboutZAxis(45.0);
    trans.RotateAboutYAxis(45.0);
    front_quad_state_ = CreateTestSharedQuadState(
        trans, viewport_rect_, render_pass_.get(), gfx::MaskFilterInfo());
    // Make sure they end up in a 3d sorting context.
    front_quad_state_->sorting_context_id = 1;

    // Create the back quad, and rotate on just the y axis. This will intersect
    // the first quad partially.
    trans = gfx::Transform();
    trans.Translate3d(0, 0, -0.707 * this->device_viewport_size_.width() / 2.0);
    trans.RotateAboutYAxis(-45.0);
    back_quad_state_ =
        CreateTestSharedQuadState(trans, viewport_rect_, render_pass_.get(),
                                  gfx::MaskFilterInfo());
    back_quad_state_->sorting_context_id = 1;
  }
  void AppendBackgroundAndRunTest(const cc::PixelComparator& comparator,
                                  const base::FilePath& ref_file) {
    SharedQuadState* background_quad_state =
        CreateTestSharedQuadState(gfx::Transform(), viewport_rect_,
                                  render_pass_.get(), gfx::MaskFilterInfo());
    auto* background_quad =
        render_pass_->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(background_quad_state, viewport_rect_,
                            viewport_rect_, SkColors::kWhite, false);
    pass_list_.push_back(std::move(render_pass_));
    EXPECT_TRUE(this->RunPixelTest(&pass_list_, ref_file, comparator));
  }
  template <typename T>
  T* CreateAndAppendDrawQuad() {
    return render_pass_->CreateAndAppendDrawQuad<T>();
  }

  std::unique_ptr<AggregatedRenderPass> render_pass_;
  gfx::Rect viewport_rect_;
  raw_ptr<SharedQuadState> front_quad_state_;
  raw_ptr<SharedQuadState> back_quad_state_;
  gfx::Rect quad_rect_;
  AggregatedRenderPassList pass_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingQuadPixelTest,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

class IntersectingVideoQuadPixelTest : public IntersectingQuadPixelTest {
 public:
  void SetUp() override {
    IntersectingQuadPixelTest::SetUp();
    constexpr bool kUseStreamVideoDrawQuad = false;
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr bool kUseR16Texture = false;
    constexpr int kMaxResourceSize = 10000;

    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(),
        /*raster_context_provider=*/nullptr, nullptr,
        this->child_resource_provider_.get(), kUseStreamVideoDrawQuad,
        kUseGpuMemoryBufferResources, kUseR16Texture, kMaxResourceSize);
    video_resource_updater2_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(),
        /*raster_context_provider=*/nullptr, nullptr,
        this->child_resource_provider_.get(), kUseStreamVideoDrawQuad,
        kUseGpuMemoryBufferResources, kUseR16Texture, kMaxResourceSize);
  }

 protected:
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater2_;
};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingVideoQuadPixelTest,
                         // TODO(crbug.com/1021566): Enable these tests for
                         // SkiaRenderer Dawn once video is supported.
                         testing::ValuesIn(GetGpuRendererTypesNoDawn()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypesNoDawn() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(IntersectingVideoQuadPixelTest);

class IntersectingQuadSoftwareTest : public IntersectingQuadPixelTest {};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingQuadSoftwareTest,
                         testing::Values(RendererType::kSoftware),
                         testing::PrintToStringParamName());

TEST_P(IntersectingQuadPixelTest, SolidColorQuads) {
  // TODO(crbug.com/1021566): Enable this test for SkiaRenderer Dawn.
  if (renderer_type() == RendererType::kSkiaDawn)
    return;

  this->SetupQuadStateAndRenderPass();

  auto* quad = this->template CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* quad2 = this->template CreateAndAppendDrawQuad<SolidColorDrawQuad>();

  quad->SetNew(this->front_quad_state_, this->quad_rect_, this->quad_rect_,
               SkColors::kBlue, false);
  quad2->SetNew(this->back_quad_state_, this->quad_rect_, this->quad_rect_,
                SkColors::kGreen, false);
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL("intersecting_blue_green.png")));
}

TEST_P(IntersectingQuadPixelTest, TexturedQuads) {
  this->SetupQuadStateAndRenderPass();
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_, SkColors::kBlack,
      SkColors::kBlue, SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      this->front_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_, SkColors::kGreen,
      SkColors::kBlack, SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());

  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL("intersecting_blue_green_squares.png")));
}

TEST_P(IntersectingQuadPixelTest, NonFlippedTexturedQuads) {
  this->SetupQuadStateAndRenderPass();
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 255)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, true /* half_and_half */,
      this->front_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, true /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());

  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL(
          "intersecting_non_flipped_blue_green_half_size_rectangles.png")));
}

TEST_P(IntersectingQuadPixelTest, FlippedTexturedQuads) {
  this->SetupQuadStateAndRenderPass();
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 255)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      true /* flipped_texture_quad */, true /* half_and_half */,
      this->front_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      true /* flipped_texture_quad */, true /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());

  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL(
          "intersecting_flipped_blue_green_half_size_rectangles.png")));
}

TEST_P(IntersectingQuadSoftwareTest, PictureQuads) {
  bool needs_blending = true;
  this->SetupQuadStateAndRenderPass();
  gfx::Rect outer_rect(this->quad_rect_);
  gfx::Rect inner_rect(this->quad_rect_.x() + (this->quad_rect_.width() / 4),
                       this->quad_rect_.y() + (this->quad_rect_.height() / 4),
                       this->quad_rect_.width() / 2,
                       this->quad_rect_.height() / 2);

  cc::PaintFlags black_flags;
  black_flags.setColor(SkColors::kBlack);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);

  std::unique_ptr<cc::FakeRecordingSource> blue_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(
          this->quad_rect_.size());
  blue_recording->add_draw_rect_with_flags(outer_rect, black_flags);
  blue_recording->add_draw_rect_with_flags(inner_rect, blue_flags);
  blue_recording->Rerecord();
  scoped_refptr<cc::RasterSource> blue_raster_source =
      blue_recording->CreateRasterSource();

  auto* blue_quad =
      this->render_pass_->template CreateAndAppendDrawQuad<PictureDrawQuad>();

  blue_quad->SetNew(this->front_quad_state_, this->quad_rect_, this->quad_rect_,
                    needs_blending, gfx::RectF(this->quad_rect_),
                    this->quad_rect_.size(), false, RGBA_8888, this->quad_rect_,
                    1.f, {}, blue_raster_source->GetDisplayItemList());

  std::unique_ptr<cc::FakeRecordingSource> green_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(
          this->quad_rect_.size());
  green_recording->add_draw_rect_with_flags(outer_rect, green_flags);
  green_recording->add_draw_rect_with_flags(inner_rect, black_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  auto* green_quad =
      this->render_pass_->template CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(this->back_quad_state_, this->quad_rect_, this->quad_rect_,
                     needs_blending, gfx::RectF(this->quad_rect_),
                     this->quad_rect_.size(), false, RGBA_8888,
                     this->quad_rect_, 1.f, {},
                     green_raster_source->GetDisplayItemList());
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL("intersecting_blue_green_squares.png")));
}

TEST_P(IntersectingQuadPixelTest, RenderPassQuads) {
  this->SetupQuadStateAndRenderPass();
  AggregatedRenderPassId child_pass_id1{2};
  AggregatedRenderPassId child_pass_id2{3};
  auto child_pass1 =
      CreateTestRenderPass(child_pass_id1, this->quad_rect_, gfx::Transform());
  SharedQuadState* child1_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), this->quad_rect_, child_pass1.get(), gfx::MaskFilterInfo());
  auto child_pass2 =
      CreateTestRenderPass(child_pass_id2, this->quad_rect_, gfx::Transform());
  SharedQuadState* child2_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), this->quad_rect_, child_pass2.get(), gfx::MaskFilterInfo());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 255)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      child1_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, child_pass1.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      child2_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, child_pass2.get());

  CreateTestRenderPassDrawQuad(this->front_quad_state_, this->quad_rect_,
                               child_pass_id1, this->render_pass_.get());
  CreateTestRenderPassDrawQuad(this->back_quad_state_, this->quad_rect_,
                               child_pass_id2, this->render_pass_.get());

  this->pass_list_.push_back(std::move(child_pass1));
  this->pass_list_.push_back(std::move(child_pass2));
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f),
      base::FilePath(FILE_PATH_LITERAL("intersecting_blue_green_squares.png")));
}

TEST_P(IntersectingVideoQuadPixelTest, YUVVideoQuads) {
  this->SetupQuadStateAndRenderPass();
  gfx::Rect inner_rect(
      ((this->quad_rect_.x() + (this->quad_rect_.width() / 4)) & ~0xF),
      ((this->quad_rect_.y() + (this->quad_rect_.height() / 4)) & ~0xF),
      (this->quad_rect_.width() / 2) & ~0xF,
      (this->quad_rect_.height() / 2) & ~0xF);

  CreateTestYUVVideoDrawQuad_TwoColor(
      this->front_quad_state_, media::PIXEL_FORMAT_I420,
      gfx::ColorSpace::CreateJpeg(), false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f),
      this->quad_rect_.size(), this->quad_rect_, 0, 128, 128, inner_rect, 29,
      255, 107, this->render_pass_.get(), this->video_resource_updater_.get(),
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  CreateTestYUVVideoDrawQuad_TwoColor(
      this->back_quad_state_, media::PIXEL_FORMAT_I420,
      gfx::ColorSpace::CreateJpeg(), false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f),
      this->quad_rect_.size(), this->quad_rect_, 149, 43, 21, inner_rect, 0,
      128, 128, this->render_pass_.get(), this->video_resource_updater2_.get(),
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  base::FilePath baseline = base::FilePath(
      FILE_PATH_LITERAL("intersecting_blue_green_squares_video.png"));

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  this->AppendBackgroundAndRunTest(cc::FuzzyPixelComparator()
                                       .DiscardAlpha()
                                       .SetErrorPixelsPercentageLimit(0.50f)
                                       .SetAvgAbsErrorLimit(1.2f)
                                       .SetAbsErrorLimit(2),
                                   baseline);
}

TEST_P(IntersectingVideoQuadPixelTest, Y16VideoQuads) {
  this->SetupQuadStateAndRenderPass();
  gfx::Rect inner_rect(
      ((this->quad_rect_.x() + (this->quad_rect_.width() / 4)) & ~0xF),
      ((this->quad_rect_.y() + (this->quad_rect_.height() / 4)) & ~0xF),
      (this->quad_rect_.width() / 2) & ~0xF,
      (this->quad_rect_.height() / 2) & ~0xF);

  CreateTestY16TextureDrawQuad_TwoColor(
      this->front_quad_state_, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 18, 0,
      this->render_pass_.get(), this->video_resource_updater_.get(),
      this->quad_rect_, this->quad_rect_, inner_rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  CreateTestY16TextureDrawQuad_TwoColor(
      this->back_quad_state_, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 0, 182,
      this->render_pass_.get(), this->video_resource_updater2_.get(),
      this->quad_rect_, this->quad_rect_, inner_rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  base::FilePath baseline = base::FilePath(
      FILE_PATH_LITERAL("intersecting_light_dark_squares_video.png"));

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  this->AppendBackgroundAndRunTest(cc::FuzzyPixelOffByOneComparator(),
                                   baseline);
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_P(GPURendererPixelTest, NonPremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      {0.0f, 1.0f, 0.0f, 0.5f},  // Texel color.
      SkColors::kTransparent,    // Background color.
      false,                     // Premultiplied alpha.
      shared_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_P(GPURendererPixelTest, NonPremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(204, 120, 255, 120)),  // Texel color.
      SkColors::kGreen,  // Background color.
      false,             // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

class VideoRendererPixelTestBase : public VizPixelTest {
 public:
  explicit VideoRendererPixelTestBase(RendererType type) : VizPixelTest(type) {}

 protected:
  // Include the protected member variables from the parent class.
  using cc::PixelTest::child_context_provider_;
  using cc::PixelTest::child_resource_provider_;
  using cc::PixelTest::resource_provider_;

  void CreateEdgeBleedPass(media::VideoPixelFormat format,
                           const gfx::ColorSpace& color_space,
                           AggregatedRenderPassList* pass_list) {
    gfx::Rect rect(200, 200);

    AggregatedRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);

    // Scale the video up so that bilinear filtering kicks in to sample more
    // than just nearest neighbor would.
    gfx::Transform scale_by_2;
    scale_by_2.Scale(2.f, 2.f);
    gfx::Rect half_rect(100, 100);
    SharedQuadState* shared_state =
        CreateTestSharedQuadState(scale_by_2, half_rect, pass.get(),
                                  gfx::MaskFilterInfo());

    gfx::Size background_size(200, 200);
    gfx::Rect green_rect(16, 20, 100, 100);
    gfx::RectF tex_coord_rect(
        static_cast<float>(green_rect.x()) / background_size.width(),
        static_cast<float>(green_rect.y()) / background_size.height(),
        static_cast<float>(green_rect.width()) / background_size.width(),
        static_cast<float>(green_rect.height()) / background_size.height());

    // YUV of (149,43,21) should be green (0,255,0) in RGB.
    // Create a video frame that has a non-green background rect, with a
    // green sub-rectangle that should be the only thing displayed in
    // the final image.  Bleeding will appear on all four sides of the video
    // if the tex coords are not clamped.
    CreateTestYUVVideoDrawQuad_TwoColor(
        shared_state, format, color_space, false, tex_coord_rect,
        background_size, gfx::Rect(background_size), 128, 128, 128, green_rect,
        149, 43, 21, pass.get(), video_resource_updater_.get(),
        resource_provider_.get(), child_resource_provider_.get(),
        child_context_provider_.get());
    pass_list->push_back(std::move(pass));
  }

  void SetUp() override {
    VizPixelTest::SetUp();
    constexpr bool kUseStreamVideoDrawQuad = false;
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr bool kUseR16Texture = false;
    constexpr int kMaxResourceSize = 10000;
    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        child_context_provider_.get(), nullptr, nullptr,
        child_resource_provider_.get(), kUseStreamVideoDrawQuad,
        kUseGpuMemoryBufferResources, kUseR16Texture, kMaxResourceSize);
  }

  void TearDown() override {
    video_resource_updater_ = nullptr;
    VizPixelTest::TearDown();
  }

  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
};

#if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)
class VideoRendererPixelHiLoTest : public VideoRendererPixelTestBase,
                                   public testing::WithParamInterface<bool> {
 public:
  VideoRendererPixelHiLoTest()
      : VideoRendererPixelTestBase(RendererType::kSkiaGL) {}

  bool IsHighbit() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(, VideoRendererPixelHiLoTest, testing::Bool());

TEST_P(VideoRendererPixelHiLoTest, SimpleYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, IsHighbit(), gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1421171): currently failing on iOS.
#define MAYBE_ClippedYUVRect DISABLED_ClippedYUVRect
#else
#define MAYBE_ClippedYUVRect ClippedYUVRect
#endif  // BUILDFLAG(IS_IOS)
TEST_P(VideoRendererPixelHiLoTest, MAYBE_ClippedYUVRect) {
  gfx::Rect viewport(this->device_viewport_size_);
  gfx::Rect draw_rect(this->device_viewport_size_.width() * 1.5,
                      this->device_viewport_size_.height() * 1.5);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, viewport);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), viewport, pass.get(),
                                gfx::MaskFilterInfo());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, IsHighbit(), gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      this->video_resource_updater_.get(), draw_rect, viewport,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());
  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_clipped.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}
#endif  // #if BUILDFLAG(ENABLE_GL_BACKEND_TESTS)

class VideoRendererPixelTest
    : public VideoRendererPixelTestBase,
      public testing::WithParamInterface<RendererType> {
 public:
  VideoRendererPixelTest() : VideoRendererPixelTestBase(GetParam()) {}
};

INSTANTIATE_TEST_SUITE_P(,
                         VideoRendererPixelTest,
                         // TODO(crbug.com/1021566): Enable these tests for
                         // SkiaRenderer Dawn once video is supported.
                         testing::ValuesIn(GetGpuRendererTypesNoDawn()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypesNoDawn() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(VideoRendererPixelTest);

TEST_P(VideoRendererPixelTest, OffsetYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // Intentionally sets frame format to I420 for testing coverage.
  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, false, gfx::RectF(0.125f, 0.25f, 0.75f, 0.5f), pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_offset.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVRectBlack) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // In MPEG color range YUV values of (15,128,128) should produce black.
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 15, 128, 128, pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // If we didn't get black out of the YUV values above, then we probably have a
  // color range issue.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("black.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 149, 43, 21, pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRectWithTemperature) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // YUV of (225,0,148) should be yellow (255,255,0) in RGB.
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 225, 0, 148, pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  SkM44 color_matrix;
  color_matrix.setRC(0, 0, 0.7f);
  color_matrix.setRC(1, 1, 0.4f);
  color_matrix.setRC(2, 2, 0.5f);
  this->output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("temperature_brown.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleNV12JRect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  CreateTestYUVVideoDrawQuad_NV12(
      shared_state, gfx::ColorSpace::CreateJpeg(),
      gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 149, 43, 21, pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Test that a YUV video doesn't bleed outside of its tex coords when the
// tex coord rect is only a partial subrectangle of the coded contents.
TEST_P(VideoRendererPixelTest, YUVEdgeBleed) {
  AggregatedRenderPassList pass_list;
  this->CreateEdgeBleedPass(media::PIXEL_FORMAT_I420,
                            gfx::ColorSpace::CreateJpeg(), &pass_list);
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, YUVAEdgeBleed) {
  AggregatedRenderPassList pass_list;
  this->CreateEdgeBleedPass(media::PIXEL_FORMAT_I420A,
                            gfx::ColorSpace::CreateREC601(), &pass_list);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVJRectGrey) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // Dark grey in JPEG color range (in MPEG, this is black).
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 15, 128, 128, pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, SimpleYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420A, gfx::ColorSpace::CreateREC601(),
      false, false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(VideoRendererPixelTest, FullyTransparentYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  this->display_color_spaces_ = kRec601DisplayColorSpaces;

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420A, gfx::ColorSpace::CreateREC601(),
      true, false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      this->video_resource_updater_.get(), rect, rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kBlack, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("black.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(VideoRendererPixelTest, TwoColorY16Rect) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  gfx::Rect upper_rect(rect.x(), rect.y(), rect.width(), rect.height() / 2);
  CreateTestY16TextureDrawQuad_TwoColor(
      shared_state, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 68, 123, pass.get(),
      this->video_resource_updater_.get(), rect, rect, upper_rect,
      this->resource_provider_.get(), this->child_resource_provider_.get(),
      this->child_context_provider_.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassColorFilterAlpha) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  float matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = matrix[4] = 0;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = matrix[9] = 0;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = matrix[14] = 0;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(SkColorFilters::Matrix(matrix),
                                             nullptr)));

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false, 1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassSaturateFilter) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateSaturateFilter(0.5f));

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false, 1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassFilterChain) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateGrayscaleFilter(1.f));
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(0.5f));

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false, 1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, FastPassColorFilterAlphaTranslation) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  float matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = 0;
  matrix[4] = 20.f / 255;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = 0;
  matrix[9] = 200.f / 255;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = 0;
  matrix[14] = 1.5f / 255;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(SkColorFilters::Matrix(matrix),
                                             nullptr)));

  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SkColors::kWhite,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, kInvalidResourceId, gfx::RectF(),
                           gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false, 1.0f);

  AggregatedRenderPassList pass_list;

  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha_translate.png")),
      cc::FuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, EnlargedRenderPassTexture) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTest, EnlargedRenderPassTextureWithAntiAliasing) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  gfx::Transform aa_transform;
  aa_transform.Translate(0.5, 0.0);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(aa_transform, pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  SharedQuadState* root_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  auto* background = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  background->SetNew(root_shared_state, gfx::Rect(this->device_viewport_size_),
                     gfx::Rect(this->device_viewport_size_), SkColors::kWhite,
                     false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_anti_aliasing.png")),
      cc::FuzzyPixelComparator()
          .DiscardAlpha()
          .SetErrorPixelsPercentageLimit(100.f)
          .SetAvgAbsErrorLimit(5.f)
          .SetAbsErrorLimit(7)));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TEST_P(RendererPixelTest, RenderPassAndMaskWithPartialQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a green box.
  static const SkColor4f kCSSGreen = SkColor4f::FromColor(0xff008000);
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
                kCSSGreen, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(4));
  flags.setColor(SkColors::kWhite);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
        MakePixelSpan(bitmap));
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This AggregatedRenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 50, 200, 100);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(sub_rect),                      // tex_coord_rect
      false,                                     // force_anti_aliasing_off
      1.0f);                                     // backdrop_filter_quality
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_bottom_right.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TEST_P(RendererPixelTest, RenderPassAndMaskWithPartialQuad2) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a green box.
  static const SkColor4f kCSSGreen = SkColor4f::FromColor(0xff008000);
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
                kCSSGreen, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setStrokeWidth(SkIntToScalar(4));
  flags.setColor(SkColors::kWhite);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
        MakePixelSpan(bitmap));
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This AggregatedRenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 20, 200, 60);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(sub_rect),                      // tex_coord_rect
      false,                                     // force_anti_aliasing_off
      1.0f);                                     // backdrop_filter_quality
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_middle.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, RenderPassAndMaskForRoundedCorner) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is just a blue box.
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(child_pass_shared_state, viewport_rect, viewport_rect,
               SkColors::kBlue, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColors::kWhite);
  flags.setAntiAlias(true);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rounded_corner_rect = mask_rect;
  rounded_corner_rect.Inset(kInset);
  SkRRect rounded_corner = SkRRect::MakeRectXY(
      gfx::RectToSkRect(rounded_corner_rect), kCornerRadius, kCornerRadius);
  canvas.drawRRect(rounded_corner, flags);

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
        MakePixelSpan(bitmap));
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, viewport_rect, viewport_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                     1.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(viewport_rect),                 // tex_coord_rect
      false,                                     // force_anti_aliasing_off
      1.0f);                                     // backdrop_filter_quality
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // The rounded corners generated by masks should be very close to the rounded
  // corners generated by the fragment shader approach. The percentage of pixel
  // mismatch is around 0.52%.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RenderPassAndMaskForRoundedCornerMultiRadii) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  const SkVector kCornerRadii[4] = {
      SkVector::Make(5.0, 5.0),
      SkVector::Make(15.0, 15.0),
      SkVector::Make(25.0, 25.0),
      SkVector::Make(35.0, 35.0),
  };

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // The child render pass is half a blue box and other half yellow box.
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(child_pass_shared_state, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(child_pass_shared_state, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  // Make a mask.
  gfx::Rect mask_rect = viewport_rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  cc::SkiaPaintCanvas canvas(bitmap);
  cc::PaintFlags flags;
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setColor(SkColors::kWhite);
  flags.setAntiAlias(true);
  canvas.clear(SkColors::kTransparent);
  gfx::Rect rounded_corner_rect = mask_rect;
  rounded_corner_rect.Inset(kInset);
  SkRRect rounded_corner =
      SkRRect::MakeRect(gfx::RectToSkRect(rounded_corner_rect));
  rounded_corner.setRectRadii(rounded_corner.rect(), kCornerRadii);
  canvas.drawRRect(rounded_corner, flags);

  ResourceId mask_resource_id;
  if (!is_software_renderer()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
        MakePixelSpan(bitmap));
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // Set up a mask on the AggregatedRenderPassDrawQuad.
  auto* mask_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, viewport_rect, viewport_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                     1.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(viewport_rect),                 // tex_coord_rect
      false,                                     // force_anti_aliasing_off
      1.0f);                                     // backdrop_filter_quality
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_radii.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

class RendererPixelTestWithBackdropFilter : public VizPixelTestWithParam {
 protected:
  void SetUpRenderPassList() {
    gfx::Rect device_viewport_rect(this->device_viewport_size_);

    AggregatedRenderPassId root_id{1};
    auto root_pass = CreateTestRootRenderPass(root_id, device_viewport_rect);
    root_pass->has_transparent_background = false;

    gfx::Transform identity_quad_to_target_transform;

    AggregatedRenderPassId filter_pass_id{2};
    gfx::Transform transform_to_root;
    auto filter_pass = CreateTestRenderPass(
        filter_pass_id, filter_pass_layer_rect_, transform_to_root);
    filter_pass->backdrop_filters = this->backdrop_filters_;
    filter_pass->backdrop_filter_bounds = this->backdrop_filter_bounds_;

    // A non-visible quad in the filtering render pass.
    {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, filter_pass_layer_rect_,
          filter_pass.get(), gfx::MaskFilterInfo());
      auto* color_quad =
          filter_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, filter_pass_layer_rect_,
                         filter_pass_layer_rect_, SkColors::kTransparent,
                         false);
    }

    ResourceId mapped_mask_resource_id(0);
    gfx::RectF mask_uv_rect;
    gfx::Size mask_texture_size;
    if (include_backdrop_mask_) {
      // Make a mask.
      gfx::Rect viewport_rect(this->device_viewport_size_);
      constexpr int kInset = 20;
      const SkVector kCornerRadii[4] = {
          SkVector::Make(5.0, 5.0),
          SkVector::Make(15.0, 15.0),
          SkVector::Make(25.0, 25.0),
          SkVector::Make(35.0, 35.0),
      };
      gfx::Rect mask_rect = viewport_rect;
      SkBitmap bitmap;
      bitmap.allocPixels(
          SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
      cc::SkiaPaintCanvas canvas(bitmap);
      cc::PaintFlags flags;
      flags.setStyle(cc::PaintFlags::kFill_Style);
      flags.setColor(SkColors::kWhite);
      flags.setAntiAlias(true);
      canvas.clear(SkColors::kTransparent);
      gfx::Rect rounded_corner_rect = mask_rect;
      rounded_corner_rect.Inset(kInset);
      SkRRect rounded_corner =
          SkRRect::MakeRect(gfx::RectToSkRect(rounded_corner_rect));
      rounded_corner.setRectRadii(rounded_corner.rect(), kCornerRadii);
      canvas.drawRRect(rounded_corner, flags);

      ResourceId mask_resource_id;
      if (!is_software_renderer()) {
        mask_resource_id = CreateGpuResource(
            this->child_context_provider_, this->child_resource_provider_.get(),
            mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
            MakePixelSpan(bitmap));
      } else {
        mask_resource_id =
            this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
      }

      // Return the mapped resource id.
      std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
          resource_map = cc::SendResourceAndGetChildToParentMap(
              {mask_resource_id}, this->resource_provider_.get(),
              this->child_resource_provider_.get(),
              this->child_context_provider_.get());
      mapped_mask_resource_id = resource_map[mask_resource_id];

      mask_uv_rect =
          gfx::ScaleRect(gfx::RectF(viewport_rect), 1.f / mask_rect.width(),
                         1.f / mask_rect.height()),  // mask_uv_rect
          mask_texture_size = gfx::Size(mask_rect.size());
    }

    {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          filter_pass_to_target_transform_, filter_pass_layer_rect_,
          filter_pass.get(), gfx::MaskFilterInfo());
      auto* filter_pass_quad =
          root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
      filter_pass_quad->SetNew(shared_state, filter_pass_layer_rect_,
                               filter_pass_layer_rect_, filter_pass_id,
                               mapped_mask_resource_id, mask_uv_rect,
                               mask_texture_size,
                               gfx::Vector2dF(1.0f, 1.0f),  // filters_scale
                               gfx::PointF(),               // filters_origin
                               gfx::RectF(),                // tex_coord_rect
                               false,  // force_anti_aliasing_off
                               1.0f);  // backdrop_filter_quality
    }

    const int kColumnWidth = device_viewport_rect.width() / 3;

    gfx::Rect left_rect = gfx::Rect(0, 0, kColumnWidth, 20);
    while (left_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, left_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, left_rect, left_rect, SkColors::kGreen,
                         false);
      left_rect += gfx::Vector2d(0, left_rect.height() + 1);
    }

    gfx::Rect middle_rect = gfx::Rect(kColumnWidth + 1, 0, kColumnWidth, 20);
    while (middle_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, middle_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, middle_rect, middle_rect, SkColors::kRed,
                         false);
      middle_rect += gfx::Vector2d(0, middle_rect.height() + 1);
    }

    gfx::Rect right_rect =
        gfx::Rect((kColumnWidth + 1) * 2, 0, kColumnWidth, 20);
    while (right_rect.y() < device_viewport_rect.height()) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, right_rect, root_pass.get(),
          gfx::MaskFilterInfo());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, right_rect, right_rect, SkColors::kBlue,
                         false);
      right_rect += gfx::Vector2d(0, right_rect.height() + 1);
    }

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        identity_quad_to_target_transform, device_viewport_rect,
        root_pass.get(), gfx::MaskFilterInfo());
    auto* background_quad =
        root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(shared_state, device_viewport_rect,
                            device_viewport_rect, SkColors::kWhite, false);

    pass_list_.push_back(std::move(filter_pass));
    pass_list_.push_back(std::move(root_pass));
  }

  AggregatedRenderPassList pass_list_;
  cc::FilterOperations backdrop_filters_;
  absl::optional<gfx::RRectF> backdrop_filter_bounds_;
  bool include_backdrop_mask_ = false;
  gfx::Transform filter_pass_to_target_transform_;
  gfx::Rect filter_pass_layer_rect_;
};

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithBackdropFilter,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

TEST_P(RendererPixelTestWithBackdropFilter, InvertFilter) {
  this->backdrop_filters_.Append(cc::FilterOperation::CreateInvertFilter(1.f));
  this->filter_pass_layer_rect_ = gfx::Rect(this->device_viewport_size_);
  this->filter_pass_layer_rect_.Inset(gfx::Insets::TLBR(14, 12, 18, 16));
  this->backdrop_filter_bounds_ =
      gfx::RRectF(gfx::RectF(this->filter_pass_layer_rect_));
  this->SetUpRenderPassList();
  EXPECT_TRUE(this->RunPixelTest(
      &this->pass_list_,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTestWithBackdropFilter, InvertFilterWithMask) {
  this->backdrop_filters_.Append(cc::FilterOperation::CreateInvertFilter(1.f));
  this->filter_pass_layer_rect_ = gfx::Rect(this->device_viewport_size_);
  this->filter_pass_layer_rect_.Inset(gfx::Insets::TLBR(14, 12, 18, 16));
  this->backdrop_filter_bounds_ =
      gfx::RRectF(gfx::RectF(this->filter_pass_layer_rect_));
  this->include_backdrop_mask_ = true;
  this->SetUpRenderPassList();

  base::FilePath expected_path(
      is_software_renderer()
          ? FILE_PATH_LITERAL("backdrop_filter_masked_sw.png")
          : FILE_PATH_LITERAL("backdrop_filter_masked.png"));

  EXPECT_TRUE(this->RunPixelTest(&this->pass_list_, expected_path,
                                 cc::FuzzyPixelOffByOneComparator()));
}

// Software renderer does not support anti-aliased edges.
TEST_P(GPURendererPixelTest, AntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  gfx::Transform red_quad_to_target_transform;
  red_quad_to_target_transform.Rotate(10);
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(red_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, rect, rect, SkColors::kRed, false);

  gfx::Transform yellow_quad_to_target_transform;
  yellow_quad_to_target_transform.Rotate(5);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      yellow_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  auto* yellow = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(yellow_shared_state, rect, rect, SkColors::kYellow, false);

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(blue_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath baseline =
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_.png"))
          .InsertBeforeExtensionASCII(this->renderer_str());

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, baseline, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Software renderer does not support anti-aliased edges.
TEST_P(GPURendererPixelTest, AntiAliasingPerspective) {
  gfx::Rect rect(this->device_viewport_size_);

  auto pass = CreateTestRootRenderPass(AggregatedRenderPassId{1}, rect);

  gfx::Rect red_rect(0, 0, 180, 500);
  auto red_quad_to_target_transform = gfx::Transform::RowMajor(
      1.0f, 2.4520f, 10.6206f, 19.0f, 0.0f, 0.3528f, 5.9737f, 9.5f, 0.0f,
      -0.2250f, -0.9744f, 0.0f, 0.0f, 0.0225f, 0.0974f, 1.0f);
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      red_quad_to_target_transform, red_rect, pass.get(), gfx::MaskFilterInfo());
  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SkColors::kRed, false);

  gfx::Rect green_rect(19, 7, 180, 10);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), green_rect, pass.get(),
                                gfx::MaskFilterInfo());
  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, green_rect, green_rect, SkColors::kGreen,
                false);

  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  base::FilePath baseline =
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_perspective_.png"))
          .InsertBeforeExtensionASCII(this->renderer_str());

  if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
    baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
  }

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, baseline, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This test tests that anti-aliasing works for axis aligned quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, AxisAligned) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);

  CreateTestAxisAlignedQuads(rect, SkColors::kRed, SkColors::kYellow, false,
                             false, pass.get());

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(blue_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SkColors::kBlue, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("axis_aligned.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// solid color draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, SolidColorDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state =
      CreateTestSharedQuadState(hole_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* hole = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SkColors::kTransparent,
               true);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// render pass draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, RenderPassDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  gfx::Transform transform_to_root;
  auto root_pass = CreateTestRenderPass(root_pass_id, rect, transform_to_root);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform child_pass_transform;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, rect, child_pass_transform);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, rect, child_pass.get(), gfx::MaskFilterInfo());
  SolidColorDrawQuad* hole =
      child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SkColors::kTransparent,
               false);

  bool needs_blending = false;
  bool force_anti_aliasing_off = true;
  float backdrop_filter_quality = 1.0f;
  bool intersects_damage_under = true;
  gfx::Transform hole_pass_to_target_transform;
  hole_pass_to_target_transform.Translate(50, 50);
  hole_pass_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      hole_pass_to_target_transform, rect, root_pass.get(), gfx::MaskFilterInfo());
  AggregatedRenderPassDrawQuad* pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  pass_quad->SetAll(pass_shared_state, rect, rect, needs_blending,
                    child_pass_id, kInvalidResourceId, gfx::RectF(),
                    gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                    gfx::RectF(rect), force_anti_aliasing_off,
                    backdrop_filter_quality, intersects_damage_under);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, root_pass.get(), gfx::MaskFilterInfo());

  SolidColorDrawQuad* green =
      root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected for
// tile draw quads.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, TileDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  canvas.clear(SkColors::kTransparent);

  gfx::Size tile_size(32, 32);
  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(this->child_context_provider_,
                                 this->child_resource_provider_.get(),
                                 tile_size, SinglePlaneFormat::kRGBA_8888,
                                 gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  bool contents_premultiplied = true;
  bool needs_blending = false;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = true;
  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state =
      CreateTestSharedQuadState(hole_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());
  TileDrawQuad* hole = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  hole->SetNew(hole_shared_state, rect, rect, needs_blending, mapped_resource,
               gfx::RectF(gfx::Rect(tile_size)), tile_size,
               contents_premultiplied, nearest_neighbor,
               force_anti_aliasing_off);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get(), gfx::MaskFilterInfo());

  SolidColorDrawQuad* green =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This test tests that forcing anti-aliasing off works as expected while
// blending is still enabled.
// Anti-aliasing is only supported in the gl and skia renderers.
TEST_P(GPURendererPixelTest, BlendingWithoutAntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);
  pass->has_transparent_background = false;

  CreateTestAxisAlignedQuads(rect, SkColor4f{0.0f, 0.0f, 1.0f, 0.5},
                             SkColor4f{0.0f, 1.0f, 0.0f, 0.5f}, true, true,
                             pass.get());

  SharedQuadState* background_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());
  auto* background_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  background_quad->SetNew(background_quad_state, rect, rect, SkColors::kBlack,
                          false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("translucent_quads_no_aa.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(GPURendererPixelTest, TrilinearFiltering) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);
  root_pass->has_transparent_background = false;

  AggregatedRenderPassId child_pass_id{2};
  gfx::Transform transform_to_root;
  gfx::Rect child_pass_rect(
      ScaleToCeiledSize(this->device_viewport_size_, 4.0f));
  bool generate_mipmap = true;
  auto child_pass = std::make_unique<AggregatedRenderPass>();
  child_pass->SetAll(
      child_pass_id, child_pass_rect, child_pass_rect, transform_to_root,
      cc::FilterOperations(), cc::FilterOperations(), gfx::RRectF(),
      gfx::ContentColorUsage::kSRGB, false, false, false, generate_mipmap);

  gfx::Rect red_rect(child_pass_rect);
  // Small enough red rect that linear filtering will miss it but large enough
  // that it makes a meaningful contribution when using trilinear filtering.
  red_rect.ClampToCenteredSize(gfx::Size(2, child_pass_rect.height()));
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), red_rect, child_pass.get(),
                                gfx::MaskFilterInfo());
  auto* red = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SkColors::kRed, false);

  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), child_pass_rect, child_pass.get(), gfx::MaskFilterInfo());
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, child_pass_rect, child_pass_rect,
               SkColors::kBlue, false);

  auto child_to_root_transform = gfx::TransformBetweenRects(
      gfx::RectF(child_pass_rect), gfx::RectF(viewport_rect));
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      child_to_root_transform, child_pass_rect, root_pass.get(), gfx::MaskFilterInfo());
  auto* child_pass_quad =
      root_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  child_pass_quad->SetNew(
      child_pass_shared_state, child_pass_rect, child_pass_rect, child_pass_id,
      kInvalidResourceId, gfx::RectF(), gfx::Size(), gfx::Vector2dF(),
      gfx::PointF(), gfx::RectF(child_pass_rect), false, 1.0f);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Skia is configured to bias the mipmap LOD by -0.5. However, the GLES 2
  // implementation that this test runs against doesn't support the bias. So GL
  // renderer and SkiaGL differ from SkiaVk.
  if (renderer_type() == RendererType::kSkiaVk) {
    EXPECT_TRUE(this->RunPixelTest(
        &pass_list,
        base::FilePath(FILE_PATH_LITERAL("trilinear_filtering_skia_vk.png")),
        cc::AlphaDiscardingExactPixelComparator()));
  } else {
    base::FilePath baseline =
        base::FilePath(FILE_PATH_LITERAL("trilinear_filtering.png"));

    if (renderer_type() == RendererType::kSkiaGL && IsANGLEMetal()) {
      baseline = baseline.InsertBeforeExtensionASCII(kANGLEMetalStr);
    }

    EXPECT_TRUE(this->RunPixelTest(&pass_list, baseline,
                                   cc::AlphaDiscardingExactPixelComparator()));
  }
}

class SoftwareRendererPixelTest : public VizPixelTest {
 public:
  SoftwareRendererPixelTest() : VizPixelTest(RendererType::kSoftware) {}
};

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One clipped blue quad in the lower right corner.  Outside the clip
  // is red, which should not appear.
  gfx::Rect blue_rect(gfx::Size(100, 100));
  gfx::Rect blue_clip_rect(gfx::Point(50, 50), gfx::Size(50, 50));

  std::unique_ptr<cc::FakeRecordingSource> blue_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(blue_rect.size());
  cc::PaintFlags red_flags;
  red_flags.setColor(SkColors::kRed);
  blue_recording->add_draw_rect_with_flags(blue_rect, red_flags);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);
  blue_recording->add_draw_rect_with_flags(blue_clip_rect, blue_flags);
  blue_recording->Rerecord();

  scoped_refptr<cc::RasterSource> blue_raster_source =
      blue_recording->CreateRasterSource();

  gfx::Vector2d offset(viewport.bottom_right() - blue_rect.bottom_right());
  bool needs_blending = true;
  gfx::Transform blue_quad_to_target_transform;
  blue_quad_to_target_transform.Translate(offset.x(), offset.y());
  gfx::Rect blue_target_clip_rect = cc::MathUtil::MapEnclosingClippedRect(
      blue_quad_to_target_transform, blue_clip_rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadStateClipped(blue_quad_to_target_transform, blue_rect,
                                       blue_target_clip_rect, pass.get());

  auto* blue_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();

  blue_quad->SetNew(blue_shared_state,
                    viewport,  // Intentionally bigger than clip.
                    viewport, needs_blending, gfx::RectF(viewport),
                    viewport.size(), nearest_neighbor, texture_format, viewport,
                    1.f, {}, blue_raster_source->GetDisplayItemList());

  // One viewport-filling green quad.
  std::unique_ptr<cc::FakeRecordingSource> green_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording->add_draw_rect_with_flags(viewport, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  auto* green_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(green_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0.f, 0.f, 1.f, 1.f), viewport.size(),
                     nearest_neighbor, texture_format, viewport, 1.f, {},
                     green_raster_source->GetDisplayItemList());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// Not WithSkiaGPUBackend since that path currently requires tiles for opacity.
TEST_F(SoftwareRendererPixelTest, PictureDrawQuadOpacity) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  ResourceFormat texture_format = RGBA_8888;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity green quad.
  std::unique_ptr<cc::FakeRecordingSource> green_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording->add_draw_rect_with_flags(viewport, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());
  green_shared_state->opacity = 0.5f;

  auto* green_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(green_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), viewport.size(), nearest_neighbor,
                     texture_format, viewport, 1.f, {},
                     green_raster_source->GetDisplayItemList());

  // One viewport-filling white quad.
  std::unique_ptr<cc::FakeRecordingSource> white_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags white_flags;
  white_flags.setColor(SkColors::kWhite);
  white_recording->add_draw_rect_with_flags(viewport, white_flags);
  white_recording->Rerecord();
  scoped_refptr<cc::RasterSource> white_raster_source =
      white_recording->CreateRasterSource();

  gfx::Transform white_quad_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      white_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  auto* white_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  white_quad->SetNew(white_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), viewport.size(), nearest_neighbor,
                     texture_format, viewport, 1.f, {},
                     white_raster_source->GetDisplayItemList());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadOpacityWithAlpha) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  ResourceFormat texture_format = RGBA_8888;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity transparent quad.
  std::unique_ptr<cc::FakeRecordingSource> transparent_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags transparent_flags;
  transparent_flags.setColor(SkColors::kTransparent);
  transparent_recording->add_draw_rect_with_flags(viewport, transparent_flags);
  transparent_recording->Rerecord();
  scoped_refptr<cc::RasterSource> transparent_raster_source =
      transparent_recording->CreateRasterSource();

  gfx::Transform transparent_quad_to_target_transform;
  SharedQuadState* transparent_shared_state = CreateTestSharedQuadState(
      transparent_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());
  transparent_shared_state->opacity = 0.5f;

  auto* transparent_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  transparent_quad->SetNew(
      transparent_shared_state, viewport, viewport, needs_blending,
      gfx::RectF(0, 0, 1, 1), viewport.size(), nearest_neighbor, texture_format,
      viewport, 1.f, {}, transparent_raster_source->GetDisplayItemList());

  // One viewport-filling white quad.
  std::unique_ptr<cc::FakeRecordingSource> white_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags white_flags;
  white_flags.setColor(SkColors::kWhite);
  white_recording->add_draw_rect_with_flags(viewport, white_flags);
  white_recording->Rerecord();
  scoped_refptr<cc::RasterSource> white_raster_source =
      white_recording->CreateRasterSource();

  gfx::Transform white_quad_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      white_quad_to_target_transform, viewport, pass.get(), gfx::MaskFilterInfo());

  auto* white_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  white_quad->SetNew(white_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), viewport.size(), nearest_neighbor,
                     texture_format, viewport, 1.f, {},
                     white_raster_source->GetDisplayItemList());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("white.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

void draw_point_color(SkCanvas* canvas,
                      SkScalar x,
                      SkScalar y,
                      SkColor4f color) {
  SkPaint paint;
  paint.setColor(color, nullptr /* SkColorSpace* colorSpace */);
  canvas->drawPoint(x, y, paint);
}

// If we disable image filtering, then a 2x2 bitmap should appear as four
// huge sharp squares.
TEST_F(SoftwareRendererPixelTest, PictureDrawQuadDisableImageFiltering) {
  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(2, 2);
  ASSERT_NE(surface, nullptr);
  SkCanvas* canvas = surface->getCanvas();
  draw_point_color(canvas, 0, 0, SkColors::kGreen);
  draw_point_color(canvas, 0, 1, SkColors::kBlue);
  draw_point_color(canvas, 1, 0, SkColors::kBlue);
  draw_point_color(canvas, 1, 1, SkColors::kGreen);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  recording->add_draw_image_with_flags(
      surface->makeImageSnapshot(), gfx::Point(),
      SkSamplingOptions(SkFilterMode::kLinear), cc::PaintFlags());
  recording->Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording->CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(0, 0, 2, 2), viewport.size(), nearest_neighbor,
               texture_format, viewport, 1.f, {},
               raster_source->GetDisplayItemList());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  this->disable_picture_quad_image_filtering_ = true;

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This disables filtering by setting |nearest_neighbor| on the
// PictureDrawQuad.
TEST_F(SoftwareRendererPixelTest, PictureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = true;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(2, 2);
  ASSERT_NE(surface, nullptr);
  SkCanvas* canvas = surface->getCanvas();
  draw_point_color(canvas, 0, 0, SkColors::kGreen);
  draw_point_color(canvas, 0, 1, SkColors::kBlue);
  draw_point_color(canvas, 1, 0, SkColors::kBlue);
  draw_point_color(canvas, 1, 1, SkColors::kGreen);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  recording->add_draw_image_with_flags(
      surface->makeImageSnapshot(), gfx::Point(),
      SkSamplingOptions(SkFilterMode::kLinear), cc::PaintFlags());
  recording->Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording->CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(0, 0, 2, 2), viewport.size(), nearest_neighbor,
               texture_format, viewport, 1.f, {},
               raster_source->GetDisplayItemList());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This disables filtering by setting |nearest_neighbor| on the
// TileDrawQuad.
TEST_P(RendererPixelTest, TileDrawQuadNearestNeighbor) {
  constexpr bool contents_premultiplied = true;
  constexpr bool needs_blending = true;
  constexpr bool nearest_neighbor = true;
  constexpr bool force_anti_aliasing_off = false;
  constexpr ResourceFormat resource_format = RGBA_8888;
  gfx::Rect viewport(this->device_viewport_size_);

  SkColorType ct = ResourceFormatToClosestSkColorType(!is_software_renderer(),
                                                      resource_format);
  SkImageInfo info = SkImageInfo::Make(2, 2, ct, kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(info);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  draw_point_color(&canvas, 0, 0, SkColors::kGreen);
  draw_point_color(&canvas, 0, 1, SkColors::kBlue);
  draw_point_color(&canvas, 1, 0, SkColors::kBlue);
  draw_point_color(&canvas, 1, 1, SkColors::kGreen);

  gfx::Size tile_size(2, 2);
  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(this->child_context_provider_,
                                 this->child_resource_provider_.get(),
                                 tile_size, SinglePlaneFormat::kRGBA_8888,
                                 gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, gfx::RectF(gfx::Rect(tile_size)), tile_size,
               contents_premultiplied, nearest_neighbor,
               force_anti_aliasing_off);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// This disables filtering by setting |nearest_neighbor| to true on the
// TextureDrawQuad.
TEST_F(SoftwareRendererPixelTest, TextureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = true;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  draw_point_color(&canvas, 0, 0, SkColors::kGreen);
  draw_point_color(&canvas, 0, 1, SkColors::kBlue);
  draw_point_color(&canvas, 1, 0, SkColors::kBlue);
  draw_point_color(&canvas, 1, 1, SkColors::kGreen);

  gfx::Size tile_size(2, 2);
  ResourceId resource =
      this->AllocateAndFillSoftwareResource(tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, false, gfx::PointF(0, 0), gfx::PointF(1, 1),
               SkColors::kBlack, vertex_opacity, false, nearest_neighbor,
               /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f)));
}

// This ensures filtering is enabled by setting |nearest_neighbor| to false on
// the TextureDrawQuad.
TEST_F(SoftwareRendererPixelTest, TextureDrawQuadLinear) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  {
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    draw_point_color(&canvas, 0, 0, SkColors::kGreen);
    draw_point_color(&canvas, 0, 1, SkColors::kBlue);
    draw_point_color(&canvas, 1, 0, SkColors::kBlue);
    draw_point_color(&canvas, 1, 1, SkColors::kGreen);
  }

  gfx::Size tile_size(2, 2);
  ResourceId resource =
      this->AllocateAndFillSoftwareResource(tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get(),
                                gfx::MaskFilterInfo());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, /*premultiplied=*/true, gfx::PointF(0, 0),
               gfx::PointF(1, 1), SkColors::kBlack, vertex_opacity, false,
               nearest_neighbor, /*secure_output=*/false,
               gfx::ProtectedVideoType::kClear);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Allow for a small amount of error as the blending alogrithm used by Skia is
  // affected by the offset in the expanded rect.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers_linear.png")),
      cc::FuzzyPixelComparator()
          .SetErrorPixelsPercentageLimit(100.f)
          .SetAbsErrorLimit(16)));
}

TEST_F(SoftwareRendererPixelTest, PictureDrawQuadNonIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = false;

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // As scaling up the blue checkerboards will cause sampling on the GPU,
  // a few extra "cleanup rects" need to be added to clobber the blending
  // to make the output image more clean.  This will also test subrects
  // of the layer.
  gfx::Transform green_quad_to_target_transform;
  gfx::Rect green_rect1(gfx::Point(80, 0), gfx::Size(20, 100));
  gfx::Rect green_rect2(gfx::Point(0, 80), gfx::Size(100, 20));

  std::unique_ptr<cc::FakeRecordingSource> green_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());

  cc::PaintFlags red_flags;
  red_flags.setColor(SkColors::kRed);
  green_recording->add_draw_rect_with_flags(viewport, red_flags);
  cc::PaintFlags green_flags;
  green_flags.setColor(SkColors::kGreen);
  green_recording->add_draw_rect_with_flags(green_rect1, green_flags);
  green_recording->add_draw_rect_with_flags(green_rect2, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  SharedQuadState* top_right_green_shared_quad_state =
      CreateTestSharedQuadState(green_quad_to_target_transform, viewport,
                                pass.get(), gfx::MaskFilterInfo());

  auto* green_quad1 = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad1->SetNew(
      top_right_green_shared_quad_state, green_rect1, green_rect1,
      needs_blending, gfx::RectF(gfx::SizeF(green_rect1.size())),
      green_rect1.size(), nearest_neighbor, texture_format, green_rect1, 1.f,
      {}, green_raster_source->GetDisplayItemList());

  auto* green_quad2 = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad2->SetNew(
      top_right_green_shared_quad_state, green_rect2, green_rect2,
      needs_blending, gfx::RectF(gfx::SizeF(green_rect2.size())),
      green_rect2.size(), nearest_neighbor, texture_format, green_rect2, 1.f,
      {}, green_raster_source->GetDisplayItemList());

  // Add a green clipped checkerboard in the bottom right to help test
  // interleaving picture quad content and solid color content.
  gfx::Rect bottom_right_rect(
      gfx::Point(viewport.width() / 2, viewport.height() / 2),
      gfx::Size(viewport.width() / 2, viewport.height() / 2));
  SharedQuadState* bottom_right_green_shared_state =
      CreateTestSharedQuadStateClipped(green_quad_to_target_transform, viewport,
                                       bottom_right_rect, pass.get());
  auto* bottom_right_color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bottom_right_color_quad->SetNew(bottom_right_green_shared_state, viewport,
                                  viewport, SkColors::kGreen, false);

  // Add two blue checkerboards taking up the bottom left and top right,
  // but use content scales as content rects to make this happen.
  // The content is at a 4x content scale.
  gfx::Rect layer_rect(gfx::Size(20, 30));
  float contents_scale = 4.f;
  // Two rects that touch at their corners, arbitrarily placed in the layer.
  gfx::RectF blue_layer_rect1(gfx::PointF(5.5f, 9.0f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF blue_layer_rect2(gfx::PointF(8.0f, 6.5f), gfx::SizeF(2.5f, 2.5f));
  gfx::RectF union_layer_rect = blue_layer_rect1;
  union_layer_rect.Union(blue_layer_rect2);

  // Because scaling up will cause sampling outside the rects, add one extra
  // pixel of buffer at the final content scale.
  float inset = -1.f / contents_scale;
  blue_layer_rect1.Inset(inset);
  blue_layer_rect2.Inset(inset);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(layer_rect.size());

  cc::Region outside(layer_rect);
  outside.Subtract(gfx::ToEnclosingRect(union_layer_rect));
  for (gfx::Rect rect : outside) {
    recording->add_draw_rect_with_flags(rect, red_flags);
  }

  cc::PaintFlags blue_flags;
  blue_flags.setColor(SkColors::kBlue);
  recording->add_draw_rectf_with_flags(blue_layer_rect1, blue_flags);
  recording->add_draw_rectf_with_flags(blue_layer_rect2, blue_flags);
  recording->Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording->CreateRasterSource();

  gfx::Rect content_union_rect(
      gfx::ToEnclosingRect(gfx::ScaleRect(union_layer_rect, contents_scale)));

  // At a scale of 4x the rectangles with a width of 2.5 will take up 10 pixels,
  // so scale an additional 10x to make them 100x100.
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Scale(10.0, 10.0);
  gfx::Rect quad_content_rect(gfx::Size(20, 20));
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, quad_content_rect, pass.get(), gfx::MaskFilterInfo());

  auto* blue_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  blue_quad->SetNew(blue_shared_state, quad_content_rect, quad_content_rect,
                    needs_blending, gfx::RectF(quad_content_rect),
                    content_union_rect.size(), nearest_neighbor, texture_format,
                    content_union_rect, contents_scale, {},
                    raster_source->GetDisplayItemList());

  // Fill left half of viewport with green.
  gfx::Transform half_green_quad_to_target_transform;
  gfx::Rect half_green_rect(gfx::Size(viewport.width() / 2, viewport.height()));
  SharedQuadState* half_green_shared_state = CreateTestSharedQuadState(
      half_green_quad_to_target_transform, half_green_rect, pass.get(),
      gfx::MaskFilterInfo());
  auto* half_color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  half_color_quad->SetNew(half_green_shared_state, half_green_rect,
                          half_green_rect, SkColors::kGreen, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

class RendererPixelTestWithFlippedOutputSurface : public VizPixelTestWithParam {
 protected:
  gfx::SurfaceOrigin GetSurfaceOrigin() const override {
    return gfx::SurfaceOrigin::kTopLeft;
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithFlippedOutputSurface,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    RendererPixelTestWithFlippedOutputSurface);

TEST_P(RendererPixelTestWithFlippedOutputSurface, ExplicitFlipTest) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Note: RunPixelTest() will issue a CopyOutputRequest on the root pass. The
  // implementation should realize the output surface is flipped, and return a
  // right-side up result regardless (i.e., NOT blue_yellow_flipped.png).
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(RendererPixelTestWithFlippedOutputSurface, CheckChildPassUnflipped) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SkColors::kYellow,
                 false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Check that the child pass remains unflipped.
  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(GPURendererPixelTest, CheckReadbackSubset) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  auto child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get(), gfx::MaskFilterInfo());

  // Draw a green quad full-size with a blue quad in the lower-right corner.
  gfx::Rect blue_rect(this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4,
                      this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SkColors::kBlue, false);
  gfx::Rect green_rect(0, 0, this->device_viewport_size_.width(),
                       this->device_viewport_size_.height());
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(shared_state, green_rect, green_rect, SkColors::kGreen, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Check that the child pass remains unflipped.
  gfx::Rect capture_rect(this->device_viewport_size_.width() / 2,
                         this->device_viewport_size_.height() / 2,
                         this->device_viewport_size_.width() / 2,
                         this->device_viewport_size_.height() / 2);
  EXPECT_TRUE(this->RunPixelTestWithReadbackTargetAndArea(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("green_small_with_blue_corner.png")),
      cc::AlphaDiscardingExactPixelComparator(), &capture_rect));
}

TEST_P(GPURendererPixelTest, TextureQuadBatching) {
  // This test verifies that multiple texture quads using the same resource
  // get drawn correctly.  It implicitly is trying to test that the
  // renderer does the right thing with its draw quad cache.

  gfx::Rect rect(this->device_viewport_size_);
  bool needs_blending = false;

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  // Make a mask.
  gfx::Rect mask_rect = rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(SkIntToScalar(4));
  paint.setColor(SkColors::kGreen);
  canvas.clear(SkColors::kWhite);
  gfx::Rect inset_rect = rect;
  while (!inset_rect.IsEmpty()) {
    inset_rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
    canvas.drawRect(SkRect::MakeXYWH(inset_rect.x(), inset_rect.y(),
                                     inset_rect.width(), inset_rect.height()),
                    paint);
    inset_rect.Inset(gfx::Insets::TLBR(6, 6, 4, 4));
  }

  ResourceId resource = CreateGpuResource(
      this->child_context_provider_, this->child_resource_provider_.get(),
      mask_rect.size(), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
      MakePixelSpan(bitmap));

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  // Arbitrary dividing lengths to divide up the resource into 16 quads.
  int widths[] = {
      0, 60, 50, 40,
  };
  int heights[] = {
      0, 10, 80, 50,
  };
  size_t num_quads = 4;
  for (size_t i = 0; i < num_quads; ++i) {
    int x_start = widths[i];
    int x_end = i == num_quads - 1 ? rect.width() : widths[i + 1];
    DCHECK_LE(x_end, rect.width());
    for (size_t j = 0; j < num_quads; ++j) {
      int y_start = heights[j];
      int y_end = j == num_quads - 1 ? rect.height() : heights[j + 1];
      DCHECK_LE(y_end, rect.height());

      float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      gfx::Rect layer_rect(x_start, y_start, x_end - x_start, y_end - y_start);
      gfx::RectF uv_rect = gfx::ScaleRect(
          gfx::RectF(layer_rect), 1.f / rect.width(), 1.f / rect.height());

      auto* texture_quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
      texture_quad->SetNew(
          shared_state, layer_rect, layer_rect, needs_blending, mapped_resource,
          true, uv_rect.origin(), uv_rect.bottom_right(), SkColors::kWhite,
          vertex_opacity, false, false, /*secure_output_only=*/false,
          gfx::ProtectedVideoType::kClear);
    }
  }

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("spiral.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

TEST_P(GPURendererPixelTest, TileQuadClamping) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool contents_premultiplied = true;
  bool needs_blending = true;
  bool nearest_neighbor = false;
  bool use_aa = false;

  gfx::Size layer_size(4, 4);
  gfx::Size tile_size(20, 20);
  gfx::Rect quad_rect(layer_size);
  gfx::RectF tex_coord_rect(quad_rect);

  // tile sized bitmap, with valid contents green and contents outside the
  // layer rect red.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(tile_size.width(), tile_size.height());
  SkCanvas canvas(bitmap, SkSurfaceProps{});
  SkPaint red;
  red.setColor(SkColors::kRed);
  canvas.drawRect(SkRect::MakeWH(tile_size.width(), tile_size.height()), red);
  SkPaint green;
  green.setColor(SkColors::kGreen);
  canvas.drawRect(SkRect::MakeWH(layer_size.width(), layer_size.height()),
                  green);

  ResourceId resource;
  if (!is_software_renderer()) {
    resource = CreateGpuResource(this->child_context_provider_,
                                 this->child_resource_provider_.get(),
                                 tile_size, SinglePlaneFormat::kRGBA_8888,
                                 gfx::ColorSpace(), MakePixelSpan(bitmap));
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, viewport, transform_to_root);

  // Green quad that should not show any red pixels from outside the
  // tex coord rect.
  gfx::Transform transform;
  transform.Scale(40, 40);
  SharedQuadState* quad_shared =
      CreateTestSharedQuadState(transform, gfx::Rect(layer_size), pass.get(),
                                gfx::MaskFilterInfo());
  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(quad_shared, gfx::Rect(layer_size), gfx::Rect(layer_size),
               needs_blending, mapped_resource, tex_coord_rect, tile_size,
               contents_premultiplied, nearest_neighbor, use_aa);

  // Green background.
  SharedQuadState* background_shared =
      CreateTestSharedQuadState(gfx::Transform(), viewport, pass.get(),
                                gfx::MaskFilterInfo());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(background_shared, viewport, viewport, SkColors::kGreen,
                     false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::AlphaDiscardingExactPixelComparator()));
}

TEST_P(RendererPixelTest, RoundedCornerSimpleSolidDrawQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Transform quad_to_target_transform;
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  gfx::Rect red_rect = blue_rect;
  blue_rect.Inset(kInset);

  gfx::RRectF rounded_corner_rrect(gfx::RectF(blue_rect), kCornerRadius);
  SharedQuadState* shared_state_rounded = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_rrect));

  auto* blue = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_rounded, blue_rect, blue_rect, SkColors::kBlue,
               false);

  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_normal, red_rect, red_rect, SkColors::kWhite,
                false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f)));
}

TEST_P(GPURendererPixelTest, RoundedCornerSimpleTextureDrawQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Transform quad_to_target_transform;
  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  gfx::Rect red_rect = blue_rect;
  blue_rect.Inset(kInset);

  gfx::RRectF rounded_corner_rrect(gfx::RectF(blue_rect), kCornerRadius);
  SharedQuadState* shared_state_rounded = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_rrect));

  const uint8_t colors[] = {0, 0, 255, 255, 0, 0, 255, 255,
                            0, 0, 255, 255, 0, 0, 255, 255};
  ResourceId resource = CreateGpuResource(
      this->child_context_provider_, this->child_resource_provider_.get(),
      gfx::Size(2, 2), SinglePlaneFormat::kRGBA_8888, gfx::ColorSpace(),
      colors);

  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];
  bool needs_blending = true;
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool flipped = false;
  const bool nearest_neighbor = false;
  auto* blue = root_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  blue->SetNew(shared_state_rounded, blue_rect, blue_rect, needs_blending,
               mapped_resource, true, uv_top_left, uv_bottom_right,
               SkColors::kBlack, vertex_opacity, flipped, nearest_neighbor,
               /*secure_output_only=*/false, gfx::ProtectedVideoType::kClear);

  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());

  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_normal, red_rect, red_rect, SkColors::kWhite,
                false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_simple.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RoundedCornerOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kCornerRadius = 20;
  constexpr int kBlueCornerRadius = 10;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect blue_rect = child_pass_local_rect;
  gfx::Vector2dF blue_offset_from_target(-30, 40);
  gfx::RRectF blue_rrect(gfx::RectF(blue_rect), kBlueCornerRadius);
  blue_rrect.Offset(blue_offset_from_target);
  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(blue_offset_from_target);
  SharedQuadState* shared_state_with_rrect = CreateTestSharedQuadState(
      quad_to_target_transform, child_pass_local_rect, child_pass.get(),
      gfx::MaskFilterInfo(blue_rrect));
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_with_rrect, blue_rect, blue_rect, SkColors::kBlue,
               false);

  SharedQuadState* shared_state_without_rrect = CreateTestSharedQuadState(
      gfx::Transform(), child_pass_local_rect, child_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect yellow_rect = child_pass_local_rect;
  yellow_rect.Offset(30, -60);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state_without_rrect, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  gfx::Rect white_rect = child_pass_local_rect;
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  base::FilePath path(FILE_PATH_LITERAL("rounded_corner_render_pass_.png"));
  path = path.InsertBeforeExtensionASCII(this->renderer_str());
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, path, cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1421171): currently failing on iOS.
#define MAYBE_LinearGradientOnRenderPass DISABLED_LinearGradientOnRenderPass
#else
#define MAYBE_LinearGradientOnRenderPass LinearGradientOnRenderPass
#endif  // BUILDFLAG(IS_IOS)
TEST_P(GPURendererPixelTest, MAYBE_LinearGradientOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kCornerRadius = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect white_rect = child_pass_local_rect;
  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  gfx::LinearGradient gradient_mask(330);
  gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
  gradient_mask.AddStep(.5, 255);
  gradient_mask.AddStep(1, 255);
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds, gradient_mask));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("linear_gradient_render_pass.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

#if BUILDFLAG(IS_IOS)
// TODO(crbug.com/1421171): currently failing on iOS.
#define MAYBE_MultiLinearGradientOnRenderPass \
  DISABLED_MultiLinearGradientOnRenderPass
#else
#define MAYBE_MultiLinearGradientOnRenderPass MultiLinearGradientOnRenderPass
#endif  // BUILDFLAG(IS_IOS)
TEST_P(GPURendererPixelTest, MAYBE_MultiLinearGradientOnRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kCornerRadius = 20;
  constexpr int kInset = 20;
  constexpr int kBlueCornerRadius = 10;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  gfx::Rect blue_rect = child_pass_local_rect;
  gfx::Vector2dF blue_offset_from_target(-30, 40);
  gfx::RRectF blue_rrect(gfx::RectF(blue_rect), kBlueCornerRadius);
  blue_rrect.Offset(blue_offset_from_target);
  gfx::LinearGradient blue_gradient(0);
  blue_gradient.AddStep(/*fraction=*/0, /*alpha=*/255);
  blue_gradient.AddStep(1, 0);

  gfx::Transform quad_to_target_transform;
  quad_to_target_transform.Translate(blue_offset_from_target);
  SharedQuadState* shared_state_with_rrect = CreateTestSharedQuadState(
      quad_to_target_transform, child_pass_local_rect, child_pass.get(),
      gfx::MaskFilterInfo(blue_rrect, blue_gradient));
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_with_rrect, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect white_rect = child_pass_local_rect;
  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadius);
  gfx::LinearGradient gradient_mask(-30);
  gradient_mask.AddStep(/*fraction=*/0, /*alpha=*/0);
  gradient_mask.AddStep(.5, 255);
  gradient_mask.AddStep(1, 255);
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds, gradient_mask));
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(
          FILE_PATH_LITERAL("multi_linear_gradient_render_pass.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.6f)));
}

TEST_P(RendererPixelTest, RoundedCornerMultiRadii) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr gfx::RoundedCornersF kCornerRadii(5, 15, 25, 35);
  constexpr int kInset = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::RRectF rounded_corner_bounds(gfx::RectF(pass_rect), kCornerRadii);
  gfx::Rect blue_rect = pass_rect;
  blue_rect.set_height(blue_rect.height() / 2);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state_normal = CreateTestSharedQuadState(
      quad_to_target_transform, pass_rect, root_pass.get(),
      gfx::MaskFilterInfo(rounded_corner_bounds));
  auto* blue = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state_normal, blue_rect, blue_rect, SkColors::kBlue,
               false);

  gfx::Rect yellow_rect = blue_rect;
  yellow_rect.Offset(0, blue_rect.height());

  auto* yellow = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state_normal, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  SharedQuadState* sqs_white = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect white_rect = gfx::Rect(this->device_viewport_size_);
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(sqs_white, white_rect, white_rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  // Software/skia renderer uses skia rrect to create rounded corner clip.
  // This results in a different corner path due to a different anti aliasing
  // approach than the fragment shader in gl renderer.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_radii.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f)));
}

TEST_P(RendererPixelTest, RoundedCornerMultipleQads) {
  const gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr gfx::RoundedCornersF kCornerRadiiUL(5, 0, 0, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiUR(0, 15, 0, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiLR(0, 0, 25, 0);
  constexpr gfx::RoundedCornersF kCornerRadiiLL(0, 0, 0, 35);
  constexpr int kInset = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass = CreateTestRootRenderPass(root_pass_id, viewport_rect);

  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::RRectF rounded_corner_bounds_ul(gfx::RectF(pass_rect), kCornerRadiiUL);
  gfx::RRectF rounded_corner_bounds_ur(gfx::RectF(pass_rect), kCornerRadiiUR);
  gfx::RRectF rounded_corner_bounds_lr(gfx::RectF(pass_rect), kCornerRadiiLR);
  gfx::RRectF rounded_corner_bounds_ll(gfx::RectF(pass_rect), kCornerRadiiLL);

  gfx::Rect ul_rect = pass_rect;
  ul_rect.set_height(ul_rect.height() / 2);
  ul_rect.set_width(ul_rect.width() / 2);

  gfx::Rect ur_rect = pass_rect;
  ur_rect.set_x(ul_rect.right());
  ur_rect.set_width(pass_rect.right() - ur_rect.x());
  ur_rect.set_height(ul_rect.height());

  gfx::Rect lr_rect = pass_rect;
  lr_rect.set_y(ur_rect.bottom());
  lr_rect.set_x(ur_rect.x());
  lr_rect.set_width(ur_rect.width());
  lr_rect.set_height(pass_rect.bottom() - lr_rect.y());

  gfx::Rect ll_rect = pass_rect;
  ll_rect.set_y(lr_rect.y());
  ll_rect.set_width(ul_rect.width());
  ll_rect.set_height(lr_rect.height());

  SharedQuadState* shared_state_normal_ul =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ul));

  SharedQuadState* shared_state_normal_ur =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ur));

  SharedQuadState* shared_state_normal_lr =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_lr));

  SharedQuadState* shared_state_normal_ll =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get(),
                                gfx::MaskFilterInfo(rounded_corner_bounds_ll));

  auto* ul = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* ur = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* lr = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* ll = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();

  ul->SetNew(shared_state_normal_ul, ul_rect, ul_rect, SkColors::kRed, false);
  ur->SetNew(shared_state_normal_ur, ur_rect, ur_rect, SkColors::kGreen, false);
  lr->SetNew(shared_state_normal_lr, lr_rect, lr_rect, SkColors::kBlue, false);
  ll->SetNew(shared_state_normal_ll, ll_rect, ll_rect, SkColors::kYellow,
             false);

  SharedQuadState* sqs_white = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect white_rect = gfx::Rect(this->device_viewport_size_);
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(sqs_white, white_rect, white_rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(root_pass));

  auto comparator =
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          0.55f);
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("rounded_corner_multi_quad.png")),
      comparator));
}

class RendererPixelTestWithOverdrawFeedback : public VizPixelTestWithParam {
 protected:
  void SetUp() override {
    this->debug_settings_.show_overdraw_feedback = true;
    VizPixelTestWithParam::SetUp();
  }
};

TEST_P(RendererPixelTestWithOverdrawFeedback, TranslucentRectangles) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  gfx::Transform transform_to_root;
  auto pass = CreateTestRenderPass(id, rect, transform_to_root);

  CreateTestAxisAlignedQuads(rect, SkColor4f{0.267f, 0.267f, 0.267f, 0.063f},
                             SkColor4f{0.8f, 0.8f, 0.8f, 0.063f}, true, false,
                             pass.get());

  gfx::Transform bg_quad_to_target_transform;
  SharedQuadState* bg_shared_state =
      CreateTestSharedQuadState(bg_quad_to_target_transform, rect, pass.get(),
                                gfx::MaskFilterInfo());

  auto* bg = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bg->SetNew(bg_shared_state, rect, rect, SkColors::kBlack, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // TODO(xing.xu): investigate why overdraw feedback has small difference
  // (http://crbug.com/909971)
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("translucent_rectangles.png")),
      cc::FuzzyPixelComparator().SetErrorPixelsPercentageLimit(2.f)));
}

INSTANTIATE_TEST_SUITE_P(,
                         RendererPixelTestWithOverdrawFeedback,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    RendererPixelTestWithOverdrawFeedback);

using PrimaryID = gfx::ColorSpace::PrimaryID;
using TransferID = gfx::ColorSpace::TransferID;

class ColorTransformPixelTest
    : public VizPixelTest,
      public testing::WithParamInterface<
          std::tuple<RendererType, gfx::ColorSpace, gfx::ColorSpace, bool>> {
 public:
  ColorTransformPixelTest() : VizPixelTest(std::get<0>(GetParam())) {
    // Note that this size of 17 is not random -- it is chosen to match the
    // size of LUTs that are created. If we did not match the LUT size exactly,
    // then the error for LUT based transforms is much larger.
    this->device_viewport_size_ = gfx::Size(17, 5);
    this->src_color_space_ = std::get<1>(GetParam());
    this->dst_color_space_ = std::get<2>(GetParam());
    if (!this->src_color_space_.IsValid()) {
      this->src_color_space_ =
          gfx::ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
    }
    if (!this->dst_color_space_.IsValid()) {
      this->dst_color_space_ =
          gfx::ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
    }
    this->display_color_spaces_ =
        gfx::DisplayColorSpaces(this->dst_color_space_);
    this->premultiplied_alpha_ = std::get<3>(GetParam());
  }

  void Basic() {
    if (src_color_space_.GetTransferID() == TransferID::PQ &&
        !dst_color_space_.IsHDR()) {
      GTEST_SKIP() << "Skipping tonemapped output";
    }

    gfx::Rect rect(this->device_viewport_size_);
    std::vector<uint8_t> input_colors(4 * rect.width() * rect.height(), 0);
    std::vector<SkColor> expected_output_colors(rect.width() * rect.height());

    // Set the input data to be:
    //   Row 0: Gradient of red from 0 to 255
    //   Row 1: Gradient of green from 0 to 255
    //   Row 2: Gradient of blue from 0 to 255
    //   Row 3: Gradient of grey from 0 to 255
    //   Row 4: Gradient of alpha from 0 to 255 with mixed colors.
    for (int x = 0; x < rect.width(); ++x) {
      int gradient_value = (x * 255) / (rect.width() - 1);
      for (int y = 0; y < rect.height(); ++y) {
        uint8_t* pixel = &input_colors[4 * (x + rect.width() * y)];
        pixel[3] = 255;
        if (y < 3) {
          pixel[y] = gradient_value;
        } else if (y == 3) {
          pixel[0] = pixel[1] = pixel[2] = gradient_value;
        } else {
          if (this->premultiplied_alpha_) {
            pixel[x % 3] = gradient_value;
            pixel[3] = gradient_value;
          } else {
            pixel[x % 3] = 0xFF;
            pixel[3] = gradient_value;
          }
        }
      }
    }

    gfx::ColorTransform::Options options;
    options.tone_map_pq_and_hlg_to_dst = true;
    options.sdr_max_luminance_nits = gfx::ColorSpace::kDefaultSDRWhiteLevel;
    std::unique_ptr<gfx::ColorTransform> transform =
        gfx::ColorTransform::NewColorTransform(this->src_color_space_,
                                               this->dst_color_space_, options);

    for (size_t i = 0; i < expected_output_colors.size(); ++i) {
      gfx::ColorTransform::TriStim color;
      color.set_x(input_colors[4 * i + 0] / 255.f);
      color.set_y(input_colors[4 * i + 1] / 255.f);
      color.set_z(input_colors[4 * i + 2] / 255.f);
      float alpha = input_colors[4 * i + 3] / 255.f;
      if (this->premultiplied_alpha_ && alpha > 0.0) {
        color.Scale(1.0f / alpha);
      }
      transform->Transform(&color, 1);
      color.Scale(alpha);
      color.set_x(base::clamp(color.x(), 0.0f, 1.0f));
      color.set_y(base::clamp(color.y(), 0.0f, 1.0f));
      color.set_z(base::clamp(color.z(), 0.0f, 1.0f));
      expected_output_colors[i] =
          SkColorSetARGB(255, static_cast<size_t>(255.f * color.x() + 0.5f),
                         static_cast<size_t>(255.f * color.y() + 0.5f),
                         static_cast<size_t>(255.f * color.z() + 0.5f));
    }

    AggregatedRenderPassId id{1};
    auto pass = CreateTestRootRenderPass(id, rect);

    // Append a quad to execute the transform.
    {
      SharedQuadState* shared_state =
          CreateTestSharedQuadState(gfx::Transform(), rect, pass.get(),
                                    gfx::MaskFilterInfo());

      ResourceId resource = CreateGpuResource(
          this->child_context_provider_, this->child_resource_provider_.get(),
          rect.size(), SinglePlaneFormat::kRGBA_8888, this->src_color_space_,
          input_colors);

      // Return the mapped resource id.
      std::unordered_map<ResourceId, ResourceId, ResourceIdHasher>
          resource_map = cc::SendResourceAndGetChildToParentMap(
              {resource}, this->resource_provider_.get(),
              this->child_resource_provider_.get(),
              this->child_context_provider_.get());
      ResourceId mapped_resource = resource_map[resource];

      bool needs_blending = true;
      const gfx::PointF uv_top_left(0.0f, 0.0f);
      const gfx::PointF uv_bottom_right(1.0f, 1.0f);
      const bool flipped = false;
      const bool nearest_neighbor = false;
      auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();

      float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
      quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
                   this->premultiplied_alpha_, uv_top_left, uv_bottom_right,
                   SkColors::kBlack, vertex_opacity, flipped, nearest_neighbor,
                   /*secure_output=*/false, gfx::ProtectedVideoType::kClear);

      auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, rect, rect, SkColors::kBlack, false);
    }

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    // Allow a difference of 2 bytes in comparison for most cases.
    float avg_abs_error_limit = 2.0f;
    int max_abs_error_limit = 2;
#if BUILDFLAG(IS_FUCHSIA)
    if (src_color_space_.GetTransferID() == TransferID::PQ) {
      // Fuchsia+SwiftShader/Vulkan has higher error on some pixels with HDR
      // color spaces. See https://crbug.com/1312141.
      max_abs_error_limit = 5;
    }
#endif

    auto comparator = cc::FuzzyPixelComparator()
                          .SetErrorPixelsPercentageLimit(100.f)
                          .SetAvgAbsErrorLimit(avg_abs_error_limit)
                          .SetAbsErrorLimit(max_abs_error_limit);
    EXPECT_TRUE(
        this->RunPixelTest(&pass_list, &expected_output_colors, comparator))
        << " src:" << src_color_space_ << ", dst:" << dst_color_space_;
  }

  gfx::ColorSpace src_color_space_;
  gfx::ColorSpace dst_color_space_;
  bool premultiplied_alpha_ = false;
};

TEST_P(ColorTransformPixelTest, Basic) {
#if BUILDFLAG(IS_LINUX) && defined(THREAD_SANITIZER)
  // Test is flaking with failed large allocations under TSAN when using
  // SkiaRenderer with GL backend. See https://crbug.com/1320955.
  if (renderer_type() == RendererType::kSkiaGL)
    return;
#endif

  Basic();
}

gfx::ColorSpace src_color_spaces[] = {
    // This will be replaced by an ICC-based space (which can't be initialized
    // here).
    gfx::ColorSpace(),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT709),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA28),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE240M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SRGB),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTEST428_1),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SRGB_HDR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR),
    gfx::ColorSpace::CreateHDR10(),
};

gfx::ColorSpace dst_color_spaces[] = {
    // This will be replaced by an ICC-based space (which can't be initialized
    // here).
    gfx::ColorSpace(),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT709),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA28),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE240M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SRGB),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SRGB_HDR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR),
};

gfx::ColorSpace intermediate_color_spaces[] = {
    gfx::ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::XYZ_D50, TransferID::SRGB_HDR),
};

INSTANTIATE_TEST_SUITE_P(
    FromColorSpace,
    ColorTransformPixelTest,
    testing::Combine(testing::ValuesIn(GetGpuRendererTypesNoDawn()),
                     testing::ValuesIn(src_color_spaces),
                     testing::ValuesIn(intermediate_color_spaces),
                     testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ToColorSpace,
    ColorTransformPixelTest,
    testing::Combine(testing::ValuesIn(GetGpuRendererTypesNoDawn()),
                     testing::ValuesIn(intermediate_color_spaces),
                     testing::ValuesIn(dst_color_spaces),
                     testing::Bool()));

// GetGpuRendererTypesNoDawn() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ColorTransformPixelTest);

class DelegatedInkTest : public VizPixelTestWithParam,
                         public DelegatedInkPointPixelTestHelper {
 public:
  void SetUp() override {
    // Partial swap must be enabled or else the test will pass even if the
    // delegated ink trail damage rect is wrong, because the whole frame is
    // always redrawn otherwise.
    renderer_settings_.partial_swap_enabled = true;
    VizPixelTestWithParam::SetUp();
    EXPECT_TRUE(VizPixelTestWithParam::renderer_->use_partial_swap());

    SetRendererAndCreateInkRenderer(VizPixelTestWithParam::renderer_.get());
  }

  std::unique_ptr<AggregatedRenderPass> CreateTestRootRenderPass(
      AggregatedRenderPassId id,
      const gfx::Rect& output_rect,
      const gfx::Rect& damage_rect) {
    auto pass = std::make_unique<AggregatedRenderPass>();
    const gfx::Transform transform_to_root_target;
    pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
    return pass;
  }

  bool DrawAndTestTrail(base::FilePath::StringPieceType file) {
    gfx::Rect rect(this->device_viewport_size_);

    // Minimize the root render pass damage rect so that it has to be expanded
    // by the delegated ink trail damage rect to confirm that it is the right
    // size to remove old trails and add new ones.
    gfx::Rect damage_rect(0, 0, 1, 1);
    AggregatedRenderPassId id{1};
    std::unique_ptr<AggregatedRenderPass> pass =
        CreateTestRootRenderPass(id, rect, damage_rect);

    SharedQuadState* shared_state = CreateTestSharedQuadState(
        gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

    SolidColorDrawQuad* color_quad =
        pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

    AggregatedRenderPassList pass_list;
    pass_list.push_back(std::move(pass));

    return this->RunPixelTest(
        &pass_list, base::FilePath(file),
        cc::AlphaDiscardingFuzzyPixelOffByOneComparator());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         DelegatedInkTest,
                         testing::ValuesIn(GetRendererTypesSkiaOnly()),
                         testing::PrintToStringParamName());
// GetRendererTypesSkiaOnly() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DelegatedInkTest);

// Test to confirm that predicted points are not drawn if prediction is not
// enabled, since it is disabled by default.
TEST_P(DelegatedInkTest, DrawTrailWithPredictionDisabled) {
  // Send some DelegatedInkPoints, numbers arbitrary. Sending 4 points will
  // cause prediction to be available.
  const gfx::PointF kFirstPoint(20, 35);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(56, 92));
  CreateAndSendPointFromLastPoint(gfx::PointF(101, 145));
  CreateAndSendPointFromLastPoint(gfx::PointF(106, 170));

  // Provide the metadata required to draw the trail, matching the first
  // DelegatedInkPoint sent.
  CreateAndSendMetadata(kFirstPoint, 3.5f, SkColors::kCyan, kFirstTimestamp,
                        gfx::RectF(0, 0, 200, 200));

  // Confirm that the trail was drawn without prediction.
  EXPECT_TRUE(DrawAndTestTrail(
      FILE_PATH_LITERAL("delegated_ink_trail_no_prediction.png")));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL("white.png")));
}

class DelegatedInkWithPredictionTest : public DelegatedInkTest {
  void SetUp() override {
    EnablePrediction();
    DelegatedInkTest::SetUp();
  }

  virtual void EnablePrediction() {
    base::FieldTrialParams params;
    params["predicted_points"] = ::features::kDraw1Point12Ms;
    base::test::FeatureRefAndParams prediction_params = {
        features::kDrawPredictedInkPoint, params};

    feature_list_.Reset();
    feature_list_.InitWithFeaturesAndParameters({prediction_params}, {});
  }
};

INSTANTIATE_TEST_SUITE_P(,
                         DelegatedInkWithPredictionTest,
                         testing::ValuesIn(GetRendererTypesSkiaOnly()),
                         testing::PrintToStringParamName());

// GetRendererTypesSkiaOnly() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DelegatedInkWithPredictionTest);

// Draw a single trail and erase it, making sure that no bits of trail are left
// behind.
TEST_P(DelegatedInkWithPredictionTest, DrawOneTrailAndErase) {
  // Send some DelegatedInkPoints, numbers arbitrary. This will predict no
  // points, so a trail made of 3 points will be drawn.
  const gfx::PointF kFirstPoint(10, 10);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(75, 62));
  CreateAndSendPointFromLastPoint(gfx::PointF(124, 45));

  // Provide the metadata required to draw the trail, matching the first
  // DelegatedInkPoint sent.
  CreateAndSendMetadata(kFirstPoint, 3.5f, SkColors::kBlack, kFirstTimestamp,
                        gfx::RectF(0, 0, 175, 172));
  // Confirm that the trail was drawn. Test three times as
  // the trail will persist for two more frames before being erased.
  EXPECT_TRUE(
      DrawAndTestTrail(FILE_PATH_LITERAL("delegated_ink_one_trail.png")));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL("white.png")));
}

// Confirm that drawing a second trail completely removes the first trail.
TEST_P(DelegatedInkWithPredictionTest, DrawTwoTrailsAndErase) {
  // TODO(crbug.com/1021566): Enable this test for SkiaRenderer Dawn.
  if (renderer_type() == RendererType::kSkiaDawn)
    return;

  // Numbers chosen arbitrarily. No points will be predicted, so a trail made of
  // 2 points will be drawn.
  const gfx::PointF kFirstPoint(140, 48);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(115, 85));

  // Provide the metadata required to draw the trail, numbers matching the first
  // DelegatedInkPoint sent.
  CreateAndSendMetadata(kFirstPoint, 8.2f, SkColors::kMagenta, kFirstTimestamp,
                        gfx::RectF(0, 0, 200, 200));

  // Confirm that the trail was drawn correctly.
  EXPECT_TRUE(DrawAndTestTrail(
      FILE_PATH_LITERAL("delegated_ink_two_trails_first.png")));

  // Now provide new metadata and points to draw a new trail. Just use the last
  // point draw above as the starting point for the new trail. One point will
  // be predicted, so a trail consisting of 4 points will be drawn.
  CreateAndSendMetadataFromLastPoint();
  CreateAndSendPointFromLastPoint(gfx::PointF(134, 100));
  CreateAndSendPointFromLastPoint(gfx::PointF(150, 81.44f));

  // Confirm the first trail is gone and only the second remains.
  EXPECT_TRUE(DrawAndTestTrail(
      FILE_PATH_LITERAL("delegated_ink_two_trails_second.png")));

  // Confirm all trails are gone.
  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL("white.png")));
}

// Confirm that the trail can't be drawn beyond the presentation area.
TEST_P(DelegatedInkWithPredictionTest, TrailExtendsBeyondPresentationArea) {
  // TODO(crbug.com/1021566): Enable this test for SkiaRenderer Dawn.
  if (renderer_type() == RendererType::kSkiaDawn)
    return;

  const gfx::PointF kFirstPoint(50.2f, 89.999f);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();

  // Send points such that some extend beyond the presentation area to confirm
  // that the trail is clipped correctly. One point will be predicted, so the
  // trail will be made of 9 points.
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(80.7f, 149.6f));
  CreateAndSendPointFromLastPoint(gfx::PointF(128.999f, 110.01f));
  CreateAndSendPointFromLastPoint(gfx::PointF(50, 50));
  CreateAndSendPointFromLastPoint(gfx::PointF(10.1f, 30.3f));
  CreateAndSendPointFromLastPoint(gfx::PointF(29.98f, 66));
  CreateAndSendPointFromLastPoint(gfx::PointF(52.3456f, 2.31f));
  CreateAndSendPointFromLastPoint(gfx::PointF(97, 36.9f));

  const gfx::RectF kPresentationArea(30, 30, 100, 100);
  CreateAndSendMetadata(kFirstPoint, 15.22f, SkColors::kCyan, kFirstTimestamp,
                        kPresentationArea);

  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL(
      "delegated_ink_trail_clipped_by_presentation_area.png")));
}

// Confirm that the trail appears on top of everything, including batched quads
// that are drawn as part of the call to FinishDrawingRenderPass.
TEST_P(DelegatedInkWithPredictionTest, DelegatedInkTrailAfterBatchedQuads) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId id{1};
  auto pass = CreateTestRootRenderPass(id, rect, rect);

  SharedQuadState* shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, pass.get(), gfx::MaskFilterInfo());

  CreateTestTextureDrawQuad(
      !is_software_renderer(), gfx::Rect(this->device_viewport_size_),
      SkColor4f::FromColor(SkColorSetARGB(128, 0, 255, 0)),  // Texel color.
      SkColors::kTransparent,  // Background color.
      true,                    // Premultiplied alpha.
      shared_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SkColors::kWhite, false);

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  const gfx::PointF kFirstPoint(34.f, 72.f);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(79, 101));
  CreateAndSendPointFromLastPoint(gfx::PointF(134, 114));

  const gfx::RectF kPresentationArea(0, 0, 200, 200);
  CreateAndSendMetadata(kFirstPoint, 7.77f, SkColors::kDkGray, kFirstTimestamp,
                        kPresentationArea);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(
          FILE_PATH_LITERAL("delegated_ink_trail_on_batched_quads.png")),
      cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Confirm that delegated ink trails are not drawn on non-root render passes.
TEST_P(DelegatedInkWithPredictionTest, SimpleTrailNonRootRenderPass) {
  gfx::Rect rect(this->device_viewport_size_);

  AggregatedRenderPassId child_id{2};
  auto child_pass = CreateTestRenderPass(child_id, rect, gfx::Transform());

  SharedQuadState* child_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, child_pass.get(), gfx::MaskFilterInfo());

  auto* color_quad = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(child_shared_state, rect, rect, SkColors::kGreen, false);

  AggregatedRenderPassId root_id{1};
  auto root_pass = CreateTestRootRenderPass(root_id, rect, rect);

  SharedQuadState* root_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), rect, root_pass.get(), gfx::MaskFilterInfo());

  CreateTestRenderPassDrawQuad(root_shared_state, rect, child_id,
                               root_pass.get());

  auto* child_pass_ptr = child_pass.get();

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Values for a simple delegated ink trail, numbers chosen arbitrarily.
  const gfx::PointF kFirstPoint(156.f, 111.f);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(119, 87.23f));
  CreateAndSendPointFromLastPoint(gfx::PointF(74.222f, 95.4f));

  const gfx::RectF kPresentationArea(0, 0, 200, 200);
  CreateAndSendMetadata(kFirstPoint, 19.177f, SkColors::kRed, kFirstTimestamp,
                        kPresentationArea);

  // This will only check what was drawn in the child pass, which should never
  // contain a delegated ink trail, so it should be solid green.
  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list, child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("green.png")),
      cc::AlphaDiscardingExactPixelComparator()));
}

// Draw two different trails that are made up of sets of DelegatedInkPoints with
// different pointer IDs. All numbers arbitrarily chosen.
TEST_P(DelegatedInkWithPredictionTest, DrawTrailsWithDifferentPointerIds) {
  const int32_t kPointerId1 = 2;
  const int32_t kPointerId2 = 100;

  const base::TimeTicks kTimestamp = base::TimeTicks::Now();

  // Constants used for sending points and making sure we can send matching
  // DelegatedInkMetadata later.
  const gfx::PointF kPointerId1StartPoint(40, 27);
  const base::TimeTicks kPointerId1StartTime = kTimestamp;
  const gfx::PointF kPointerId2StartPoint(160, 190);
  const base::TimeTicks kPointerId2StartTime =
      kTimestamp + base::Milliseconds(15);

  // Send four points for pointer ID 1 and two points for pointer ID 2 in mixed
  // order to confirm that they get put in the right buckets. Some timestamps
  // match intentionally to make sure that point is considered when matching
  // DelegatedInkMetadata to DelegatedInkPoints
  CreateAndSendPoint(kPointerId1StartPoint, kPointerId1StartTime, kPointerId1);
  CreateAndSendPoint(gfx::PointF(24, 80), kTimestamp + base::Milliseconds(15),
                     kPointerId1);
  CreateAndSendPoint(kPointerId2StartPoint, kPointerId2StartTime, kPointerId2);
  CreateAndSendPoint(gfx::PointF(60, 130), kTimestamp + base::Milliseconds(24),
                     kPointerId1);
  CreateAndSendPoint(gfx::PointF(80, 118), kTimestamp + base::Milliseconds(20),
                     kPointerId2);
  CreateAndSendPoint(gfx::PointF(100, 190), kTimestamp + base::Milliseconds(30),
                     kPointerId1);

  const gfx::RectF kPresentationArea(200, 200);

  // Now send a metadata to match the first point of the first pointer id to
  // confirm that only that trail is drawn.
  CreateAndSendMetadata(kPointerId1StartPoint, 7, SkColors::kYellow,
                        kPointerId1StartTime, kPresentationArea);
  EXPECT_TRUE(
      DrawAndTestTrail(FILE_PATH_LITERAL("delegated_ink_pointer_id_1.png")));

  // Then send metadata that matches the first point of the other pointer id.
  // These points should not have been erased, so all 3 points should be drawn.
  CreateAndSendMetadata(kPointerId2StartPoint, 2.4f, SkColors::kRed,
                        kPointerId2StartTime, kPresentationArea);
  EXPECT_TRUE(
      DrawAndTestTrail(FILE_PATH_LITERAL("delegated_ink_pointer_id_2.png")));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL("white.png")));
}

// Draw a single trail and erase it, making sure that no bits of trail are left
// behind.
TEST_P(DelegatedInkWithPredictionTest,
       IdenticalTrailDrawnAfterSameMetadataReceived) {
  // Send some DelegatedInkPoints, numbers arbitrary. This will predict no
  // points, so a trail made of 3 points will be drawn.
  const gfx::PointF kFirstPoint(10, 10);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(75, 62));
  CreateAndSendPointFromLastPoint(gfx::PointF(124, 45));

  // Provide the metadata required to draw the trail, matching the first
  // DelegatedInkPoint sent.
  CreateAndSendMetadata(kFirstPoint, 3.5f, SkColors::kBlack, kFirstTimestamp,
                        gfx::RectF(0, 0, 175, 172));
  // Confirm that the trail was drawn. Test three times as
  // the trail will persist for two more frames before being erased.
  EXPECT_TRUE(
      DrawAndTestTrail(FILE_PATH_LITERAL("delegated_ink_one_trail.png")));

  // Send metadata again and expect the same trail to be drawn.
  CreateAndSendMetadata(kFirstPoint, 3.5f, SkColors::kBlack, kFirstTimestamp,
                        gfx::RectF(0, 0, 175, 172));
  EXPECT_TRUE(
      DrawAndTestTrail(FILE_PATH_LITERAL("delegated_ink_one_trail.png")));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(FILE_PATH_LITERAL("white.png")));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace
}  // namespace viz
