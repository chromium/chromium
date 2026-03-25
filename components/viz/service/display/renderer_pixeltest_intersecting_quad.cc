// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_flags.h"
#include "cc/test/fake_raster_source.h"
#include "cc/test/fake_recording_source.h"
#include "cc/test/pixel_comparator.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/renderer_pixeltest_utils.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

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
    back_quad_state_ = CreateTestSharedQuadState(
        trans, viewport_rect_, render_pass_.get(), gfx::MaskFilterInfo());
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
  raw_ptr<SharedQuadState, DanglingUntriaged> front_quad_state_;
  raw_ptr<SharedQuadState, DanglingUntriaged> back_quad_state_;
  gfx::Rect quad_rect_;
  AggregatedRenderPassList pass_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingQuadPixelTest,
                         testing::ValuesIn(GetRendererTypes()),
                         testing::PrintToStringParamName());

class IntersectingQuadSoftwareTest : public IntersectingQuadPixelTest {};

INSTANTIATE_TEST_SUITE_P(,
                         IntersectingQuadSoftwareTest,
                         testing::Values(RendererType::kSoftware),
                         testing::PrintToStringParamName());

TEST_P(IntersectingQuadPixelTest, SolidColorQuads) {
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
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_, SkColors::kGreen,
      SkColors::kBlack, SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());

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
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, true /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());

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
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      true /* flipped_texture_quad */, true /* half_and_half */,
      this->back_quad_state_, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      this->render_pass_.get());

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

  cc::FakeRecordingSource blue_recording(quad_rect_.size());
  blue_recording.add_draw_rect_with_flags(outer_rect, black_flags);
  blue_recording.add_draw_rect_with_flags(inner_rect, blue_flags);
  blue_recording.Rerecord();
  scoped_refptr<cc::RasterSource> blue_raster_source =
      blue_recording.CreateRasterSource();

  auto* blue_quad =
      this->render_pass_->template CreateAndAppendDrawQuad<PictureDrawQuad>();

  blue_quad->SetNew(
      this->front_quad_state_, this->quad_rect_, this->quad_rect_,
      needs_blending, gfx::RectF(this->quad_rect_), false, this->quad_rect_,
      1.f, {}, blue_raster_source->GetDisplayItemList(), cc::ScrollOffsetMap());

  cc::FakeRecordingSource green_recording(quad_rect_.size());
  green_recording.add_draw_rect_with_flags(outer_rect, green_flags);
  green_recording.add_draw_rect_with_flags(inner_rect, black_flags);
  green_recording.Rerecord();
  scoped_refptr<cc::RasterSource> green_raster_source =
      green_recording.CreateRasterSource();

  auto* green_quad =
      this->render_pass_->template CreateAndAppendDrawQuad<PictureDrawQuad>();
  green_quad->SetNew(this->back_quad_state_, this->quad_rect_, this->quad_rect_,
                     needs_blending, gfx::RectF(this->quad_rect_), false,
                     this->quad_rect_, 1.f, {},
                     green_raster_source->GetDisplayItemList(),
                     cc::ScrollOffsetMap());
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
  SharedQuadState* child1_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), this->quad_rect_,
                                child_pass1.get(), gfx::MaskFilterInfo());
  auto child_pass2 =
      CreateTestRenderPass(child_pass_id2, this->quad_rect_, gfx::Transform());
  SharedQuadState* child2_quad_state =
      CreateTestSharedQuadState(gfx::Transform(), this->quad_rect_,
                                child_pass2.get(), gfx::MaskFilterInfo());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 255)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      child1_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      child_pass1.get());
  CreateTestTwoColoredTextureDrawQuad(
      !is_software_renderer(), this->quad_rect_,
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 255, 0)),
      SkColor4f::FromColor(SkColorSetARGB(255, 0, 0, 0)),
      SkColors::kTransparent, true /* premultiplied_alpha */,
      false /* flipped_texture_quad */, false /* half_and_half */,
      child2_quad_state, this->resource_provider_.get(),
      this->child_resource_provider_.get(), this->child_context_provider_,
      child_pass2.get());

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

}  // namespace
}  // namespace viz
