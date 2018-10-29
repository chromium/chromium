// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <tuple>

#include "base/memory/aligned_memory.h"
#include "build/build_config.h"
#include "cc/base/math_util.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/skia_paint_canvas.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/pixel_test.h"
#include "cc/test/render_pass_test_utils.h"
#include "cc/test/resource_provider_test_utils.h"
#include "cc/test/test_in_process_context_provider.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/gpu/texture_allocation.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/gl_renderer.h"
#include "components/viz/test/test_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "media/base/video_frame.h"
#include "media/renderers/video_resource_updater.h"
#include "media/video/half_float_maker.h"
#include "third_party/skia/include/core/SkColorPriv.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/effects/SkColorMatrixFilter.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/test/icc_profiles.h"

using cc::RendererPixelTest;
using cc::GLRendererPixelTest;
using cc::SkiaRendererPixelTest;
using gpu::gles2::GLES2Interface;

namespace viz {
namespace {

#if !defined(OS_ANDROID)
std::unique_ptr<base::SharedMemory> AllocateAndRegisterSharedBitmapMemory(
    const SharedBitmapId& id,
    const gfx::Size& size,
    SharedBitmapManager* shared_bitmap_manager) {
  std::unique_ptr<base::SharedMemory> shm =
      bitmap_allocation::AllocateMappedBitmap(size, RGBA_8888);
  shared_bitmap_manager->ChildAllocatedSharedBitmap(
      bitmap_allocation::DuplicateAndCloseMappedBitmap(shm.get(), size,
                                                       RGBA_8888),
      id);
  return shm;
}

void DeleteTexture(scoped_refptr<ContextProvider> context_provider,
                   GLuint texture,
                   const gpu::SyncToken& sync_token,
                   bool is_lost) {
  DCHECK(context_provider);
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  DCHECK(gl);
  gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  gl->DeleteTextures(1, &texture);
}

ResourceId CreateGpuResource(scoped_refptr<ContextProvider> context_provider,
                             ClientResourceProvider* resource_provider,
                             const gfx::Size& size,
                             ResourceFormat format,
                             gfx::ColorSpace color_space,
                             const void* pixels = nullptr) {
  DCHECK(context_provider);
  gpu::gles2::GLES2Interface* gl = context_provider->ContextGL();
  DCHECK(gl);
  const gpu::Capabilities& caps = context_provider->ContextCapabilities();
  auto allocation = TextureAllocation::MakeTextureId(
      gl, caps, format,
      /*use_gpu_memory_buffer_resources=*/false,
      /*for_framebuffer_attachment=*/false);
  if (pixels) {
    TextureAllocation::UploadStorage(gl, caps, format, size, allocation,
                                     color_space, pixels);
  } else {
    TextureAllocation::AllocateStorage(gl, caps, format, size, allocation,
                                       color_space);
  }
  gpu::Mailbox mailbox;
  gl->ProduceTextureDirectCHROMIUM(allocation.texture_id, mailbox.name);
  gpu::SyncToken sync_token;
  gl->GenSyncTokenCHROMIUM(sync_token.GetData());
  TransferableResource gl_resource = TransferableResource::MakeGL(
      mailbox, GL_LINEAR, allocation.texture_target, sync_token);
  gl_resource.size = size;
  gl_resource.format = format;
  gl_resource.color_space = std::move(color_space);
  auto release_callback = SingleReleaseCallback::Create(base::BindOnce(
      &DeleteTexture, std::move(context_provider), allocation.texture_id));
  return resource_provider->ImportResource(gl_resource,
                                           std::move(release_callback));
}

std::unique_ptr<RenderPass> CreateTestRootRenderPass(int id,
                                                     const gfx::Rect& rect) {
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  const gfx::Transform transform_to_root_target;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass;
}

std::unique_ptr<RenderPass> CreateTestRenderPass(
    int id,
    const gfx::Rect& rect,
    const gfx::Transform& transform_to_root_target) {
  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  const gfx::Rect output_rect = rect;
  const gfx::Rect damage_rect = rect;
  pass->SetNew(id, output_rect, damage_rect, transform_to_root_target);
  return pass;
}

SharedQuadState* CreateTestSharedQuadState(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    RenderPass* render_pass) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const gfx::Rect clip_rect = rect;
  const bool is_clipped = false;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       clip_rect, is_clipped, are_contents_opaque, opacity,
                       blend_mode, sorting_context_id);
  return shared_state;
}

SharedQuadState* CreateTestSharedQuadStateClipped(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    const gfx::Rect& clip_rect,
    RenderPass* render_pass) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = clip_rect;
  const bool is_clipped = true;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       clip_rect, is_clipped, are_contents_opaque, opacity,
                       blend_mode, sorting_context_id);
  return shared_state;
}

void CreateTestRenderPassDrawQuad(const SharedQuadState* shared_state,
                                  const gfx::Rect& rect,
                                  int pass_id,
                                  RenderPass* render_pass) {
  auto* quad = render_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  quad->SetNew(shared_state, rect, rect, pass_id,
               0,                 // mask_resource_id
               gfx::RectF(),      // mask_uv_rect
               gfx::Size(),       // mask_texture_size
               gfx::Vector2dF(),  // filters scale
               gfx::PointF(),     // filter origin
               gfx::RectF(rect),  // tex_coord_rect
               false);            // force_anti_aliasing_off
}

void CreateTestTwoColoredTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor texel_color,
    SkColor texel_stripe_color,
    SkColor background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    RenderPass* render_pass) {
  SkPMColor pixel_color = premultiplied_alpha
                              ? SkPreMultiplyColor(texel_color)
                              : SkPackARGB32NoCheck(SkColorGetA(texel_color),
                                                    SkColorGetR(texel_color),
                                                    SkColorGetG(texel_color),
                                                    SkColorGetB(texel_color));
  SkPMColor pixel_stripe_color =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_stripe_color)
          : SkPackARGB32NoCheck(SkColorGetA(texel_stripe_color),
                                SkColorGetR(texel_stripe_color),
                                SkColorGetG(texel_stripe_color),
                                SkColorGetB(texel_stripe_color));
  std::vector<uint32_t> pixels(rect.size().GetArea(), pixel_color);
  for (int i = rect.height() / 4; i < (rect.height() * 3 / 4); ++i) {
    for (int k = rect.width() / 4; k < (rect.width() * 3 / 4); ++k) {
      pixels[i * rect.width() + k] = pixel_stripe_color;
    }
  }

  ResourceId resource;
  if (gpu_resource) {
    resource = CreateGpuResource(std::move(child_context_provider),
                                 child_resource_provider, rect.size(),
                                 RGBA_8888, gfx::ColorSpace(), &pixels.front());
  } else {
    SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
    std::unique_ptr<base::SharedMemory> shm =
        AllocateAndRegisterSharedBitmapMemory(shared_bitmap_id, rect.size(),
                                              shared_bitmap_manager);
    resource = child_resource_provider->ImportResource(
        TransferableResource::MakeSoftware(shared_bitmap_id, rect.size(),
                                           RGBA_8888),
        SingleReleaseCallback::Create(base::DoNothing()));

    for (int i = 0; i < rect.size().GetArea(); ++i)
      static_cast<uint32_t*>(shm->memory())[i] = pixels[i];
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider.get());
  ResourceId mapped_resource = resource_map[resource];

  bool needs_blending = true;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const gfx::PointF uv_top_left(0.0f, 0.0f);
  const gfx::PointF uv_bottom_right(1.0f, 1.0f);
  const bool flipped = false;
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
               premultiplied_alpha, uv_top_left, uv_bottom_right,
               background_color, vertex_opacity, flipped, nearest_neighbor,
               false);
}

void CreateTestTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor texel_color,
    float vertex_opacity[4],
    SkColor background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    RenderPass* render_pass) {
  SkPMColor pixel_color = premultiplied_alpha
                              ? SkPreMultiplyColor(texel_color)
                              : SkPackARGB32NoCheck(SkColorGetA(texel_color),
                                                    SkColorGetR(texel_color),
                                                    SkColorGetG(texel_color),
                                                    SkColorGetB(texel_color));
  size_t num_pixels = static_cast<size_t>(rect.width()) * rect.height();
  std::vector<uint32_t> pixels(num_pixels, pixel_color);

  ResourceId resource;
  if (gpu_resource) {
    resource = CreateGpuResource(std::move(child_context_provider),
                                 child_resource_provider, rect.size(),
                                 RGBA_8888, gfx::ColorSpace(), &pixels.front());
  } else {
    SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
    std::unique_ptr<base::SharedMemory> shm =
        AllocateAndRegisterSharedBitmapMemory(shared_bitmap_id, rect.size(),
                                              shared_bitmap_manager);
    resource = child_resource_provider->ImportResource(
        TransferableResource::MakeSoftware(shared_bitmap_id, rect.size(),
                                           RGBA_8888),
        SingleReleaseCallback::Create(base::DoNothing()));

    for (int i = 0; i < rect.size().GetArea(); ++i)
      static_cast<uint32_t*>(shm->memory())[i] = pixels[i];
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
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
               false);
}

void CreateTestTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor texel_color,
    SkColor background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    SharedBitmapManager* shared_bitmap_manager,
    scoped_refptr<ContextProvider> child_context_provider,
    RenderPass* render_pass) {
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
    RenderPass* render_pass,
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
    memset(video_frame->data(media::VideoFrame::kAPlane), alpha_value,
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
      SingleReleaseCallback::Create(
          std::move(resources.release_callbacks[media::VideoFrame::kYPlane])));
  ResourceId resource_u = child_resource_provider->ImportResource(
      resources.resources[media::VideoFrame::kUPlane],
      SingleReleaseCallback::Create(
          std::move(resources.release_callbacks[media::VideoFrame::kUPlane])));
  ResourceId resource_v = child_resource_provider->ImportResource(
      resources.resources[media::VideoFrame::kVPlane],
      SingleReleaseCallback::Create(
          std::move(resources.release_callbacks[media::VideoFrame::kVPlane])));
  ResourceId resource_a = 0;
  if (with_alpha) {
    resource_a = child_resource_provider->ImportResource(
        resources.resources[media::VideoFrame::kAPlane],
        SingleReleaseCallback::Create(std::move(
            resources.release_callbacks[media::VideoFrame::kAPlane])));
  }

  std::vector<ResourceId> resource_ids_to_transfer;
  resource_ids_to_transfer.push_back(resource_y);
  resource_ids_to_transfer.push_back(resource_u);
  resource_ids_to_transfer.push_back(resource_v);
  if (with_alpha)
    resource_ids_to_transfer.push_back(resource_a);
  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          resource_ids_to_transfer, resource_provider, child_resource_provider,
          child_context_provider);

  ResourceId mapped_resource_y = resource_map[resource_y];
  ResourceId mapped_resource_u = resource_map[resource_u];
  ResourceId mapped_resource_v = resource_map[resource_v];
  ResourceId mapped_resource_a = 0;
  if (with_alpha)
    mapped_resource_a = resource_map[resource_a];
  const gfx::Size ya_tex_size = video_frame->coded_size();
  const gfx::Size uv_tex_size = media::VideoFrame::PlaneSize(
      video_frame->format(), media::VideoFrame::kUPlane,
      video_frame->coded_size());
  DCHECK(uv_tex_size == media::VideoFrame::PlaneSize(
                            video_frame->format(), media::VideoFrame::kVPlane,
                            video_frame->coded_size()));
  if (with_alpha) {
    DCHECK(ya_tex_size == media::VideoFrame::PlaneSize(
                              video_frame->format(), media::VideoFrame::kAPlane,
                              video_frame->coded_size()));
  }

  gfx::RectF ya_tex_coord_rect(tex_coord_rect.x() * ya_tex_size.width(),
                               tex_coord_rect.y() * ya_tex_size.height(),
                               tex_coord_rect.width() * ya_tex_size.width(),
                               tex_coord_rect.height() * ya_tex_size.height());
  gfx::RectF uv_tex_coord_rect(tex_coord_rect.x() * uv_tex_size.width(),
                               tex_coord_rect.y() * uv_tex_size.height(),
                               tex_coord_rect.width() * uv_tex_size.width(),
                               tex_coord_rect.height() * uv_tex_size.height());

  auto* yuv_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  uint32_t bits_per_channel = 8;
  if (video_frame->format() == media::PIXEL_FORMAT_YUV420P10 ||
      video_frame->format() == media::PIXEL_FORMAT_YUV422P10 ||
      video_frame->format() == media::PIXEL_FORMAT_YUV444P10) {
    bits_per_channel = 10;
  }

  ResourceFormat yuv_highbit_resource_format =
      video_resource_updater->YuvResourceFormat(bits_per_channel);

  float offset = 0.0f;
  float multiplier = 1.0f;

  if (yuv_highbit_resource_format == R16_EXT) {
    multiplier = 65535.0f / ((1 << bits_per_channel) - 1);
  } else if (yuv_highbit_resource_format == LUMINANCE_F16) {
    std::unique_ptr<media::HalfFloatMaker> half_float_maker =
        media::HalfFloatMaker::NewHalfFloatMaker(bits_per_channel);
    offset = half_float_maker->Offset();
    multiplier = half_float_maker->Multiplier();
  } else {
    bits_per_channel = 8;
  }

  yuv_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                   ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                   uv_tex_size, mapped_resource_y, mapped_resource_u,
                   mapped_resource_v, mapped_resource_a, video_color_space,
                   offset, multiplier, bits_per_channel);
}

void CreateTestY16TextureDrawQuad_FromVideoFrame(
    const SharedQuadState* shared_state,
    scoped_refptr<media::VideoFrame> video_frame,
    const gfx::RectF& tex_coord_rect,
    RenderPass* render_pass,
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
      resources.resources[0],
      SingleReleaseCallback::Create(std::move(resources.release_callbacks[0])));

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap({resource_y}, resource_provider,
                                             child_resource_provider,
                                             child_context_provider);
  ResourceId mapped_resource_y = resource_map[resource_y];

  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  bool needs_blending = true;
  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource_y,
               false, tex_coord_rect.origin(), tex_coord_rect.bottom_right(),
               SK_ColorBLACK, vertex_opacity, false, false, false);
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
  ret->metadata()->MergeMetadataFrom(video_frame->metadata());

  for (int plane = media::VideoFrame::kYPlane;
       plane <= media::VideoFrame::kVPlane; ++plane) {
    int width = video_frame->row_bytes(plane);
    const uint8_t* src = video_frame->data(plane);
    uint16_t* dst = reinterpret_cast<uint16_t*>(ret->data(plane));
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
    RenderPass* render_pass,
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
    uint8_t* y_row = video_frame->data(media::VideoFrame::kYPlane) +
                     video_frame->stride(media::VideoFrame::kYPlane) * i;
    for (int j = 0; j < video_frame->row_bytes(media::VideoFrame::kYPlane);
         ++j) {
      y_row[j] = (y_value += 1);
    }
  }
  for (int i = 0; i < video_frame->rows(media::VideoFrame::kUPlane); ++i) {
    uint8_t* u_row = video_frame->data(media::VideoFrame::kUPlane) +
                     video_frame->stride(media::VideoFrame::kUPlane) * i;
    uint8_t* v_row = video_frame->data(media::VideoFrame::kVPlane) +
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
    RenderPass* render_pass,
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
    memset(video_frame->data(planes[i]), yuv_background[i],
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
        video_frame->data(planes[i])[offset] = yuv_foreground[i];
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
    RenderPass* render_pass,
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
  memset(video_frame->data(media::VideoFrame::kYPlane), y,
         video_frame->stride(media::VideoFrame::kYPlane) *
             video_frame->rows(media::VideoFrame::kYPlane));
  memset(video_frame->data(media::VideoFrame::kUPlane), u,
         video_frame->stride(media::VideoFrame::kUPlane) *
             video_frame->rows(media::VideoFrame::kUPlane));
  memset(video_frame->data(media::VideoFrame::kVPlane), v,
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
    RenderPass* render_pass,
    media::VideoResourceUpdater* video_resource_updater,
    const gfx::Rect& rect,
    const gfx::Rect& visible_rect,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    scoped_refptr<ContextProvider> child_context_provider) {
  bool needs_blending = true;
  const gfx::Size ya_tex_size = rect.size();
  const gfx::Size uv_tex_size = media::VideoFrame::PlaneSize(
      media::PIXEL_FORMAT_NV12, media::VideoFrame::kUVPlane, rect.size());

  std::vector<uint8_t> y_pixels(ya_tex_size.GetArea(), y);
  ResourceId resource_y = CreateGpuResource(
      child_context_provider, child_resource_provider, ya_tex_size,
      video_resource_updater->YuvResourceFormat(8), color_space,
      y_pixels.data());

  // U goes in the R component and V goes in the G component.
  uint32_t rgba_pixel = (u << 24) | (v << 16);
  std::vector<uint32_t> uv_pixels(uv_tex_size.GetArea(), rgba_pixel);
  ResourceId resource_u =
      CreateGpuResource(child_context_provider, child_resource_provider,
                        uv_tex_size, RGBA_8888, color_space, uv_pixels.data());
  ResourceId resource_v = resource_u;
  ResourceId resource_a = 0;

  // Transfer resources to the parent, and get the resource map.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource_y, resource_u, resource_v}, resource_provider,
          child_resource_provider, child_context_provider.get());

  ResourceId mapped_resource_y = resource_map[resource_y];
  ResourceId mapped_resource_u = resource_map[resource_u];
  ResourceId mapped_resource_v = resource_map[resource_v];

  gfx::RectF ya_tex_coord_rect(tex_coord_rect.x() * ya_tex_size.width(),
                               tex_coord_rect.y() * ya_tex_size.height(),
                               tex_coord_rect.width() * ya_tex_size.width(),
                               tex_coord_rect.height() * ya_tex_size.height());
  gfx::RectF uv_tex_coord_rect(tex_coord_rect.x() * uv_tex_size.width(),
                               tex_coord_rect.y() * uv_tex_size.height(),
                               tex_coord_rect.width() * uv_tex_size.width(),
                               tex_coord_rect.height() * uv_tex_size.height());

  auto* yuv_quad = render_pass->CreateAndAppendDrawQuad<YUVVideoDrawQuad>();
  yuv_quad->SetNew(shared_state, rect, visible_rect, needs_blending,
                   ya_tex_coord_rect, uv_tex_coord_rect, ya_tex_size,
                   uv_tex_size, mapped_resource_y, mapped_resource_u,
                   mapped_resource_v, resource_a, color_space, 0.0f, 1.0f, 8);
}

void CreateTestY16TextureDrawQuad_TwoColor(
    const SharedQuadState* shared_state,
    const gfx::RectF& tex_coord_rect,
    uint8_t g_foreground,
    uint8_t g_background,
    RenderPass* render_pass,
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
    uint8_t* row = video_frame->data(0) + j * video_frame->stride(0);
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

using RendererTypes =
    ::testing::Types<GLRenderer,
                     SoftwareRenderer,
                     SkiaRenderer,
                     cc::SkiaRendererDDL,
                     cc::GLRendererWithExpandedViewport,
                     cc::SoftwareRendererWithExpandedViewport>;
TYPED_TEST_CASE(RendererPixelTest, RendererTypes);

template <typename RendererType>
class SoftwareRendererPixelTest : public cc::RendererPixelTest<RendererType> {};

using SoftwareRendererTypes =
    ::testing::Types<SoftwareRenderer,
                     cc::SoftwareRendererWithExpandedViewport>;
TYPED_TEST_CASE(SoftwareRendererPixelTest, SoftwareRendererTypes);

// TODO(weiliangc): Move these tests to normal RendererPixelTest as they pass
// with SkiaRenderer. Failed test list recorded in crbug.com/821176.
template <typename RendererType>
class NonSkiaRendererPixelTest : public cc::RendererPixelTest<RendererType> {};

using NonSkiaRendererTypes =
    ::testing::Types<GLRenderer,
                     SoftwareRenderer,
                     cc::GLRendererWithExpandedViewport,
                     cc::SoftwareRendererWithExpandedViewport>;
TYPED_TEST_CASE(NonSkiaRendererPixelTest, NonSkiaRendererTypes);

template <typename RendererType>
class FuzzyForSoftwareOnlyPixelComparator : public cc::PixelComparator {
 public:
  explicit FuzzyForSoftwareOnlyPixelComparator(bool discard_alpha)
      : fuzzy_(discard_alpha), exact_(discard_alpha) {}

  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 private:
  cc::FuzzyPixelOffByOneComparator fuzzy_;
  cc::ExactPixelComparator exact_;
};

template <>
bool FuzzyForSoftwareOnlyPixelComparator<SoftwareRenderer>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template <>
bool FuzzyForSoftwareOnlyPixelComparator<SkiaRenderer>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template <>
bool FuzzyForSoftwareOnlyPixelComparator<cc::SkiaRendererDDL>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template <>
bool FuzzyForSoftwareOnlyPixelComparator<
    cc::SoftwareRendererWithExpandedViewport>::
    Compare(const SkBitmap& actual_bmp, const SkBitmap& expected_bmp) const {
  return fuzzy_.Compare(actual_bmp, expected_bmp);
}

template <typename RendererType>
bool FuzzyForSoftwareOnlyPixelComparator<RendererType>::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  return exact_.Compare(actual_bmp, expected_bmp);
}

TYPED_TEST(RendererPixelTest, SimpleGreenRect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorGREEN, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, SimpleGreenRect_NonRootRenderPass) {
  gfx::Rect rect(this->device_viewport_size_);
  gfx::Rect small_rect(100, 100);

  int child_id = 2;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_id, small_rect, gfx::Transform());

  SharedQuadState* child_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), small_rect, child_pass.get());

  auto* color_quad = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(child_shared_state, rect, rect, SK_ColorGREEN, false);

  int root_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRenderPass(root_id, rect, gfx::Transform());

  SharedQuadState* root_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, root_pass.get());

  CreateTestRenderPassDrawQuad(root_shared_state, small_rect, child_id,
                               root_pass.get());

  RenderPass* child_pass_ptr = child_pass.get();

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list, child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("green_small.png")),
      cc::ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, PremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  CreateTestTextureDrawQuad(
      this->use_gpu(), gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(128, 0, 255, 0),  // Texel color.
      SK_ColorTRANSPARENT,             // Background color.
      true,                            // Premultiplied alpha.
      shared_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TYPED_TEST(RendererPixelTest, PremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      this->use_gpu(), gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(204, 120, 255, 120),  // Texel color.
      SK_ColorGREEN,                       // Background color.
      true,                                // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(GLRendererPixelTest, SolidColorBlend) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  shared_state->opacity = 1 - 16.0f / 255;
  shared_state->blend_mode = SkBlendMode::kDstOut;

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorRED, false);

  SharedQuadState* shared_state_background =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  SkColor background_color = SkColorSetRGB(0xff, 0xff * 14 / 16, 0xff);
  auto* color_quad_background =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad_background->SetNew(shared_state_background, rect, rect,
                                background_color, false);
  // Result should be r=16, g=14, b=16.

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(GLRendererPixelTest, SolidColorWithTemperature) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorYELLOW, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
  color_matrix.set(0, 0, 0.7f);
  color_matrix.set(1, 1, 0.4f);
  color_matrix.set(2, 2, 0.5f);
  output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("temperature_brown.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(GLRendererPixelTest, SolidColorWithTemperature_NonRootRenderPass) {
  // Create a root and a child passes with two different solid color quads.
  RenderPassList render_passes_in_draw_order;
  gfx::Rect viewport_rect(this->device_viewport_size_);
  gfx::Rect root_rect(0, 0, viewport_rect.width(), viewport_rect.height() / 2);
  gfx::Rect child_rect(0, root_rect.bottom(), viewport_rect.width(),
                       root_rect.height());

  // Child pass.
  int child_pass_id = 2;
  RenderPass* child_pass = cc::AddRenderPass(
      &render_passes_in_draw_order, child_pass_id, viewport_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(child_pass, child_rect, SK_ColorGREEN);

  // Root pass.
  int root_pass_id = 1;
  RenderPass* root_pass = cc::AddRenderPass(
      &render_passes_in_draw_order, root_pass_id, viewport_rect,
      gfx::Transform(), cc::FilterOperations());
  cc::AddQuad(root_pass, root_rect, SK_ColorYELLOW);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), viewport_rect, root_pass);
  CreateTestRenderPassDrawQuad(pass_shared_state, viewport_rect, child_pass_id,
                               root_pass);

  // Set a non-identity output color matrix on the output surface, and expect
  // that the colors will be transformed.
  SkMatrix44 color_matrix(SkMatrix44::kIdentity_Constructor);
  color_matrix.set(0, 0, 0.7f);
  color_matrix.set(1, 1, 0.4f);
  color_matrix.set(2, 2, 0.5f);
  output_surface_->set_color_matrix(color_matrix);

  EXPECT_TRUE(this->RunPixelTest(
      &render_passes_in_draw_order,
      base::FilePath(FILE_PATH_LITERAL("temperature_brown_non_root.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(GLRendererPixelTest,
       PremultipliedTextureWithBackgroundAndVertexOpacity) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  texture_quad_state->opacity = 0.8f;

  float vertex_opacity[4] = {1.f, 1.f, 0.f, 0.f};
  CreateTestTextureDrawQuad(
      this->use_gpu(), gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(204, 120, 255, 120),  // Texel color.
      vertex_opacity,
      SK_ColorGREEN,  // Background color.
      true,           // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_alpha_vertex_opacity.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

template <typename TypeParam>
class IntersectingQuadPixelTest : public RendererPixelTest<TypeParam> {
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

    int id = 1;
    render_pass_ = CreateTestRootRenderPass(id, viewport_rect_);

    // Create the front quad rotated on the Z and Y axis.
    gfx::Transform trans;
    trans.Translate3d(0, 0, 0.707 * this->device_viewport_size_.width() / 2.0);
    trans.RotateAboutZAxis(45.0);
    trans.RotateAboutYAxis(45.0);
    front_quad_state_ =
        CreateTestSharedQuadState(trans, viewport_rect_, render_pass_.get());
    front_quad_state_->clip_rect = quad_rect_;
    // Make sure they end up in a 3d sorting context.
    front_quad_state_->sorting_context_id = 1;

    // Create the back quad, and rotate on just the y axis. This will intersect
    // the first quad partially.
    trans = gfx::Transform();
    trans.Translate3d(0, 0, -0.707 * this->device_viewport_size_.width() / 2.0);
    trans.RotateAboutYAxis(-45.0);
    back_quad_state_ =
        CreateTestSharedQuadState(trans, viewport_rect_, render_pass_.get());
    back_quad_state_->sorting_context_id = 1;
    back_quad_state_->clip_rect = quad_rect_;
  }
  void AppendBackgroundAndRunTest(const cc::PixelComparator& comparator,
                                  const base::FilePath::CharType* ref_file) {
    SharedQuadState* background_quad_state = CreateTestSharedQuadState(
        gfx::Transform(), viewport_rect_, render_pass_.get());
    auto* background_quad =
        render_pass_->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(background_quad_state, viewport_rect_,
                            viewport_rect_, SK_ColorWHITE, false);
    pass_list_.push_back(std::move(render_pass_));
    EXPECT_TRUE(
        this->RunPixelTest(&pass_list_, base::FilePath(ref_file), comparator));
  }
  template <typename T>
  T* CreateAndAppendDrawQuad() {
    return render_pass_->CreateAndAppendDrawQuad<T>();
  }

  std::unique_ptr<RenderPass> render_pass_;
  gfx::Rect viewport_rect_;
  SharedQuadState* front_quad_state_;
  SharedQuadState* back_quad_state_;
  gfx::Rect quad_rect_;
  RenderPassList pass_list_;
};

template <typename TypeParam>
class IntersectingQuadGLPixelTest
    : public IntersectingQuadPixelTest<TypeParam> {
 public:
  void SetUp() override {
    IntersectingQuadPixelTest<TypeParam>::SetUp();
    constexpr bool kUseStreamVideoDrawQuad = false;
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr bool kUseR16Texture = false;
    constexpr int kMaxResourceSize = 10000;

    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(), nullptr,
        this->child_resource_provider_.get(), kUseStreamVideoDrawQuad,
        kUseGpuMemoryBufferResources, kUseR16Texture, kMaxResourceSize);
    video_resource_updater2_ = std::make_unique<media::VideoResourceUpdater>(
        this->child_context_provider_.get(), nullptr,
        this->child_resource_provider_.get(), kUseStreamVideoDrawQuad,
        kUseGpuMemoryBufferResources, kUseR16Texture, kMaxResourceSize);
  }

 protected:
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater2_;
};

template <typename TypeParam>
class IntersectingQuadSoftwareTest
    : public IntersectingQuadPixelTest<TypeParam> {};

using SoftwareRendererTypes =
    ::testing::Types<SoftwareRenderer,
                     cc::SoftwareRendererWithExpandedViewport>;
using GLRendererTypes =
    ::testing::Types<GLRenderer, cc::GLRendererWithExpandedViewport>;

TYPED_TEST_CASE(IntersectingQuadPixelTest, RendererTypes);
TYPED_TEST_CASE(IntersectingQuadGLPixelTest, GLRendererTypes);
TYPED_TEST_CASE(IntersectingQuadSoftwareTest, SoftwareRendererTypes);

TYPED_TEST(IntersectingQuadPixelTest, SolidColorQuads) {
  this->SetupQuadStateAndRenderPass();

  auto* quad = this->template CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  auto* quad2 = this->template CreateAndAppendDrawQuad<SolidColorDrawQuad>();

  quad->SetNew(this->front_quad_state_, this->quad_rect_, this->quad_rect_,
               SK_ColorBLUE, false);
  quad2->SetNew(this->back_quad_state_, this->quad_rect_, this->quad_rect_,
                SK_ColorGREEN, false);
  SCOPED_TRACE("IntersectingSolidColorQuads");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator(false, 2.f, 0.f, 256.f, 256, 0.f),
      FILE_PATH_LITERAL("intersecting_blue_green.png"));
}

static inline uint32_t GetSkiaOrGLColor(const SkColor& color) {
  return SkColorSetARGB(SkColorGetA(color), SkColorGetB(color),
                        SkColorGetG(color), SkColorGetR(color));
}

template <typename TypeParam>
uint32_t GetColor(const SkColor& color) {
  return color;
}

template <>
uint32_t GetColor<GLRenderer>(const SkColor& color) {
  return GetSkiaOrGLColor(color);
}

template <>
uint32_t GetColor<SkiaRenderer>(const SkColor& color) {
  return GetSkiaOrGLColor(color);
}

template <>
uint32_t GetColor<cc::SkiaRendererDDL>(const SkColor& color) {
  return GetSkiaOrGLColor(color);
}

template <>
uint32_t GetColor<cc::GLRendererWithExpandedViewport>(const SkColor& color) {
  return GetSkiaOrGLColor(color);
}

TYPED_TEST(IntersectingQuadPixelTest, TexturedQuads) {
  this->SetupQuadStateAndRenderPass();
  CreateTestTwoColoredTextureDrawQuad(
      this->use_gpu(), this->quad_rect_,
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 0)),
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 255)), SK_ColorTRANSPARENT,
      true, this->front_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      this->use_gpu(), this->quad_rect_,
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 255, 0)),
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 0)), SK_ColorTRANSPARENT,
      true, this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, this->render_pass_.get());

  SCOPED_TRACE("IntersectingTexturedQuads");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator(false, 2.f, 0.f, 256.f, 256, 0.f),
      FILE_PATH_LITERAL("intersecting_blue_green_squares.png"));
}

TYPED_TEST(IntersectingQuadSoftwareTest, PictureQuads) {
  bool needs_blending = true;
  this->SetupQuadStateAndRenderPass();
  gfx::Rect outer_rect(this->quad_rect_);
  gfx::Rect inner_rect(this->quad_rect_.x() + (this->quad_rect_.width() / 4),
                       this->quad_rect_.y() + (this->quad_rect_.height() / 4),
                       this->quad_rect_.width() / 2,
                       this->quad_rect_.height() / 2);

  cc::PaintFlags black_flags;
  black_flags.setColor(SK_ColorBLACK);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
  cc::PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);

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
  SCOPED_TRACE("IntersectingPictureQuadsPass");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator(false, 2.f, 0.f, 256.f, 256, 0.f),
      FILE_PATH_LITERAL("intersecting_blue_green_squares.png"));
}

TYPED_TEST(IntersectingQuadPixelTest, RenderPassQuads) {
  this->SetupQuadStateAndRenderPass();
  int child_pass_id1 = 2;
  int child_pass_id2 = 3;
  std::unique_ptr<RenderPass> child_pass1 =
      CreateTestRenderPass(child_pass_id1, this->quad_rect_, gfx::Transform());
  SharedQuadState* child1_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), this->quad_rect_, child_pass1.get());
  std::unique_ptr<RenderPass> child_pass2 =
      CreateTestRenderPass(child_pass_id2, this->quad_rect_, gfx::Transform());
  SharedQuadState* child2_quad_state = CreateTestSharedQuadState(
      gfx::Transform(), this->quad_rect_, child_pass2.get());
  CreateTestTwoColoredTextureDrawQuad(
      this->use_gpu(), this->quad_rect_,
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 0)),
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 255)), SK_ColorTRANSPARENT,
      true, child1_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, child_pass1.get());
  CreateTestTwoColoredTextureDrawQuad(
      this->use_gpu(), this->quad_rect_,
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 255, 0)),
      GetColor<TypeParam>(SkColorSetARGB(255, 0, 0, 0)), SK_ColorTRANSPARENT,
      true, child2_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, child_pass2.get());

  CreateTestRenderPassDrawQuad(this->front_quad_state_, this->quad_rect_,
                               child_pass_id1, this->render_pass_.get());
  CreateTestRenderPassDrawQuad(this->back_quad_state_, this->quad_rect_,
                               child_pass_id2, this->render_pass_.get());

  this->pass_list_.push_back(std::move(child_pass1));
  this->pass_list_.push_back(std::move(child_pass2));
  SCOPED_TRACE("IntersectingRenderQuadsPass");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelComparator(false, 2.f, 0.f, 256.f, 256, 0.f),
      FILE_PATH_LITERAL("intersecting_blue_green_squares.png"));
}

TYPED_TEST(IntersectingQuadGLPixelTest, YUVVideoQuads) {
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

  SCOPED_TRACE("IntersectingVideoQuads");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelOffByOneComparator(false),
      FILE_PATH_LITERAL("intersecting_blue_green_squares_video.png"));
}

TYPED_TEST(IntersectingQuadGLPixelTest, Y16VideoQuads) {
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

  SCOPED_TRACE("IntersectingVideoQuads");
  this->AppendBackgroundAndRunTest(
      cc::FuzzyPixelOffByOneComparator(false),
      FILE_PATH_LITERAL("intersecting_light_dark_squares_video.png"));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_F(GLRendererPixelTest, NonPremultipliedTextureWithoutBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  CreateTestTextureDrawQuad(
      this->use_gpu(), gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(128, 0, 255, 0),  // Texel color.
      SK_ColorTRANSPARENT,             // Background color.
      false,                           // Premultiplied alpha.
      shared_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

// TODO(skaslev): The software renderer does not support non-premultplied alpha.
TEST_F(GLRendererPixelTest, NonPremultipliedTextureWithBackground) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* texture_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  texture_quad_state->opacity = 0.8f;

  CreateTestTextureDrawQuad(
      this->use_gpu(), gfx::Rect(this->device_viewport_size_),
      SkColorSetARGB(204, 120, 255, 120),  // Texel color.
      SK_ColorGREEN,                       // Background color.
      false,                               // Premultiplied alpha.
      texture_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->shared_bitmap_manager_.get(),
      this->child_context_provider_, pass.get());

  SharedQuadState* color_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(color_quad_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

class VideoGLRendererPixelTest : public cc::GLRendererPixelTest {
 protected:
  void CreateEdgeBleedPass(media::VideoPixelFormat format,
                           const gfx::ColorSpace& color_space,
                           RenderPassList* pass_list) {
    gfx::Rect rect(200, 200);

    int id = 1;
    std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

    // Scale the video up so that bilinear filtering kicks in to sample more
    // than just nearest neighbor would.
    gfx::Transform scale_by_2;
    scale_by_2.Scale(2.f, 2.f);
    gfx::Rect half_rect(100, 100);
    SharedQuadState* shared_state =
        CreateTestSharedQuadState(scale_by_2, half_rect, pass.get());

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
    GLRendererPixelTest::SetUp();
    constexpr bool kUseStreamVideoDrawQuad = false;
    constexpr bool kUseGpuMemoryBufferResources = false;
    constexpr bool kUseR16Texture = false;
    constexpr int kMaxResourceSize = 10000;
    video_resource_updater_ = std::make_unique<media::VideoResourceUpdater>(
        child_context_provider_.get(), nullptr, child_resource_provider_.get(),
        kUseStreamVideoDrawQuad, kUseGpuMemoryBufferResources, kUseR16Texture,
        kMaxResourceSize);
  }

  void TearDown() override {
    video_resource_updater_ = nullptr;
    GLRendererPixelTest::TearDown();
  }

  std::unique_ptr<media::VideoResourceUpdater> video_resource_updater_;
};

class VideoGLRendererPixelHiLoTest : public VideoGLRendererPixelTest,
                                     public testing::WithParamInterface<bool> {
 public:
  bool IsHighbit() const { return GetParam(); }
};

TEST_P(VideoGLRendererPixelHiLoTest, SimpleYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, IsHighbit(), gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_P(VideoGLRendererPixelHiLoTest, ClippedYUVRect) {
  gfx::Rect viewport(this->device_viewport_size_);
  gfx::Rect draw_rect(this->device_viewport_size_.width() * 1.5,
                      this->device_viewport_size_.height() * 1.5);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, viewport);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), viewport, pass.get());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, IsHighbit(), gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      video_resource_updater_.get(), draw_rect, viewport,
      resource_provider_.get(), child_resource_provider_.get(),
      child_context_provider_.get());
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_clipped.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelHiLoTest, OffsetYUVRect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // Intentionally sets frame format to I420 for testing coverage.
  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, false, gfx::RectF(0.125f, 0.25f, 0.75f, 0.5f), pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_offset.png")),
      cc::FuzzyPixelComparator(true, 100.0f, 1.0f, 1.0f, 1, 0)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVRectBlack) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // In MPEG color range YUV values of (15,128,128) should produce black.
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateREC601(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 15, 128, 128, pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // If we didn't get black out of the YUV values above, then we probably have a
  // color range issue.
  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("black.png")),
                                 cc::FuzzyPixelOffByOneComparator(true)));
}

// First argument (test case prefix) is intentionally left empty.
INSTANTIATE_TEST_CASE_P(, VideoGLRendererPixelHiLoTest, testing::Bool());

TEST_F(VideoGLRendererPixelTest, SimpleYUVJRect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 149, 43, 21, pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleNV12JRect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // YUV of (149,43,21) should be green (0,255,0) in RGB.
  CreateTestYUVVideoDrawQuad_NV12(
      shared_state, gfx::ColorSpace::CreateJpeg(),
      gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 149, 43, 21, pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::FuzzyPixelOffByOneComparator(true)));
}

// Test that a YUV video doesn't bleed outside of its tex coords when the
// tex coord rect is only a partial subrectangle of the coded contents.
TEST_F(VideoGLRendererPixelTest, YUVEdgeBleed) {
  RenderPassList pass_list;
  CreateEdgeBleedPass(media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
                      &pass_list);
  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, YUVAEdgeBleed) {
  RenderPassList pass_list;
  CreateEdgeBleedPass(media::PIXEL_FORMAT_I420A,
                      gfx::ColorSpace::CreateREC601(), &pass_list);
  // Set the output color space to match the input primaries and transfer.
  pass_list.back()->color_space =
      gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                      gfx::ColorSpace::TransferID::SMPTE170M);
  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, SimpleYUVJRectGrey) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // Dark grey in JPEG color range (in MPEG, this is black).
  CreateTestYUVVideoDrawQuad_Solid(
      shared_state, media::PIXEL_FORMAT_I420, gfx::ColorSpace::CreateJpeg(),
      false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 15, 128, 128, pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("dark_grey.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelHiLoTest, SimpleYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420A, gfx::ColorSpace::CreateREC601(),
      false, false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("yuv_stripes_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, FullyTransparentYUVARect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  // Set the output color space to match the input primaries and transfer.
  pass->color_space = gfx::ColorSpace(gfx::ColorSpace::PrimaryID::SMPTE170M,
                                      gfx::ColorSpace::TransferID::SMPTE170M);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  CreateTestYUVVideoDrawQuad_Striped(
      shared_state, media::PIXEL_FORMAT_I420A, gfx::ColorSpace::CreateREC601(),
      true, false, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), pass.get(),
      video_resource_updater_.get(), rect, rect, resource_provider_.get(),
      child_resource_provider_.get(), child_context_provider_.get());

  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(shared_state, rect, rect, SK_ColorBLACK, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("black.png")),
                                 cc::ExactPixelComparator(true)));
}

TEST_F(VideoGLRendererPixelTest, TwoColorY16Rect) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  gfx::Rect upper_rect(rect.x(), rect.y(), rect.width(), rect.height() / 2);
  CreateTestY16TextureDrawQuad_TwoColor(
      shared_state, gfx::RectF(0.0f, 0.0f, 1.0f, 1.0f), 68, 123, pass.get(),
      video_resource_updater_.get(), rect, rect, upper_rect,
      resource_provider_.get(), child_resource_provider_.get(),
      child_context_provider_.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TYPED_TEST(RendererPixelTest, FastPassColorFilterAlpha) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  SkScalar matrix[20];
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
      sk_make_sp<cc::ColorFilterPaintFilter>(
          SkColorFilter::MakeMatrixFilterRowMajor255(matrix), nullptr)));

  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SK_ColorWHITE,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, FastPassSaturateFilter) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateSaturateFilter(0.5f));

  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SK_ColorWHITE,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, FastPassFilterChain) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateGrayscaleFilter(1.f));
  filters.Append(cc::FilterOperation::CreateBrightnessFilter(0.5f));

  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SK_ColorWHITE,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test blends slightly differently with the software renderer vs. the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_filter_chain.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, FastPassColorFilterAlphaTranslation) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  SkScalar matrix[20];
  float amount = 0.5f;
  matrix[0] = 0.213f + 0.787f * amount;
  matrix[1] = 0.715f - 0.715f * amount;
  matrix[2] = 1.f - (matrix[0] + matrix[1]);
  matrix[3] = 0;
  matrix[4] = 20.f;
  matrix[5] = 0.213f - 0.213f * amount;
  matrix[6] = 0.715f + 0.285f * amount;
  matrix[7] = 1.f - (matrix[5] + matrix[6]);
  matrix[8] = 0;
  matrix[9] = 200.f;
  matrix[10] = 0.213f - 0.213f * amount;
  matrix[11] = 0.715f - 0.715f * amount;
  matrix[12] = 1.f - (matrix[10] + matrix[11]);
  matrix[13] = 0;
  matrix[14] = 1.5f;
  matrix[15] = matrix[16] = matrix[17] = matrix[19] = 0;
  matrix[18] = 1;
  cc::FilterOperations filters;
  filters.Append(cc::FilterOperation::CreateReferenceFilter(
      sk_make_sp<cc::ColorFilterPaintFilter>(
          SkColorFilter::MakeMatrixFilterRowMajor255(matrix), nullptr)));

  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);
  child_pass->filters = filters;

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());
  shared_state->opacity = 0.5f;

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* blank_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(blank_state, viewport_rect, viewport_rect, SK_ColorWHITE,
                false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());

  auto* render_pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  render_pass_quad->SetNew(pass_shared_state, pass_rect, pass_rect,
                           child_pass_id, 0, gfx::RectF(), gfx::Size(),
                           gfx::Vector2dF(), gfx::PointF(),
                           gfx::RectF(pass_rect), false);

  RenderPassList pass_list;

  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // This test has alpha=254 for the software renderer vs. alpha=255 for the gl
  // renderer so use a fuzzy comparator.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_alpha_translate.png")),
      FuzzyForSoftwareOnlyPixelComparator<TypeParam>(false)));
}

TYPED_TEST(RendererPixelTest, EnlargedRenderPassTexture) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::ExactPixelComparator(true)));
}

TYPED_TEST(RendererPixelTest, EnlargedRenderPassTextureWithAntiAliasing) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  gfx::Transform aa_transform;
  aa_transform.Translate(0.5, 0.0);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(aa_transform, pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  SharedQuadState* root_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get());
  auto* background = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  background->SetNew(root_shared_state, gfx::Rect(this->device_viewport_size_),
                     gfx::Rect(this->device_viewport_size_), SK_ColorWHITE,
                     false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  this->renderer_->SetEnlargePassTextureAmountForTesting(gfx::Size(50, 75));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("blue_yellow_anti_aliasing.png")),
      cc::FuzzyPixelComparator(true, 100.f, 0.f, 5.f, 7, 0)));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TYPED_TEST(RendererPixelTest, RenderPassAndMaskWithPartialQuad) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get());

  int child_pass_id = 2;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get());

  // The child render pass is just a green box.
  static const SkColor kCSSGreen = 0xff008000;
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
  flags.setColor(SK_ColorWHITE);
  canvas.clear(SK_ColorTRANSPARENT);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(6, 6, 4, 4);
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(6, 6, 4, 4);
  }

  ResourceId mask_resource_id;
  if (this->use_gpu()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This RenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 50, 200, 100);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the RenderPassDrawQuad.
  auto* mask_quad = root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(sub_rect),                      // tex_coord_rect
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_bottom_right.png")),
      cc::ExactPixelComparator(true)));
}

// This tests the case where we have a RenderPass with a mask, but the quad
// for the masked surface does not include the full surface texture.
TYPED_TEST(RendererPixelTest, RenderPassAndMaskWithPartialQuad2) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  SharedQuadState* root_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, root_pass.get());

  int child_pass_id = 2;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, viewport_rect, transform_to_root);
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), viewport_rect, child_pass.get());

  // The child render pass is just a green box.
  static const SkColor kCSSGreen = 0xff008000;
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
  flags.setColor(SK_ColorWHITE);
  canvas.clear(SK_ColorTRANSPARENT);
  gfx::Rect rect = mask_rect;
  while (!rect.IsEmpty()) {
    rect.Inset(6, 6, 4, 4);
    canvas.drawRect(
        SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()),
        flags);
    rect.Inset(6, 6, 4, 4);
  }

  ResourceId mask_resource_id;
  if (this->use_gpu()) {
    mask_resource_id = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        mask_rect.size(), RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());
  } else {
    mask_resource_id =
        this->AllocateAndFillSoftwareResource(mask_rect.size(), bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {mask_resource_id}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_mask_resource_id = resource_map[mask_resource_id];

  // This RenderPassDrawQuad does not include the full |viewport_rect|
  // which is the size of the child render pass.
  gfx::Rect sub_rect = gfx::Rect(50, 20, 200, 60);
  EXPECT_NE(sub_rect.x(), child_pass->output_rect.x());
  EXPECT_NE(sub_rect.y(), child_pass->output_rect.y());
  EXPECT_NE(sub_rect.right(), child_pass->output_rect.right());
  EXPECT_NE(sub_rect.bottom(), child_pass->output_rect.bottom());

  // Set up a mask on the RenderPassDrawQuad.
  auto* mask_quad = root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  mask_quad->SetNew(
      root_pass_shared_state, sub_rect, sub_rect, child_pass_id,
      mapped_mask_resource_id,
      gfx::ScaleRect(gfx::RectF(sub_rect), 2.f / mask_rect.width(),
                     2.f / mask_rect.height()),  // mask_uv_rect
      gfx::Size(mask_rect.size()),               // mask_texture_size
      gfx::Vector2dF(),                          // filters scale
      gfx::PointF(),                             // filter origin
      gfx::RectF(sub_rect),                      // tex_coord_rect
      false);                                    // force_anti_aliasing_off
  // White background behind the masked render pass.
  auto* white = root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(root_pass_shared_state, viewport_rect, viewport_rect,
                SK_ColorWHITE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("mask_middle.png")),
      cc::ExactPixelComparator(true)));
}

template <typename RendererType>
class RendererPixelTestWithBackgroundFilter
    : public RendererPixelTest<RendererType> {
 protected:
  void SetUpRenderPassList() {
    gfx::Rect device_viewport_rect(this->device_viewport_size_);

    int root_id = 1;
    std::unique_ptr<RenderPass> root_pass =
        CreateTestRootRenderPass(root_id, device_viewport_rect);
    root_pass->has_transparent_background = false;

    gfx::Transform identity_quad_to_target_transform;

    int filter_pass_id = 2;
    gfx::Transform transform_to_root;
    std::unique_ptr<RenderPass> filter_pass = CreateTestRenderPass(
        filter_pass_id, filter_pass_layer_rect_, transform_to_root);
    filter_pass->backdrop_filters = this->backdrop_filters_;

    // A non-visible quad in the filtering render pass.
    {
      SharedQuadState* shared_state =
          CreateTestSharedQuadState(identity_quad_to_target_transform,
                                    filter_pass_layer_rect_, filter_pass.get());
      auto* color_quad =
          filter_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, filter_pass_layer_rect_,
                         filter_pass_layer_rect_, SK_ColorTRANSPARENT, false);
    }

    {
      SharedQuadState* shared_state =
          CreateTestSharedQuadState(filter_pass_to_target_transform_,
                                    filter_pass_layer_rect_, filter_pass.get());
      auto* filter_pass_quad =
          root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
      filter_pass_quad->SetNew(shared_state, filter_pass_layer_rect_,
                               filter_pass_layer_rect_, filter_pass_id,
                               0,                           // mask_resource_id
                               gfx::RectF(),                // mask_uv_rect
                               gfx::Size(),                 // mask_texture_size
                               gfx::Vector2dF(1.0f, 1.0f),  // filters_scale
                               gfx::PointF(),               // filters_origin
                               gfx::RectF(),                // tex_coord_rect
                               false);  // force_anti_aliasing_off
    }

    const int kColumnWidth = device_viewport_rect.width() / 3;

    gfx::Rect left_rect = gfx::Rect(0, 0, kColumnWidth, 20);
    for (int i = 0; left_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, left_rect, root_pass.get());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, left_rect, left_rect, SK_ColorGREEN,
                         false);
      left_rect += gfx::Vector2d(0, left_rect.height() + 1);
    }

    gfx::Rect middle_rect = gfx::Rect(kColumnWidth + 1, 0, kColumnWidth, 20);
    for (int i = 0; middle_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, middle_rect, root_pass.get());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, middle_rect, middle_rect, SK_ColorRED,
                         false);
      middle_rect += gfx::Vector2d(0, middle_rect.height() + 1);
    }

    gfx::Rect right_rect =
        gfx::Rect((kColumnWidth + 1) * 2, 0, kColumnWidth, 20);
    for (int i = 0; right_rect.y() < device_viewport_rect.height(); ++i) {
      SharedQuadState* shared_state = CreateTestSharedQuadState(
          identity_quad_to_target_transform, right_rect, root_pass.get());
      auto* color_quad =
          root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
      color_quad->SetNew(shared_state, right_rect, right_rect, SK_ColorBLUE,
                         false);
      right_rect += gfx::Vector2d(0, right_rect.height() + 1);
    }

    SharedQuadState* shared_state =
        CreateTestSharedQuadState(identity_quad_to_target_transform,
                                  device_viewport_rect, root_pass.get());
    auto* background_quad =
        root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    background_quad->SetNew(shared_state, device_viewport_rect,
                            device_viewport_rect, SK_ColorWHITE, false);

    pass_list_.push_back(std::move(filter_pass));
    pass_list_.push_back(std::move(root_pass));
  }

  RenderPassList pass_list_;
  cc::FilterOperations backdrop_filters_;
  gfx::Transform filter_pass_to_target_transform_;
  gfx::Rect filter_pass_layer_rect_;
};

// The software renderer does not support background filters yet.
using BackgroundFilterRendererTypes =
    ::testing::Types<GLRenderer, SkiaRenderer, cc::SkiaRendererDDL>;

TYPED_TEST_CASE(RendererPixelTestWithBackgroundFilter,
                BackgroundFilterRendererTypes);

TYPED_TEST(RendererPixelTestWithBackgroundFilter, InvertFilter) {
  this->backdrop_filters_.Append(cc::FilterOperation::CreateInvertFilter(1.f));

  this->filter_pass_layer_rect_ = gfx::Rect(this->device_viewport_size_);
  this->filter_pass_layer_rect_.Inset(12, 14, 16, 18);

  this->SetUpRenderPassList();
  EXPECT_TRUE(this->RunPixelTest(
      &this->pass_list_,
      base::FilePath(FILE_PATH_LITERAL("backdrop_filter.png")),
      cc::ExactPixelComparator(true)));
}

class ExternalStencilPixelTest : public GLRendererPixelTest {
 protected:
  void ClearBackgroundToGreen() {
    GLES2Interface* gl = output_surface_->context_provider()->ContextGL();
    output_surface_->EnsureBackbuffer();
    output_surface_->Reshape(device_viewport_size_, 1, gfx::ColorSpace(), true,
                             false);
    gl->ClearColor(0.f, 1.f, 0.f, 1.f);
    gl->Clear(GL_COLOR_BUFFER_BIT);
  }

  void PopulateStencilBuffer() {
    // Set two quadrants of the stencil buffer to 1.
    GLES2Interface* gl = output_surface_->context_provider()->ContextGL();
    output_surface_->EnsureBackbuffer();
    output_surface_->Reshape(device_viewport_size_, 1, gfx::ColorSpace(), true,
                             false);
    gl->ClearStencil(0);
    gl->Clear(GL_STENCIL_BUFFER_BIT);
    gl->Enable(GL_SCISSOR_TEST);
    gl->ClearStencil(1);
    gl->Scissor(0, 0, device_viewport_size_.width() / 2,
                device_viewport_size_.height() / 2);
    gl->Clear(GL_STENCIL_BUFFER_BIT);
    gl->Scissor(device_viewport_size_.width() / 2,
                device_viewport_size_.height() / 2,
                device_viewport_size_.width(), device_viewport_size_.height());
    gl->Clear(GL_STENCIL_BUFFER_BIT);
    gl->StencilFunc(GL_EQUAL, 1, 1);
  }
};

TEST_F(ExternalStencilPixelTest, StencilTestEnabled) {
  ClearBackgroundToGreen();
  PopulateStencilBuffer();
  this->EnableExternalStencilTest();

  // Draw a blue quad that covers the entire device viewport. It should be
  // clipped to the bottom left and top right corners by the external stencil.
  gfx::Rect rect(this->device_viewport_size_);
  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);
  pass->has_transparent_background = false;
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

TEST_F(ExternalStencilPixelTest, StencilTestDisabled) {
  PopulateStencilBuffer();

  // Draw a green quad that covers the entire device viewport. The stencil
  // buffer should be ignored.
  gfx::Rect rect(this->device_viewport_size_);
  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);
  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::ExactPixelComparator(true)));
}

TEST_F(ExternalStencilPixelTest, RenderSurfacesIgnoreStencil) {
  // The stencil test should apply only to the final render pass.
  ClearBackgroundToGreen();
  PopulateStencilBuffer();
  this->EnableExternalStencilTest();

  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  root_pass->has_transparent_background = false;

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height());
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());
  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

// Software renderer does not support anti-aliased edges.
TEST_F(GLRendererPixelTest, AntiAliasing) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  gfx::Transform red_quad_to_target_transform;
  red_quad_to_target_transform.Rotate(10);
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(red_quad_to_target_transform, rect, pass.get());

  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, rect, rect, SK_ColorRED, false);

  gfx::Transform yellow_quad_to_target_transform;
  yellow_quad_to_target_transform.Rotate(5);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      yellow_quad_to_target_transform, rect, pass.get());

  auto* yellow = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(yellow_shared_state, rect, rect, SK_ColorYELLOW, false);

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      blue_quad_to_target_transform, rect, pass.get());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("anti_aliasing.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

// This test tests that anti-aliasing works for axis aligned quads.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, AxisAligned) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform red_quad_to_target_transform;
  red_quad_to_target_transform.Translate(50, 50);
  red_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                     0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(red_quad_to_target_transform, rect, pass.get());

  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, rect, rect, SK_ColorRED, false);

  gfx::Transform yellow_quad_to_target_transform;
  yellow_quad_to_target_transform.Translate(25.5f, 25.5f);
  yellow_quad_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* yellow_shared_state = CreateTestSharedQuadState(
      yellow_quad_to_target_transform, rect, pass.get());

  auto* yellow = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(yellow_shared_state, rect, rect, SK_ColorYELLOW, false);

  gfx::Transform blue_quad_to_target_transform;
  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      blue_quad_to_target_transform, rect, pass.get());

  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("axis_aligned.png")),
      cc::ExactPixelComparator(true)));
}

// This test tests that forcing anti-aliasing off works as expected for
// solid color draw quads.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, SolidColorDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      hole_quad_to_target_transform, rect, pass.get());

  auto* hole = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SK_ColorTRANSPARENT, true);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get());

  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::ExactPixelComparator(false)));
}

// This test tests that forcing anti-aliasing off works as expected for
// render pass draw quads.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, RenderPassDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  int root_pass_id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRenderPass(root_pass_id, rect, transform_to_root);

  int child_pass_id = 2;
  gfx::Transform child_pass_transform;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, rect, child_pass_transform);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, rect, child_pass.get());
  SolidColorDrawQuad* hole =
      child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  hole->SetAll(hole_shared_state, rect, rect, false, SK_ColorTRANSPARENT,
               false);

  bool needs_blending = false;
  bool force_anti_aliasing_off = true;
  gfx::Transform hole_pass_to_target_transform;
  hole_pass_to_target_transform.Translate(50, 50);
  hole_pass_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      hole_pass_to_target_transform, rect, root_pass.get());
  RenderPassDrawQuad* pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  pass_quad->SetAll(pass_shared_state, rect, rect, needs_blending,
                    child_pass_id, 0, gfx::RectF(), gfx::Size(),
                    gfx::Vector2dF(), gfx::PointF(), gfx::RectF(rect),
                    force_anti_aliasing_off);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, root_pass.get());

  SolidColorDrawQuad* green =
      root_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::ExactPixelComparator(false)));
}

// This test tests that forcing anti-aliasing off works as expected for
// tile draw quads.
// Anti-aliasing is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, TileDrawQuadForceAntiAliasingOff) {
  gfx::Rect rect(this->device_viewport_size_);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(32, 32);
  SkCanvas canvas(bitmap);
  canvas.clear(SK_ColorTRANSPARENT);

  gfx::Size tile_size(32, 32);
  ResourceId resource;
  if (this->use_gpu()) {
    resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        tile_size, RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  bool swizzle_contents = true;
  bool contents_premultiplied = true;
  bool needs_blending = false;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = true;
  gfx::Transform hole_quad_to_target_transform;
  hole_quad_to_target_transform.Translate(50, 50);
  hole_quad_to_target_transform.Scale(0.5f + 1.0f / (rect.width() * 2.0f),
                                      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* hole_shared_state = CreateTestSharedQuadState(
      hole_quad_to_target_transform, rect, pass.get());
  TileDrawQuad* hole = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  hole->SetNew(hole_shared_state, rect, rect, needs_blending, mapped_resource,
               gfx::RectF(gfx::Rect(tile_size)), tile_size, swizzle_contents,
               contents_premultiplied, nearest_neighbor,
               force_anti_aliasing_off);

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, rect, pass.get());

  SolidColorDrawQuad* green =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, rect, rect, SK_ColorGREEN, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("force_anti_aliasing_off.png")),
      cc::ExactPixelComparator(false)));
}

TEST_F(GLRendererPixelTest, AntiAliasingPerspective) {
  gfx::Rect rect(this->device_viewport_size_);

  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(1, rect);

  gfx::Rect red_rect(0, 0, 180, 500);
  gfx::Transform red_quad_to_target_transform(
      1.0f, 2.4520f, 10.6206f, 19.0f, 0.0f, 0.3528f, 5.9737f, 9.5f, 0.0f,
      -0.2250f, -0.9744f, 0.0f, 0.0f, 0.0225f, 0.0974f, 1.0f);
  SharedQuadState* red_shared_state = CreateTestSharedQuadState(
      red_quad_to_target_transform, red_rect, pass.get());
  auto* red = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SK_ColorRED, false);

  gfx::Rect green_rect(19, 7, 180, 10);
  SharedQuadState* green_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), green_rect, pass.get());
  auto* green = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(green_shared_state, green_rect, green_rect, SK_ColorGREEN,
                false);

  SharedQuadState* blue_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());
  auto* blue = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, rect, rect, SK_ColorBLUE, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("anti_aliasing_perspective.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

// Trilinear filtering is only supported in the gl renderer.
TEST_F(GLRendererPixelTest, TrilinearFiltering) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);
  root_pass->has_transparent_background = false;

  int child_pass_id = 2;
  gfx::Transform transform_to_root;
  gfx::Rect child_pass_rect(
      ScaleToCeiledSize(this->device_viewport_size_, 4.0f));
  bool generate_mipmap = true;
  std::unique_ptr<RenderPass> child_pass = RenderPass::Create();
  child_pass->SetAll(child_pass_id, child_pass_rect, child_pass_rect,
                     transform_to_root, cc::FilterOperations(),
                     cc::FilterOperations(), gfx::ColorSpace::CreateSRGB(),
                     false, false, false, generate_mipmap);

  gfx::Rect red_rect(child_pass_rect);
  // Small enough red rect that linear filtering will miss it but large enough
  // that it makes a meaningful contribution when using trilinear filtering.
  red_rect.ClampToCenteredSize(gfx::Size(2, child_pass_rect.height()));
  SharedQuadState* red_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), red_rect, child_pass.get());
  auto* red = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  red->SetNew(red_shared_state, red_rect, red_rect, SK_ColorRED, false);

  SharedQuadState* blue_shared_state = CreateTestSharedQuadState(
      gfx::Transform(), child_pass_rect, child_pass.get());
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(blue_shared_state, child_pass_rect, child_pass_rect,
               SK_ColorBLUE, false);

  gfx::Transform child_to_root_transform(SkMatrix::MakeRectToRect(
      RectToSkRect(child_pass_rect), RectToSkRect(viewport_rect),
      SkMatrix::kFill_ScaleToFit));
  SharedQuadState* child_pass_shared_state = CreateTestSharedQuadState(
      child_to_root_transform, child_pass_rect, root_pass.get());
  auto* child_pass_quad =
      root_pass->CreateAndAppendDrawQuad<RenderPassDrawQuad>();
  child_pass_quad->SetNew(child_pass_shared_state, child_pass_rect,
                          child_pass_rect, child_pass_id, 0, gfx::RectF(),
                          gfx::Size(), gfx::Vector2dF(), gfx::PointF(),
                          gfx::RectF(child_pass_rect), false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("trilinear_filtering.png")),
      cc::ExactPixelComparator(true)));
}

TYPED_TEST(SoftwareRendererPixelTest, PictureDrawQuadIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;
  bool nearest_neighbor = false;

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // One clipped blue quad in the lower right corner.  Outside the clip
  // is red, which should not appear.
  gfx::Rect blue_rect(gfx::Size(100, 100));
  gfx::Rect blue_clip_rect(gfx::Point(50, 50), gfx::Size(50, 50));

  std::unique_ptr<cc::FakeRecordingSource> blue_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(blue_rect.size());
  cc::PaintFlags red_flags;
  red_flags.setColor(SK_ColorRED);
  blue_recording->add_draw_rect_with_flags(blue_rect, red_flags);
  cc::PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
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
  green_flags.setColor(SK_ColorGREEN);
  green_recording->add_draw_rect_with_flags(viewport, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, viewport, pass.get());

  auto* green_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(green_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0.f, 0.f, 1.f, 1.f), viewport.size(),
                     nearest_neighbor, texture_format, viewport, 1.f, {},
                     green_raster_source->GetDisplayItemList());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("green_with_blue_corner.png")),
      cc::ExactPixelComparator(true)));
}

// Not WithSkiaGPUBackend since that path currently requires tiles for opacity.
TYPED_TEST(SoftwareRendererPixelTest, PictureDrawQuadOpacity) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  ResourceFormat texture_format = RGBA_8888;
  bool nearest_neighbor = false;

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // One viewport-filling 0.5-opacity green quad.
  std::unique_ptr<cc::FakeRecordingSource> green_recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);
  green_recording->add_draw_rect_with_flags(viewport, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  gfx::Transform green_quad_to_target_transform;
  SharedQuadState* green_shared_state = CreateTestSharedQuadState(
      green_quad_to_target_transform, viewport, pass.get());
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
  white_flags.setColor(SK_ColorWHITE);
  white_recording->add_draw_rect_with_flags(viewport, white_flags);
  white_recording->Rerecord();
  scoped_refptr<cc::RasterSource> white_raster_source =
      white_recording->CreateRasterSource();

  gfx::Transform white_quad_to_target_transform;
  SharedQuadState* white_shared_state = CreateTestSharedQuadState(
      white_quad_to_target_transform, viewport, pass.get());

  auto* white_quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  white_quad->SetNew(white_shared_state, viewport, viewport, needs_blending,
                     gfx::RectF(0, 0, 1, 1), viewport.size(), nearest_neighbor,
                     texture_format, viewport, 1.f, {},
                     white_raster_source->GetDisplayItemList());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("green_alpha.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

template <typename TypeParam>
bool IsSoftwareRenderer() {
  return false;
}

template <>
bool IsSoftwareRenderer<SoftwareRenderer>() {
  return true;
}

template <>
bool IsSoftwareRenderer<cc::SoftwareRendererWithExpandedViewport>() {
  return true;
}

void draw_point_color(SkCanvas* canvas, SkScalar x, SkScalar y, SkColor color) {
  SkPaint paint;
  paint.setColor(color);
  canvas->drawPoint(x, y, paint);
}

// If we disable image filtering, then a 2x2 bitmap should appear as four
// huge sharp squares.
TYPED_TEST(SoftwareRendererPixelTest, PictureDrawQuadDisableImageFiltering) {
  // We only care about this in software mode since bilinear filtering is
  // cheap in hardware.
  if (!IsSoftwareRenderer<TypeParam>())
    return;

  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = false;

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(2, 2);
  ASSERT_NE(surface, nullptr);
  SkCanvas* canvas = surface->getCanvas();
  draw_point_color(canvas, 0, 0, SK_ColorGREEN);
  draw_point_color(canvas, 0, 1, SK_ColorBLUE);
  draw_point_color(canvas, 1, 0, SK_ColorBLUE);
  draw_point_color(canvas, 1, 1, SK_ColorGREEN);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  recording->add_draw_image_with_flags(surface->makeImageSnapshot(),
                                       gfx::Point(), flags);
  recording->Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording->CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get());

  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(0, 0, 2, 2), viewport.size(), nearest_neighbor,
               texture_format, viewport, 1.f, {},
               raster_source->GetDisplayItemList());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  this->disable_picture_quad_image_filtering_ = true;

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

// This disables filtering by setting |nearest_neighbor| on the
// PictureDrawQuad.
TYPED_TEST(SoftwareRendererPixelTest, PictureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = true;

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(2, 2);
  ASSERT_NE(surface, nullptr);
  SkCanvas* canvas = surface->getCanvas();
  draw_point_color(canvas, 0, 0, SK_ColorGREEN);
  draw_point_color(canvas, 0, 1, SK_ColorBLUE);
  draw_point_color(canvas, 1, 0, SK_ColorBLUE);
  draw_point_color(canvas, 1, 1, SK_ColorGREEN);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(viewport.size());
  cc::PaintFlags flags;
  flags.setFilterQuality(kLow_SkFilterQuality);
  recording->add_draw_image_with_flags(surface->makeImageSnapshot(),
                                       gfx::Point(), flags);
  recording->Rerecord();
  scoped_refptr<cc::RasterSource> raster_source =
      recording->CreateRasterSource();

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get());

  auto* quad = pass->CreateAndAppendDrawQuad<PictureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               gfx::RectF(0, 0, 2, 2), viewport.size(), nearest_neighbor,
               texture_format, viewport, 1.f, {},
               raster_source->GetDisplayItemList());

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

// This disables filtering by setting |nearest_neighbor| on the
// TileDrawQuad.
TYPED_TEST(NonSkiaRendererPixelTest, TileDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool swizzle_contents = true;
  bool contents_premultiplied = true;
  bool needs_blending = true;
  bool nearest_neighbor = true;
  bool force_anti_aliasing_off = false;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  SkCanvas canvas(bitmap);
  draw_point_color(&canvas, 0, 0, SK_ColorGREEN);
  draw_point_color(&canvas, 0, 1, SK_ColorBLUE);
  draw_point_color(&canvas, 1, 0, SK_ColorBLUE);
  draw_point_color(&canvas, 1, 1, SK_ColorGREEN);

  gfx::Size tile_size(2, 2);
  ResourceId resource;
  if (this->use_gpu()) {
    resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        tile_size, RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get());

  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, gfx::RectF(gfx::Rect(tile_size)), tile_size,
               swizzle_contents, contents_premultiplied, nearest_neighbor,
               force_anti_aliasing_off);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

// This disables filtering by setting |nearest_neighbor| to true on the
// TextureDrawQuad.
TYPED_TEST(SoftwareRendererPixelTest, TextureDrawQuadNearestNeighbor) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = true;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  SkCanvas canvas(bitmap);
  draw_point_color(&canvas, 0, 0, SK_ColorGREEN);
  draw_point_color(&canvas, 0, 1, SK_ColorBLUE);
  draw_point_color(&canvas, 1, 0, SK_ColorBLUE);
  draw_point_color(&canvas, 1, 1, SK_ColorGREEN);

  gfx::Size tile_size(2, 2);
  ResourceId resource =
      this->AllocateAndFillSoftwareResource(tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, false, gfx::PointF(0, 0), gfx::PointF(1, 1),
               SK_ColorBLACK, vertex_opacity, false, nearest_neighbor, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::FuzzyPixelComparator(false, 2.f, 0.f, 256.f, 256, 0.f)));
}

// This ensures filtering is enabled by setting |nearest_neighbor| to false on
// the TextureDrawQuad.
TYPED_TEST(SoftwareRendererPixelTest, TextureDrawQuadLinear) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool needs_blending = true;
  bool nearest_neighbor = false;

  SkBitmap bitmap;
  bitmap.allocN32Pixels(2, 2);
  {
    SkCanvas canvas(bitmap);
    draw_point_color(&canvas, 0, 0, SK_ColorGREEN);
    draw_point_color(&canvas, 0, 1, SK_ColorBLUE);
    draw_point_color(&canvas, 1, 0, SK_ColorBLUE);
    draw_point_color(&canvas, 1, 1, SK_ColorGREEN);
  }

  gfx::Size tile_size(2, 2);
  ResourceId resource =
      this->AllocateAndFillSoftwareResource(tile_size, bitmap);

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state =
      CreateTestSharedQuadState(quad_to_target_transform, viewport, pass.get());

  float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, viewport, viewport, needs_blending,
               mapped_resource, false, gfx::PointF(0, 0), gfx::PointF(1, 1),
               SK_ColorBLACK, vertex_opacity, false, nearest_neighbor, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Allow for a small amount of error as the blending alogrithm used by Skia is
  // affected by the offset in the expanded rect.
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers_linear.png")),
      cc::FuzzyPixelComparator(false, 100.f, 0.f, 16.f, 16.f, 0.f)));
}

TYPED_TEST(SoftwareRendererPixelTest, PictureDrawQuadNonIdentityScale) {
  gfx::Rect viewport(this->device_viewport_size_);
  // TODO(enne): the renderer should figure this out on its own.
  ResourceFormat texture_format = RGBA_8888;
  bool needs_blending = true;
  bool nearest_neighbor = false;

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

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
  red_flags.setColor(SK_ColorRED);
  green_recording->add_draw_rect_with_flags(viewport, red_flags);
  cc::PaintFlags green_flags;
  green_flags.setColor(SK_ColorGREEN);
  green_recording->add_draw_rect_with_flags(green_rect1, green_flags);
  green_recording->add_draw_rect_with_flags(green_rect2, green_flags);
  green_recording->Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording->CreateRasterSource();

  SharedQuadState* top_right_green_shared_quad_state =
      CreateTestSharedQuadState(green_quad_to_target_transform, viewport,
                                pass.get());

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
                                  viewport, SK_ColorGREEN, false);

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
  blue_layer_rect1.Inset(inset, inset, inset, inset);
  blue_layer_rect2.Inset(inset, inset, inset, inset);

  std::unique_ptr<cc::FakeRecordingSource> recording =
      cc::FakeRecordingSource::CreateFilledRecordingSource(layer_rect.size());

  cc::Region outside(layer_rect);
  outside.Subtract(gfx::ToEnclosingRect(union_layer_rect));
  for (gfx::Rect rect : outside) {
    recording->add_draw_rect_with_flags(rect, red_flags);
  }

  cc::PaintFlags blue_flags;
  blue_flags.setColor(SK_ColorBLUE);
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
      quad_to_target_transform, quad_content_rect, pass.get());

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
      half_green_quad_to_target_transform, half_green_rect, pass.get());
  auto* half_color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  half_color_quad->SetNew(half_green_shared_state, half_green_rect,
                          half_green_rect, SK_ColorGREEN, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("four_blue_green_checkers.png")),
      cc::ExactPixelComparator(true)));
}

using GLRendererPixelTestWithFlippedOutputSurface =
    RendererPixelTest<cc::GLRendererWithFlippedSurface>;

TEST_F(GLRendererPixelTestWithFlippedOutputSurface, ExplicitFlipTest) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Note: RunPixelTest() will issue a CopyOutputRequest on the root pass. The
  // implementation should realize the output surface is flipped, and return a
  // right-side up result regardless (i.e., NOT blue_yellow_flipped.png).
  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::ExactPixelComparator(true)));
}

TEST_F(GLRendererPixelTestWithFlippedOutputSurface, CheckChildPassUnflipped) {
  // This draws a blue rect above a yellow rect with an inverted output surface.
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  gfx::Rect blue_rect(0, 0, this->device_viewport_size_.width(),
                      this->device_viewport_size_.height() / 2);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect yellow_rect(0, this->device_viewport_size_.height() / 2,
                        this->device_viewport_size_.width(),
                        this->device_viewport_size_.height() / 2);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state, yellow_rect, yellow_rect, SK_ColorYELLOW, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  RenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Check that the child pass remains unflipped.
  EXPECT_TRUE(this->RunPixelTestWithReadbackTarget(
      &pass_list, pass_list.front().get(),
      base::FilePath(FILE_PATH_LITERAL("blue_yellow.png")),
      cc::ExactPixelComparator(true)));
}

TEST_F(GLRendererPixelTest, CheckReadbackSubset) {
  gfx::Rect viewport_rect(this->device_viewport_size_);

  int root_pass_id = 1;
  std::unique_ptr<RenderPass> root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect);

  int child_pass_id = 2;
  gfx::Rect pass_rect(this->device_viewport_size_);
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> child_pass =
      CreateTestRenderPass(child_pass_id, pass_rect, transform_to_root);

  gfx::Transform quad_to_target_transform;
  SharedQuadState* shared_state = CreateTestSharedQuadState(
      quad_to_target_transform, viewport_rect, child_pass.get());

  // Draw a green quad full-size with a blue quad in the lower-right corner.
  gfx::Rect blue_rect(this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4,
                      this->device_viewport_size_.width() * 3 / 4,
                      this->device_viewport_size_.height() * 3 / 4);
  auto* blue = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  blue->SetNew(shared_state, blue_rect, blue_rect, SK_ColorBLUE, false);
  gfx::Rect green_rect(0, 0, this->device_viewport_size_.width(),
                       this->device_viewport_size_.height());
  auto* green = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  green->SetNew(shared_state, green_rect, green_rect, SK_ColorGREEN, false);

  SharedQuadState* pass_shared_state =
      CreateTestSharedQuadState(gfx::Transform(), pass_rect, root_pass.get());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  RenderPassList pass_list;
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
      cc::ExactPixelComparator(true), &capture_rect));
}

TEST_F(GLRendererPixelTest, TextureQuadBatching) {
  // This test verifies that multiple texture quads using the same resource
  // get drawn correctly.  It implicitly is trying to test that the
  // GLRenderer does the right thing with its draw quad cache.

  gfx::Rect rect(this->device_viewport_size_);
  bool needs_blending = false;

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);

  SharedQuadState* shared_state =
      CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

  // Make a mask.
  gfx::Rect mask_rect = rect;
  SkBitmap bitmap;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(mask_rect.width(), mask_rect.height()));
  SkCanvas canvas(bitmap);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(SkIntToScalar(4));
  paint.setColor(SK_ColorGREEN);
  canvas.clear(SK_ColorWHITE);
  gfx::Rect inset_rect = rect;
  while (!inset_rect.IsEmpty()) {
    inset_rect.Inset(6, 6, 4, 4);
    canvas.drawRect(SkRect::MakeXYWH(inset_rect.x(), inset_rect.y(),
                                     inset_rect.width(), inset_rect.height()),
                    paint);
    inset_rect.Inset(6, 6, 4, 4);
  }

  ResourceId resource = CreateGpuResource(
      this->child_context_provider_, this->child_resource_provider_.get(),
      mask_rect.size(), RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
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
      texture_quad->SetNew(shared_state, layer_rect, layer_rect, needs_blending,
                           mapped_resource, true, uv_rect.origin(),
                           uv_rect.bottom_right(), SK_ColorWHITE,
                           vertex_opacity, false, false, false);
    }
  }

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list, base::FilePath(FILE_PATH_LITERAL("spiral.png")),
      cc::FuzzyPixelOffByOneComparator(true)));
}

TEST_F(GLRendererPixelTest, TileQuadClamping) {
  gfx::Rect viewport(this->device_viewport_size_);
  bool swizzle_contents = true;
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
  SkCanvas canvas(bitmap);
  SkPaint red;
  red.setColor(SK_ColorRED);
  canvas.drawRect(SkRect::MakeWH(tile_size.width(), tile_size.height()), red);
  SkPaint green;
  green.setColor(SK_ColorGREEN);
  canvas.drawRect(SkRect::MakeWH(layer_size.width(), layer_size.height()),
                  green);

  ResourceId resource;
  if (this->use_gpu()) {
    resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        tile_size, RGBA_8888, gfx::ColorSpace(), bitmap.getPixels());
  } else {
    resource = this->AllocateAndFillSoftwareResource(tile_size, bitmap);
  }
  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, this->resource_provider_.get(),
          this->child_resource_provider_.get(),
          this->child_context_provider_.get());
  ResourceId mapped_resource = resource_map[resource];

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, viewport, transform_to_root);

  // Green quad that should not show any red pixels from outside the
  // tex coord rect.
  gfx::Transform transform;
  transform.Scale(40, 40);
  SharedQuadState* quad_shared =
      CreateTestSharedQuadState(transform, gfx::Rect(layer_size), pass.get());
  auto* quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
  quad->SetNew(quad_shared, gfx::Rect(layer_size), gfx::Rect(layer_size),
               needs_blending, mapped_resource, tex_coord_rect, tile_size,
               swizzle_contents, contents_premultiplied, nearest_neighbor,
               use_aa);

  // Green background.
  SharedQuadState* background_shared =
      CreateTestSharedQuadState(gfx::Transform(), viewport, pass.get());
  auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  color_quad->SetNew(background_shared, viewport, viewport, SK_ColorGREEN,
                     false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(&pass_list,
                                 base::FilePath(FILE_PATH_LITERAL("green.png")),
                                 cc::ExactPixelComparator(true)));
}

class GLRendererPixelTestWithOverdrawFeedback : public GLRendererPixelTest {
 protected:
  void SetUp() override {
    renderer_settings_.show_overdraw_feedback = true;
    GLRendererPixelTest::SetUp();
  }
};

TEST_F(GLRendererPixelTestWithOverdrawFeedback, TranslucentRectangles) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform dark_gray_quad_to_target_transform;
  dark_gray_quad_to_target_transform.Translate(50, 50);
  dark_gray_quad_to_target_transform.Scale(
      0.5f + 1.0f / (rect.width() * 2.0f),
      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* dark_gray_shared_state = CreateTestSharedQuadState(
      dark_gray_quad_to_target_transform, rect, pass.get());

  auto* dark_gray = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  dark_gray->SetNew(dark_gray_shared_state, rect, rect, 0x10444444, false);

  gfx::Transform light_gray_quad_to_target_transform;
  light_gray_quad_to_target_transform.Translate(25.5f, 25.5f);
  light_gray_quad_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* light_gray_shared_state = CreateTestSharedQuadState(
      light_gray_quad_to_target_transform, rect, pass.get());

  auto* light_gray = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  light_gray->SetNew(light_gray_shared_state, rect, rect, 0x10CCCCCC, false);

  gfx::Transform bg_quad_to_target_transform;
  SharedQuadState* bg_shared_state =
      CreateTestSharedQuadState(bg_quad_to_target_transform, rect, pass.get());

  auto* bg = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bg->SetNew(bg_shared_state, rect, rect, SK_ColorBLACK, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("translucent_rectangles.png")),
      cc::ExactPixelComparator(true)));
}

class SkiaRendererPixelTestWithOverdrawFeedback : public SkiaRendererPixelTest {
 protected:
  void SetUp() override {
    renderer_settings_.show_overdraw_feedback = true;
    SkiaRendererPixelTest::SetUp();
  }
};

TEST_F(SkiaRendererPixelTestWithOverdrawFeedback, TranslucentRectangles) {
  gfx::Rect rect(this->device_viewport_size_);

  int id = 1;
  gfx::Transform transform_to_root;
  std::unique_ptr<RenderPass> pass =
      CreateTestRenderPass(id, rect, transform_to_root);

  gfx::Transform dark_gray_quad_to_target_transform;
  dark_gray_quad_to_target_transform.Translate(50, 50);
  dark_gray_quad_to_target_transform.Scale(
      0.5f + 1.0f / (rect.width() * 2.0f),
      0.5f + 1.0f / (rect.height() * 2.0f));
  SharedQuadState* dark_gray_shared_state = CreateTestSharedQuadState(
      dark_gray_quad_to_target_transform, rect, pass.get());

  auto* dark_gray = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  dark_gray->SetNew(dark_gray_shared_state, rect, rect, 0x10444444, false);

  gfx::Transform light_gray_quad_to_target_transform;
  light_gray_quad_to_target_transform.Translate(25.5f, 25.5f);
  light_gray_quad_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* light_gray_shared_state = CreateTestSharedQuadState(
      light_gray_quad_to_target_transform, rect, pass.get());

  auto* light_gray = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  light_gray->SetNew(light_gray_shared_state, rect, rect, 0x10CCCCCC, false);

  gfx::Transform bg_quad_to_target_transform;
  SharedQuadState* bg_shared_state =
      CreateTestSharedQuadState(bg_quad_to_target_transform, rect, pass.get());

  auto* bg = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bg->SetNew(bg_shared_state, rect, rect, SK_ColorBLACK, false);

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  EXPECT_TRUE(this->RunPixelTest(
      &pass_list,
      base::FilePath(FILE_PATH_LITERAL("skia_translucent_rectangles.png")),
      cc::ExactPixelComparator(true)));
}

using ColorSpacePair = std::tuple<gfx::ColorSpace, gfx::ColorSpace, bool>;

class ColorTransformPixelTest
    : public GLRendererPixelTest,
      public testing::WithParamInterface<ColorSpacePair> {
 public:
  ColorTransformPixelTest() {
    // Note that this size of 17 is not random -- it is chosen to match the
    // size of LUTs that are created. If we did not match the LUT size exactly,
    // then the error for LUT based transforms is much larger.
    device_viewport_size_ = gfx::Size(17, 5);
    src_color_space_ = std::get<0>(GetParam());
    dst_color_space_ = std::get<1>(GetParam());
    if (!src_color_space_.IsValid()) {
      src_color_space_ =
          gfx::ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
    }
    if (!dst_color_space_.IsValid()) {
      dst_color_space_ =
          gfx::ICCProfileForTestingNoAnalyticTrFn().GetColorSpace();
    }
    premultiplied_alpha_ = std::get<2>(GetParam());
  }
  gfx::ColorSpace src_color_space_;
  gfx::ColorSpace dst_color_space_;
  bool premultiplied_alpha_ = false;
};

TEST_P(ColorTransformPixelTest, Basic) {
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
        if (premultiplied_alpha_) {
          pixel[x % 3] = gradient_value;
          pixel[3] = gradient_value;
        } else {
          pixel[x % 3] = 0xFF;
          pixel[3] = gradient_value;
        }
      }
    }
  }

  std::unique_ptr<gfx::ColorTransform> transform =
      gfx::ColorTransform::NewColorTransform(
          src_color_space_, dst_color_space_,
          gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);

  for (size_t i = 0; i < expected_output_colors.size(); ++i) {
    gfx::ColorTransform::TriStim color;
    color.set_x(input_colors[4 * i + 0] / 255.f);
    color.set_y(input_colors[4 * i + 1] / 255.f);
    color.set_z(input_colors[4 * i + 2] / 255.f);
    float alpha = input_colors[4 * i + 3] / 255.f;
    if (premultiplied_alpha_ && alpha > 0.0) {
      color.Scale(1.0f / alpha);
    }
    transform->Transform(&color, 1);
    color.Scale(alpha);
    color.set_x(std::min(std::max(0.f, color.x()), 1.f));
    color.set_y(std::min(std::max(0.f, color.y()), 1.f));
    color.set_z(std::min(std::max(0.f, color.z()), 1.f));
    expected_output_colors[i] =
        SkColorSetARGB(255, static_cast<size_t>(255.f * color.x() + 0.5f),
                       static_cast<size_t>(255.f * color.y() + 0.5f),
                       static_cast<size_t>(255.f * color.z() + 0.5f));
  }

  int id = 1;
  std::unique_ptr<RenderPass> pass = CreateTestRootRenderPass(id, rect);
  pass->color_space = dst_color_space_;

  // Append a quad to execute the transform.
  {
    SharedQuadState* shared_state =
        CreateTestSharedQuadState(gfx::Transform(), rect, pass.get());

    ResourceId resource = CreateGpuResource(
        this->child_context_provider_, this->child_resource_provider_.get(),
        rect.size(), RGBA_8888, src_color_space_, input_colors.data());

    // Return the mapped resource id.
    std::unordered_map<ResourceId, ResourceId> resource_map =
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
    auto* quad = pass->CreateAndAppendDrawQuad<TextureDrawQuad>();

    float vertex_opacity[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
                 premultiplied_alpha_, uv_top_left, uv_bottom_right,
                 SK_ColorBLACK, vertex_opacity, flipped, nearest_neighbor,
                 false);

    auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
    color_quad->SetNew(shared_state, rect, rect, SK_ColorBLACK, false);
  }

  RenderPassList pass_list;
  pass_list.push_back(std::move(pass));

  // Allow a difference of 2 bytes in comparison for shader-based transforms,
  // and 4 bytes for LUT-based transforms (determined empirically).
  cc::FuzzyPixelComparator comparator(false, 100.f, 0.f, 2.f, 2, 0);
  if (!transform->CanGetShaderSource())
    comparator = cc::FuzzyPixelComparator(false, 100.f, 0.f, 6.f, 6, 0);
  EXPECT_TRUE(RunPixelTest(&pass_list, &expected_output_colors, comparator));
}

using PrimaryID = gfx::ColorSpace::PrimaryID;
using TransferID = gfx::ColorSpace::TransferID;
using MatrixID = gfx::ColorSpace::MatrixID;
using RangeID = gfx::ColorSpace::RangeID;

gfx::ColorSpace src_color_spaces[] = {
    // This will be replaced by an ICC-based space (which can't be initialized
    // here).
    gfx::ColorSpace(),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT709),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA22),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA24),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA28),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE170M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE240M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LOG),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LOG_SQRT),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_4),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT1361_ECG),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT2020_10),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT2020_12),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTEST2084),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTEST428_1),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::ARIB_STD_B67),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1_HDR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR),
    gfx::ColorSpace(PrimaryID::BT709,
                    TransferID::BT2020_10,
                    MatrixID::BT2020_CL,
                    RangeID::FULL),
};

gfx::ColorSpace dst_color_spaces[] = {
    // This will be replaced by an ICC-based space (which can't be initialized
    // here).
    gfx::ColorSpace(),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT709),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA22),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA24),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::GAMMA28),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE170M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTE240M),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LOG),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LOG_SQRT),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_4),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT1361_ECG),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT2020_10),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::BT2020_12),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::SMPTEST2084),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::ARIB_STD_B67),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::IEC61966_2_1_HDR),
    gfx::ColorSpace(PrimaryID::BT709, TransferID::LINEAR_HDR),
};

gfx::ColorSpace intermediate_color_spaces[] = {
    gfx::ColorSpace(PrimaryID::XYZ_D50, TransferID::LINEAR),
    gfx::ColorSpace(PrimaryID::XYZ_D50, TransferID::IEC61966_2_1_HDR),
};

bool color_space_premul_values[] = {
    true, false,
};

INSTANTIATE_TEST_CASE_P(
    FromColorSpace,
    ColorTransformPixelTest,
    testing::Combine(testing::ValuesIn(src_color_spaces),
                     testing::ValuesIn(intermediate_color_spaces),
                     testing::ValuesIn(color_space_premul_values)));

INSTANTIATE_TEST_CASE_P(
    ToColorSpace,
    ColorTransformPixelTest,
    testing::Combine(testing::ValuesIn(intermediate_color_spaces),
                     testing::ValuesIn(dst_color_spaces),
                     testing::ValuesIn(color_space_premul_values)));

#endif  // !defined(OS_ANDROID)

}  // namespace
}  // namespace viz
