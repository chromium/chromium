// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/compositor_frame.h"

#include "base/unguessable_token.h"
#include "cc/paint/filter_operation.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/test/compositor_frame_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

constexpr gfx::Rect kOutputRect(0, 0, 100, 100);
constexpr gfx::Rect kDamageRect(0, 0, 100, 100);
constexpr gfx::Rect kSmallRect(0, 0, 10, 10);

SurfaceId MakeSurfaceId() {
  return SurfaceId(
      FrameSinkId(1, 1),
      LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(1u, 2u)));
}

// A frame with no render passes at all is empty.
TEST(CompositorFrameTest, NoRenderPassesIsEmpty) {
  CompositorFrame frame = CompositorFrameBuilder().Build();
  EXPECT_FALSE(frame.HasVisuallyNonEmptyContent());
}

// A single render pass with no quads is empty. This is what a client produces
// before it has painted anything.
TEST(CompositorFrameTest, EmptyRootRenderPassIsEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  EXPECT_FALSE(frame.HasVisuallyNonEmptyContent());
}

// A single solid color quad is the default background fill, so it is empty.
TEST(CompositorFrameTest, SingleSolidColorQuadIsEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kWhite))
          .Build();
  EXPECT_FALSE(frame.HasVisuallyNonEmptyContent());
}

// The color of the single solid color quad does not matter; a client may paint
// a non-white background (e.g. in dark mode) without having painted content.
TEST(CompositorFrameTest, SingleNonWhiteSolidColorQuadIsEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kBlack))
          .Build();
  EXPECT_FALSE(frame.HasVisuallyNonEmptyContent());
}

// The single solid color quad is not required to cover the whole output_rect.
// We deliberately do not check coverage or clipping, since a partial background
// fill still cannot be used to spoof page content, and strict rect checks would
// risk false positives from viewport sizing and scale rounding quirks. Note
// that the frame must still damage its whole output_rect, which is what keeps
// such quads from being combined across frames.
TEST(CompositorFrameTest, SinglePartialSolidColorQuadWithFullDamageIsEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kSmallRect, SkColors::kRed))
          .Build();
  EXPECT_FALSE(frame.HasVisuallyNonEmptyContent());
}

// A frame that damages less than its output_rect leaves the pixels outside the
// damage on screen from earlier frames, so a client could paint arbitrary
// content one rect per frame while every frame looks like a background fill.
TEST(CompositorFrameTest, PartialDamageIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kWhite)
                             .SetDamageRect(kSmallRect))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// The same applies without any quads, since partial damage lets a client erase
// rects back to the embedder's background color, which draws just as well as
// painting them.
TEST(CompositorFrameTest, EmptyRootPassWithPartialDamageIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kSmallRect).Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// Two solid color quads can be combined into recognizable content, so they are
// non-empty. Only a single solid color quad is treated as a background fill.
TEST(CompositorFrameTest, TwoSolidColorQuadsAreNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kWhite)
                             .AddSolidColorQuad(kSmallRect, SkColors::kRed))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// Any single quad that is not a solid color quad is non-empty, even on its own.
// This is what keeps quad types that can draw arbitrary content (textures,
// tiles, pictures, and debug borders) from being mistaken for a background.
TEST(CompositorFrameTest, SingleTextureQuadIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kOutputRect)
                  .AddTextureQuad(kOutputRect, ResourceId(1),
                                  TextureQuadParams{.needs_blending = false}))
          .PopulateResources()
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// A surface quad embeds another client (e.g. an OOPIF, an OffscreenCanvas or a
// video). This is what prevents a client from hiding content behind a nested
// frame sink.
TEST(CompositorFrameTest, SingleSurfaceQuadIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kOutputRect)
                  .AddSurfaceQuad(kOutputRect, SurfaceRange(MakeSurfaceId())))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// More than one render pass implies intermediate passes (effects, filters,
// masks), which only exist when there is content to apply them to.
TEST(CompositorFrameTest, MultipleRenderPassesAreNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{2}, kOutputRect)
                  .AddSolidColorQuad(kOutputRect, SkColors::kWhite))
          .AddRenderPass(
              RenderPassBuilder(CompositorRenderPassId{1}, kOutputRect)
                  .AddRenderPassQuad(kOutputRect, CompositorRenderPassId{2}))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// The single-quad exemption only applies when there is exactly one render pass,
// so an empty root pass alongside another pass is still non-empty.
TEST(CompositorFrameTest, EmptyRootPassWithSecondPassIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kWhite))
          .AddRenderPass(kOutputRect, kDamageRect)
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// A filter on the root pass turns a single background quad into arbitrary
// content, so it is non-empty despite the single solid color quad. An alpha
// threshold filter masks the pass to an arbitrary set of rects, which is enough
// to draw recognizable shapes or text out of one opaque quad.
TEST(CompositorFrameTest, SingleSolidColorQuadWithFilterIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kOutputRect)
                  .AddSolidColorQuad(kOutputRect, SkColors::kBlack)
                  .AddFilter(cc::FilterOperation::CreateOpacityFilter(0.5f)))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// The same, for the alpha threshold filter specifically, since its region is
// deserialized from the client and can be an arbitrary union of rects.
TEST(CompositorFrameTest, SingleSolidColorQuadWithAlphaThresholdIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(
              RenderPassBuilder(kOutputRect)
                  .AddSolidColorQuad(kOutputRect, SkColors::kBlack)
                  .AddFilter(cc::FilterOperation::CreateAlphaThresholdFilter(
                      cc::FilterOperation::ShapeRects{kSmallRect})))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// Backdrop filters sample and transform what is already on screen behind the
// pass, so they can also produce visible content from a single quad.
TEST(CompositorFrameTest, SingleSolidColorQuadWithBackdropFilterIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder()
          .AddRenderPass(RenderPassBuilder(kOutputRect)
                             .AddSolidColorQuad(kOutputRect, SkColors::kWhite)
                             .AddBackdropFilter(
                                 cc::FilterOperation::CreateInvertFilter(1.0f)))
          .Build();
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

// A filter with no quads to apply it to still counts as non-empty, rather than
// relying on the empty quad list alone to decide.
TEST(CompositorFrameTest, EmptyRootPassWithFilterIsNonEmpty) {
  CompositorFrame frame =
      CompositorFrameBuilder().AddRenderPass(kOutputRect, kDamageRect).Build();
  // Set the filter directly, because RenderPassBuilder requires a quad.
  frame.render_pass_list.back()->filters.Append(
      cc::FilterOperation::CreateInvertFilter(1.0f));
  EXPECT_TRUE(frame.HasVisuallyNonEmptyContent());
}

}  // namespace
}  // namespace viz
