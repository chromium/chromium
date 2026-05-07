// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/renderer_pixeltest_utils.h"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "cc/test/resource_provider_test_utils.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/quads/aggregated_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/texture_draw_quad.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/display/display_resource_provider.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/private/chromium/SkPMColor.h"

namespace viz {

base::span<const uint8_t> MakePixelSpan(const SkBitmap& bitmap) {
  return UNSAFE_TODO(base::span(static_cast<const uint8_t*>(bitmap.getPixels()),
                                bitmap.computeByteSize()));
}

void DeleteSharedImage(scoped_refptr<gpu::ClientSharedImage> shared_image,
                       const gpu::SyncToken& sync_token,
                       bool is_lost) {
  shared_image->UpdateDestructionSyncToken(sync_token);
}

ResourceId CreateGpuResource(
    scoped_refptr<RasterContextProvider> context_provider,
    ClientResourceProvider* resource_provider,
    const gfx::Size& size,
    SharedImageFormat format,
    SkAlphaType alpha_type,
    gfx::ColorSpace color_space,
    base::span<const uint8_t> pixels,
    GrSurfaceOrigin origin) {
  DCHECK(context_provider);
  gpu::SharedImageInterface* sii = context_provider->SharedImageInterface();
  DCHECK(sii);
  auto client_shared_image = sii->CreateSharedImage(
      {format, size, color_space, origin, alpha_type,
       gpu::SHARED_IMAGE_USAGE_DISPLAY_READ, "TestLabel"},
      pixels);

  TransferableResource gl_resource = TransferableResource::Make(
      client_shared_image, TransferableResource::ResourceSource::kTest,
      client_shared_image->creation_sync_token());
  auto release_callback =
      base::BindOnce(&DeleteSharedImage, std::move(client_shared_image));
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

std::unique_ptr<CompositorRenderPass> CreateTestRootRenderPass(
    CompositorRenderPassId id,
    const gfx::Rect& rect) {
  auto pass = CompositorRenderPass::Create(/*shared_quad_state_list_size=*/1u,
                                           /*quad_list_size*/ 1u);
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
    RenderPassInternal* render_pass,
    const gfx::MaskFilterInfo& mask_filter_info) {
  const gfx::Rect layer_rect = rect;
  const gfx::Rect visible_layer_rect = rect;
  const bool are_contents_opaque = false;
  const float opacity = 1.0f;
  const SkBlendMode blend_mode = SkBlendMode::kSrcOver;
  int sorting_context_id = 0;
  SharedQuadState* shared_state = render_pass->CreateAndAppendSharedQuadState();
  shared_state->SetAll(quad_to_target_transform, layer_rect, visible_layer_rect,
                       mask_filter_info, /**clip_rect=*/std::nullopt,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id,
                       /*layer_id=*/0u, /*fast_rounded_corner=*/false);
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
                       /*filter_info=*/gfx::MaskFilterInfo(), clip_rect,
                       are_contents_opaque, opacity, blend_mode,
                       sorting_context_id, /*layer_id=*/0u,
                       /*fast_rounded_corner=*/false);

  return shared_state;
}

void CreateTestRenderPassDrawQuad(const SharedQuadState* shared_state,
                                  const gfx::Rect& rect,
                                  AggregatedRenderPassId pass_id,
                                  AggregatedRenderPass* render_pass,
                                  cc::FilterOperations filters) {
  auto* quad =
      render_pass->CreateAndAppendDrawQuad<AggregatedRenderPassDrawQuad>();
  quad->SetNew(shared_state, rect, rect, pass_id,
               kInvalidResourceId,        // mask_resource_id
               gfx::RectF(),              // mask_uv_rect
               gfx::Size(),               // mask_texture_size
               false);                    // force_anti_aliasing_off
  quad->SetFilters(filters, {},           // backdrop_filters
                   std::nullopt,          // backdrop_filter_bounds
                   gfx::Vector2dF(1, 1),  // filters scale
                   gfx::PointF(),         // filter origin
                   1.0f);                 // backdrop_filter_quality
}

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
    scoped_refptr<RasterContextProvider> child_context_provider,
    AggregatedRenderPass* render_pass) {
  // As this function renders to an RGBA_8888 texture, it makes sense to use
  // integer colors
  SkPMColor pixel_color_one =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color_one.toSkColor())
          : SkPMColorSetARGB(255 * texel_color_one.fA, 255 * texel_color_one.fR,
                             255 * texel_color_one.fG,
                             255 * texel_color_one.fB);
  SkPMColor pixel_color_two =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color_two.toSkColor())
          : SkPMColorSetARGB(255 * texel_color_two.fA, 255 * texel_color_two.fR,
                             255 * texel_color_two.fG,
                             255 * texel_color_two.fB);
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

  const GrSurfaceOrigin origin = flipped_texture_quad
                                     ? kBottomLeft_GrSurfaceOrigin
                                     : kTopLeft_GrSurfaceOrigin;
  const SkAlphaType alpha_type =
      premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;

  ResourceId resource;
  if (gpu_resource) {
    resource = CreateGpuResource(
        child_context_provider, child_resource_provider, rect.size(),
        SinglePlaneFormat::kBGRA_8888, alpha_type, gfx::ColorSpace(),
        base::as_byte_span(pixels), origin);
  } else {
    auto shared_image =
        child_context_provider->SharedImageInterface()
            ->CreateSharedImageForSoftwareCompositor(
                {SinglePlaneFormat::kBGRA_8888, rect.size(), gfx::ColorSpace(),
                 origin, alpha_type, gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY,
                 "PixelTest"});

    auto mapping = shared_image->Map();

    auto transferable_resource = TransferableResource::Make(
        shared_image, TransferableResource::ResourceSource::kTileRasterTask,
        shared_image->creation_sync_token());
    auto release_callback =
        base::BindOnce(&DeleteSharedImage, std::move(shared_image));

    resource = child_resource_provider->ImportResource(
        std::move(transferable_resource), std::move(release_callback));

    uint32_t* ptr =
        reinterpret_cast<uint32_t*>(mapping->GetMemoryForPlane(0).data());
    base::span<uint32_t> span = UNSAFE_BUFFERS(base::span(ptr, pixels.size()));

    std::ranges::copy(pixels, span.begin());
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, resource_provider, child_resource_provider,
          child_context_provider->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  bool needs_blending = true;
  const gfx::PointF tex_coord_top_left(0.0f, 0.0f);
  const gfx::PointF tex_coord_bottom_right(rect.width(), rect.height());
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
               tex_coord_top_left, tex_coord_bottom_right, background_color,
               nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);
}

void CreateTestTextureDrawQuad(
    bool gpu_resource,
    const gfx::Rect& rect,
    SkColor4f texel_color,
    SkColor4f background_color,
    bool premultiplied_alpha,
    const SharedQuadState* shared_state,
    DisplayResourceProvider* resource_provider,
    ClientResourceProvider* child_resource_provider,
    scoped_refptr<RasterContextProvider> child_context_provider,
    AggregatedRenderPass* render_pass) {
  // As this function renders to an RGBA_8888 texture, it makes sense to use
  // integer colors
  SkPMColor pixel_color =
      premultiplied_alpha
          ? SkPreMultiplyColor(texel_color.toSkColor())
          : SkPMColorSetARGB(texel_color.fA * 255, texel_color.fR * 255,
                             texel_color.fG * 255, texel_color.fB * 255);
  size_t num_pixels = static_cast<size_t>(rect.width()) * rect.height();
  std::vector<uint32_t> pixels(num_pixels, pixel_color);

  const SkAlphaType alpha_type =
      premultiplied_alpha ? kPremul_SkAlphaType : kUnpremul_SkAlphaType;
  ResourceId resource;
  if (gpu_resource) {
    resource = CreateGpuResource(child_context_provider,
                                 child_resource_provider, rect.size(),
                                 SinglePlaneFormat::kRGBA_8888, alpha_type,
                                 gfx::ColorSpace(), base::as_byte_span(pixels));
  } else {
    auto shared_image =
        child_context_provider->SharedImageInterface()
            ->CreateSharedImageForSoftwareCompositor(
                {SinglePlaneFormat::kBGRA_8888, rect.size(), gfx::ColorSpace(),
                 kTopLeft_GrSurfaceOrigin, alpha_type,
                 gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY, "PixelTest"});
    auto mapping = shared_image->Map();

    auto transferable_resource = TransferableResource::Make(
        shared_image, TransferableResource::ResourceSource::kTileRasterTask,
        shared_image->creation_sync_token());
    auto release_callback =
        base::BindOnce(&DeleteSharedImage, std::move(shared_image));

    resource = child_resource_provider->ImportResource(
        std::move(transferable_resource), std::move(release_callback));

    uint32_t* ptr =
        reinterpret_cast<uint32_t*>(mapping->GetMemoryForPlane(0).data());
    base::span<uint32_t> span = UNSAFE_BUFFERS(base::span(ptr, pixels.size()));

    std::ranges::copy(pixels, span.begin());
  }

  // Return the mapped resource id.
  std::unordered_map<ResourceId, ResourceId, ResourceIdHasher> resource_map =
      cc::SendResourceAndGetChildToParentMap(
          {resource}, resource_provider, child_resource_provider,
          child_context_provider->SharedImageInterface());
  ResourceId mapped_resource = resource_map[resource];

  bool needs_blending = true;
  const gfx::PointF tex_coord_top_left(0.0f, 0.0f);
  const gfx::PointF tex_coord_bottom_right(rect.width(), rect.height());
  const bool nearest_neighbor = false;
  auto* quad = render_pass->CreateAndAppendDrawQuad<TextureDrawQuad>();
  quad->SetNew(shared_state, rect, rect, needs_blending, mapped_resource,
               tex_coord_top_left, tex_coord_bottom_right, background_color,
               nearest_neighbor,
               /*secure_output=*/false, gfx::ProtectedVideoType::kClear,
               /*is_tex_coords_normalized=*/false);
}

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
  SharedQuadState* front_shared_state = CreateTestSharedQuadState(
      front_quad_to_target_transform, rect, pass, gfx::MaskFilterInfo());

  auto* front = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  front->SetAll(front_shared_state, rect, rect, needs_blending, front_color,
                force_aa_off);

  gfx::Transform back_quad_to_target_transform;
  back_quad_to_target_transform.Translate(25.5f, 25.5f);
  back_quad_to_target_transform.Scale(0.5f, 0.5f);
  SharedQuadState* back_shared_state = CreateTestSharedQuadState(
      back_quad_to_target_transform, rect, pass, gfx::MaskFilterInfo());

  auto* back = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  back->SetAll(back_shared_state, rect, rect, needs_blending, back_color,
               force_aa_off);
}

}  // namespace viz
