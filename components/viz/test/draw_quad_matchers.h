// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_
#define COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_

#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/quads/draw_quad.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkColor.h"

// This file contains gmock matchers for verifying DrawQuads are of the expected
// type with expected attributes. This can be used to verify that a
// CompositorRenderPass or AggregatedRenderPass contains the expected quads.
//
// For example verifying that a render pass contains in front-to-back order, a
// SolidColorDrawQuad which is white, a TextureDrawQuad and finally an
// AggregatedRenderPassDrawQuad would look like:
//
//   EXPECT_THAT(render_pass->quad_list,
//               testing::ElementsAre(IsSolidColorQuad(SK_ColorWHITE),
//                                    IsTextureQuad(),
//                                    IsAggregatedRenderPassQuad()));
//
// Add additional matcher generation functions to this file for DrawQuad types
// and attributes that aren't implemented yet.

namespace viz {

// Provides human readable quad material names for gtest/gmock.
void PrintTo(DrawQuad::Material material, ::std::ostream* os);
void PrintTo(const OffsetTag& offset_tag, ::std::ostream* os);

// Matches a SolidColorDrawQuad.
testing::Matcher<const DrawQuad*> IsSolidColorQuad();

// Matches a SolidColorDrawQuad with |expected_color|.
testing::Matcher<const DrawQuad*> IsSolidColorQuad(SkColor4f expected_color);

// Matches a TextureDrawQuad.
testing::Matcher<const DrawQuad*> IsTextureQuad();

// Matches a SurfaceDrawQuad.
testing::Matcher<const DrawQuad*> IsSurfaceQuad();

// Matches an CompositorRenderPassQuad with matching `id`.
testing::Matcher<const DrawQuad*> IsCompositorRenderPassQuad(
    CompositorRenderPassId id);

// Matches an AggregatedRenderPassQuad.
testing::Matcher<const DrawQuad*> IsAggregatedRenderPassQuad();

// Matches a DrawQuad with expected DrawQuad::rect.
testing::Matcher<const DrawQuad*> HasRect(const gfx::Rect& rect);

// Matches a DrawQuad with expected DrawQuad::visible_rect.
testing::Matcher<const DrawQuad*> HasVisibleRect(const gfx::Rect& visible_rect);

// Matches a DrawQuad with expected SharedQuadState::quad_to_target_transform.
testing::Matcher<const DrawQuad*> HasTransform(const gfx::Transform& transform);

// Matches a DrawQuad with expected SharedQuadState::opacity.
testing::Matcher<const DrawQuad*> HasOpacity(float opacity);

// Matches a DrawQuad with expected SharedQuadState::are_contents_opaque.
testing::Matcher<const DrawQuad*> AreContentsOpaque(bool opaque);

// Matches a DrawQuad with expected SharedQuadState::clip_rect.
testing::Matcher<const DrawQuad*> HasClipRect(
    std::optional<gfx::Rect> clip_rect);

// Matches a DrawQuad with expected SharedQuadState::offset_tag.
testing::Matcher<const DrawQuad*> HasOffsetTag(OffsetTag offset_tag);

// Matches a DrawQuad with expected SharedQuadState::layer_id.
testing::Matcher<const DrawQuad*> HasLayerId(uint32_t layer_id);

// Matches a DrawQuad with expected SharedQuadState::layer_namespace_id.
testing::Matcher<const DrawQuad*> HasLayerNamespaceId(
    uint32_t layer_namespace_id);

// Matches a DrawQuad with expected SharedQuadState::mask_filter_info.
testing::Matcher<const DrawQuad*> HasMaskFilterInfo(
    const gfx::MaskFilterInfo& mask_filter_info);

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_
