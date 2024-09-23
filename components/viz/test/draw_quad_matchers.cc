// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/draw_quad_matchers.h"

#include "components/viz/common/quads/compositor_render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace viz {
namespace {

const char* MaterialToString(DrawQuad::Material material) {
  switch (material) {
    case DrawQuad::Material::kInvalid:
      return "kInvalid";
    case DrawQuad::Material::kDebugBorder:
      return "kDebugBorder";
    case DrawQuad::Material::kPictureContent:
      return "kPictureContent";
    case DrawQuad::Material::kCompositorRenderPass:
      return "kCompositorRenderPass";
    case DrawQuad::Material::kAggregatedRenderPass:
      return "kAggregatedRenderPass";
    case DrawQuad::Material::kSolidColor:
      return "kSolidColor";
    case DrawQuad::Material::kSharedElement:
      return "kSharedElement";
    case DrawQuad::Material::kSurfaceContent:
      return "kSurfaceContent";
    case DrawQuad::Material::kTextureContent:
      return "kTextureContent";
    case DrawQuad::Material::kTiledContent:
      return "kTiledContent";
    case DrawQuad::Material::kVideoHole:
      return "kVideoHole";
  }
}

// Produces a matcher for a DrawQuad that matches on quad material.
testing::Matcher<const DrawQuad*> IsQuadType(
    DrawQuad::Material expected_material) {
  return testing::Field("material", &DrawQuad::material,
                        testing::Eq(expected_material));
}

testing::Matcher<const DrawQuad*> HasSharedQuadState(
    testing::Matcher<const SharedQuadState*> matcher) {
  return testing::Field("shared_quad_state", &DrawQuad::shared_quad_state,
                        matcher);
}

}  // namespace

void PrintTo(DrawQuad::Material material, ::std::ostream* os) {
  *os << MaterialToString(material);
}

void PrintTo(const OffsetTag& offset_tag, ::std::ostream* os) {
  *os << offset_tag.ToString();
}

testing::Matcher<const DrawQuad*> IsSolidColorQuad() {
  return IsQuadType(DrawQuad::Material::kSolidColor);
}

testing::Matcher<const DrawQuad*> IsSolidColorQuad(SkColor4f expected_color) {
  return testing::AllOf(
      IsSolidColorQuad(),
      testing::Truly([expected_color](const DrawQuad* quad) {
        return SolidColorDrawQuad::MaterialCast(quad)->color == expected_color;
      }));
}

testing::Matcher<const DrawQuad*> IsTextureQuad() {
  return IsQuadType(DrawQuad::Material::kTextureContent);
}

testing::Matcher<const DrawQuad*> IsSurfaceQuad() {
  return IsQuadType(DrawQuad::Material::kSurfaceContent);
}

testing::Matcher<const DrawQuad*> IsCompositorRenderPassQuad(
    CompositorRenderPassId id) {
  return testing::AllOf(
      IsQuadType(DrawQuad::Material::kCompositorRenderPass),
      testing::Truly([id](const DrawQuad* quad) {
        return CompositorRenderPassDrawQuad::MaterialCast(quad)
                   ->render_pass_id == id;
      }));
}

testing::Matcher<const DrawQuad*> IsAggregatedRenderPassQuad() {
  return IsQuadType(DrawQuad::Material::kAggregatedRenderPass);
}

testing::Matcher<const DrawQuad*> HasRect(const gfx::Rect& rect) {
  return testing::Field("rect", &DrawQuad::rect, testing::Eq(rect));
}

testing::Matcher<const DrawQuad*> HasVisibleRect(
    const gfx::Rect& visible_rect) {
  return testing::Field("visible_rect", &DrawQuad::visible_rect,
                        testing::Eq(visible_rect));
}

testing::Matcher<const DrawQuad*> HasTransform(
    const gfx::Transform& transform) {
  return HasSharedQuadState(testing::Field(
      "quad_to_target_transform", &SharedQuadState::quad_to_target_transform,
      testing::Eq(transform)));
}

testing::Matcher<const DrawQuad*> HasOpacity(float opacity) {
  return HasSharedQuadState(testing::Field("opacity", &SharedQuadState::opacity,
                                           testing::Eq(opacity)));
}

testing::Matcher<const DrawQuad*> AreContentsOpaque(bool opaque) {
  return HasSharedQuadState(testing::Field(
      "are_contents_opaque", &SharedQuadState::are_contents_opaque,
      testing::Eq(opaque)));
}

testing::Matcher<const DrawQuad*> HasClipRect(
    std::optional<gfx::Rect> clip_rect) {
  return HasSharedQuadState(testing::Field(
      "clip_rect", &SharedQuadState::clip_rect, testing::Eq(clip_rect)));
}

testing::Matcher<const DrawQuad*> HasOffsetTag(OffsetTag offset_tag) {
  return HasSharedQuadState(testing::Field(
      "offset_tag", &SharedQuadState::offset_tag, testing::Eq(offset_tag)));
}

testing::Matcher<const DrawQuad*> HasLayerId(uint32_t layer_id) {
  return HasSharedQuadState(testing::Field(
      "layer_id", &SharedQuadState::layer_id, testing::Eq(layer_id)));
}

testing::Matcher<const DrawQuad*> HasLayerNamespaceId(
    uint32_t layer_namespace_id) {
  return HasSharedQuadState(testing::Field("layer_namespace_id",
                                           &SharedQuadState::layer_namespace_id,
                                           testing::Eq(layer_namespace_id)));
}

testing::Matcher<const DrawQuad*> HasMaskFilterInfo(
    const gfx::MaskFilterInfo& mask_filter_info) {
  return HasSharedQuadState(testing::Field("mask_filter_info",
                                           &SharedQuadState::mask_filter_info,
                                           testing::Eq(mask_filter_info)));
}

}  // namespace viz
