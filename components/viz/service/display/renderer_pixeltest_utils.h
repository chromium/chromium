// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_PIXELTEST_UTILS_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_PIXELTEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/filter_operations.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/skia/include/core/SkAlphaType.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/gpu/ganesh/GrTypes.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace gpu {
class ClientSharedImage;
}

class SkBitmap;

namespace viz {

class ClientResourceProvider;
class DisplayResourceProvider;
class RenderPassInternal;
class SharedQuadState;

base::span<const uint8_t> MakePixelSpan(const SkBitmap& bitmap);

void DeleteSharedImage(scoped_refptr<gpu::ClientSharedImage> shared_image,
                       const gpu::SyncToken& sync_token,
                       bool is_lost);

ResourceId CreateGpuResource(
    scoped_refptr<RasterContextProvider> context_provider,
    ClientResourceProvider* resource_provider,
    const gfx::Size& size,
    SharedImageFormat format,
    SkAlphaType alpha_type,
    gfx::ColorSpace color_space,
    base::span<const uint8_t> pixels,
    GrSurfaceOrigin origin = kTopLeft_GrSurfaceOrigin);

std::unique_ptr<AggregatedRenderPass> CreateTestRootRenderPass(
    AggregatedRenderPassId id,
    const gfx::Rect& rect);

std::unique_ptr<CompositorRenderPass> CreateTestRootRenderPass(
    CompositorRenderPassId id,
    const gfx::Rect& rect);

std::unique_ptr<AggregatedRenderPass> CreateTestRenderPass(
    AggregatedRenderPassId id,
    const gfx::Rect& rect,
    const gfx::Transform& transform_to_root_target);

SharedQuadState* CreateTestSharedQuadState(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    RenderPassInternal* render_pass,
    const gfx::MaskFilterInfo& mask_filter_info);

SharedQuadState* CreateTestSharedQuadStateClipped(
    gfx::Transform quad_to_target_transform,
    const gfx::Rect& rect,
    const gfx::Rect& clip_rect,
    AggregatedRenderPass* render_pass);

void CreateTestRenderPassDrawQuad(const SharedQuadState* shared_state,
                                  const gfx::Rect& rect,
                                  AggregatedRenderPassId pass_id,
                                  AggregatedRenderPass* render_pass,
                                  cc::FilterOperations filters = {});

// Create a TextureDrawDrawQuad with two given colors.
// flipped_texture_quad: The TextureDrawDrawQuad is y flipped.
// half_and_half: if true, the upper half part of the texture is filled with
//   texel_color_one, other part of the texture is filled with texel_color_two.
//   if false, a 1/2 width and height rectangle in the middle of the quad will
//   be filled with texel_color_two, other part of the texture is filled with
//   texel_color_one,
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
    AggregatedRenderPass* render_pass);

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
    AggregatedRenderPass* render_pass);

// Create two quads of specified colors on half-pixel boundaries.
void CreateTestAxisAlignedQuads(const gfx::Rect& rect,
                                SkColor4f front_color,
                                SkColor4f back_color,
                                bool needs_blending,
                                bool force_aa_off,
                                AggregatedRenderPass* pass);

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_RENDERER_PIXELTEST_UTILS_H_
