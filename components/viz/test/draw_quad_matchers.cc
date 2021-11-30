// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/draw_quad_matchers.h"

#include "components/viz/common/quads/solid_color_draw_quad.h"

namespace viz {
namespace {

// Produces a matcher for a DrawQuad that matches on quad material.
testing::Matcher<const DrawQuad*> IsQuadType(
    DrawQuad::Material expected_material) {
  return testing::Field("material", &DrawQuad::material, expected_material);
}

}  // namespace

testing::Matcher<const DrawQuad*> IsSolidColorQuad() {
  return IsQuadType(DrawQuad::Material::kSolidColor);
}

testing::Matcher<const DrawQuad*> IsSolidColorQuad(SkColor expected_color) {
  return testing::AllOf(
      IsSolidColorQuad(),
      testing::Truly([expected_color](const DrawQuad* quad) {
        return SolidColorDrawQuad::MaterialCast(quad)->color == expected_color;
      }));
}

testing::Matcher<const DrawQuad*> IsTextureQuad() {
  return IsQuadType(DrawQuad::Material::kTextureContent);
}

testing::Matcher<const DrawQuad*> IsYuvVideoQuad() {
  return IsQuadType(DrawQuad::Material::kYuvVideoContent);
}

testing::Matcher<const DrawQuad*> IsAggregatedRenderPassQuad() {
  return IsQuadType(DrawQuad::Material::kAggregatedRenderPass);
}

}  // namespace viz
