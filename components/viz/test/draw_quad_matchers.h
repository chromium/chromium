// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_
#define COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_

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

// Produces a matcher that matches a SolidColorDrawQuad.
testing::Matcher<const DrawQuad*> IsSolidColorQuad();

// Produces a matcher that matches a SolidColorDrawQuad with |expected_color|.
testing::Matcher<const DrawQuad*> IsSolidColorQuad(SkColor expected_color);

// Produces a matcher that matches a TextureDrawQuad.
testing::Matcher<const DrawQuad*> IsTextureQuad();

// Produces a matcher that matches a YuvVideoDrawQuad.
testing::Matcher<const DrawQuad*> IsYuvVideoQuad();

// Produces a matcher that matches a AggregatedRenderPassQuad.
testing::Matcher<const DrawQuad*> IsAggregatedRenderPassQuad();

}  // namespace viz

#endif  // COMPONENTS_VIZ_TEST_DRAW_QUAD_MATCHERS_H_
