// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "cc/test/pixel_comparator.h"
#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/service/display/delegated_ink_point_pixel_test_helper.h"
#include "components/viz/service/display/renderer_pixeltest_utils.h"
#include "components/viz/service/display/viz_pixel_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace viz {
namespace {

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

  void TearDown() override {
    DropRenderer();
    VizPixelTestWithParam::TearDown();
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

  bool DrawAndTestTrail(base::FilePath file, int render_pass_id) {
    gfx::Rect rect(this->device_viewport_size_);

    // Minimize the root render pass damage rect so that it has to be expanded
    // by the delegated ink trail damage rect to confirm that it is the right
    // size to remove old trails and add new ones.
    gfx::Rect damage_rect(0, 0, 1, 1);
    AggregatedRenderPassId id{static_cast<uint64_t>(render_pass_id)};
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
        &pass_list, file, cc::AlphaDiscardingFuzzyPixelOffByOneComparator());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         DelegatedInkTest,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());
// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DelegatedInkTest);

class DelegatedInkWithPredictionTest : public DelegatedInkTest {};

INSTANTIATE_TEST_SUITE_P(,
                         DelegatedInkWithPredictionTest,
                         testing::ValuesIn(GetGpuRendererTypes()),
                         testing::PrintToStringParamName());

// GetGpuRendererTypes() can return an empty list, e.g. on Fuchsia ARM64.
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
                        gfx::RectF(0, 0, 175, 172), /*render_pass_id=*/1);
  // Confirm that the trail was drawn. Test three times as
  // the trail will persist for two more frames before being erased.
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_one_trail.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(base::FilePath(FILE_PATH_LITERAL("white.png")),
                               /*render_pass_id=*/1));
}

// Confirm that drawing a second trail completely removes the first trail.
TEST_P(DelegatedInkWithPredictionTest, DrawTwoTrailsAndErase) {
  // Numbers chosen arbitrarily. No points will be predicted, so a trail made of
  // 2 points will be drawn.
  const gfx::PointF kFirstPoint(140, 48);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(115, 85));

  // Provide the metadata required to draw the trail, numbers matching the first
  // DelegatedInkPoint sent.
  CreateAndSendMetadata(kFirstPoint, 8.2f, SkColors::kMagenta, kFirstTimestamp,
                        gfx::RectF(0, 0, 200, 200), /*render_pass_id=*/1);

  // Confirm that the trail was drawn correctly.
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_two_trails_first.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));

  // Now provide new metadata and points to draw a new trail. Just use the last
  // point draw above as the starting point for the new trail. One point will
  // be predicted, so a trail consisting of 4 points will be drawn.
  CreateAndSendMetadataFromLastPoint();
  CreateAndSendPointFromLastPoint(gfx::PointF(134, 100));
  CreateAndSendPointFromLastPoint(gfx::PointF(150, 81.44f));

  // Confirm the first trail is gone and only the second remains.
  base::FilePath expected_result_second =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_two_trails_second.png"));
  if (is_skia_graphite()) {
    expected_result_second =
        expected_result_second.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result_second, /*render_pass_id=*/1));

  // Confirm all trails are gone.
  EXPECT_TRUE(DrawAndTestTrail(base::FilePath(FILE_PATH_LITERAL("white.png")),
                               /*render_pass_id=*/1));
}

// Confirm that the trail can't be drawn beyond the presentation area.
TEST_P(DelegatedInkWithPredictionTest, TrailExtendsBeyondPresentationArea) {
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
                        kPresentationArea, /*render_pass_id=*/1);

  base::FilePath expected_result = base::FilePath(FILE_PATH_LITERAL(
      "delegated_ink_trail_clipped_by_presentation_area.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));
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
      this->child_resource_provider_.get(), this->child_context_provider_,
      pass.get());

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
                        kPresentationArea, /*render_pass_id=*/1);

  base::FilePath expected_result = base::FilePath(
      FILE_PATH_LITERAL("delegated_ink_trail_on_batched_quads.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }

  EXPECT_TRUE(
      this->RunPixelTest(&pass_list, expected_result,
                         cc::AlphaDiscardingFuzzyPixelOffByOneComparator()));
}

// Delegated ink trail is drawn on a non root render pass, with the correct
// transforms.
TEST_P(DelegatedInkWithPredictionTest, SimpleTrailNonRootRenderPass) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;
  constexpr int kTargetPassId = 2;
  AggregatedRenderPassId root_pass_id{1};
  auto root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect, gfx::Rect());

  AggregatedRenderPassId child_pass_id{kTargetPassId};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  transform_to_root.RotateAboutZAxis(10);
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);
  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());
  gfx::Rect yellow_rect = child_pass_local_rect;
  yellow_rect.Offset(30, -60);
  auto* yellow = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  yellow->SetNew(shared_state_without_rrect, yellow_rect, yellow_rect,
                 SkColors::kYellow, false);

  gfx::Rect white_rect = child_pass_local_rect;
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      transform_to_root, pass_rect, root_pass.get(), gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  auto* child_pass_ptr = child_pass.get();

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Simulate the user drawing a horizontal line.
  const gfx::PointF kFirstPoint(156.f, 87.23f);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  CreateAndSendPointFromLastPoint(gfx::PointF(119, 87.23f));
  CreateAndSendPointFromLastPoint(gfx::PointF(75, 87.23f));

  const gfx::RectF kPresentationArea(0, 0, 200, 200);
  CreateAndSendMetadata(kFirstPoint, 19.177f, SkColors::kRed, kFirstTimestamp,
                        kPresentationArea, /*render_pass_id=*/kTargetPassId);

  // Check that the ink trail is drawn on the child render pass. The trail
  // should be slightly diagonal since the pass has been rotated; albeit in the
  // opposite direction. That way when the pass is drawn relative to the root,
  // the trail will appear horizontal.
  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequest(
      &pass_list, child_pass_ptr,
      base::FilePath(
          FILE_PATH_LITERAL("delegated_ink_trail_non_root_render_pass.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          1.0f)));
}

// Delegated ink trail is not drawn when the metadata is outside of the
// render pass area.
TEST_P(DelegatedInkWithPredictionTest, NonIntersectingMetadata) {
  gfx::Rect viewport_rect(this->device_viewport_size_);
  constexpr int kInset = 20;

  AggregatedRenderPassId root_pass_id{1};
  auto root_pass =
      CreateTestRootRenderPass(root_pass_id, viewport_rect, gfx::Rect());

  AggregatedRenderPassId child_pass_id{2};
  gfx::Rect pass_rect(this->device_viewport_size_);
  pass_rect.Inset(kInset);
  gfx::Rect child_pass_local_rect = gfx::Rect(pass_rect.size());
  gfx::Transform transform_to_root;
  transform_to_root.Translate(pass_rect.OffsetFromOrigin());
  auto child_pass = CreateTestRenderPass(child_pass_id, child_pass_local_rect,
                                         transform_to_root);

  SharedQuadState* shared_state_without_rrect =
      CreateTestSharedQuadState(gfx::Transform(), child_pass_local_rect,
                                child_pass.get(), gfx::MaskFilterInfo());

  gfx::Rect white_rect = child_pass_local_rect;
  auto* white = child_pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  white->SetNew(shared_state_without_rrect, white_rect, white_rect,
                SkColors::kWhite, false);

  SharedQuadState* pass_shared_state = CreateTestSharedQuadState(
      transform_to_root, pass_rect, root_pass.get(), gfx::MaskFilterInfo());
  CreateTestRenderPassDrawQuad(pass_shared_state, pass_rect, child_pass_id,
                               root_pass.get());

  auto* child_pass_ptr = child_pass.get();

  AggregatedRenderPassList pass_list;
  pass_list.push_back(std::move(child_pass));
  pass_list.push_back(std::move(root_pass));

  // Simulate the user drawing a diagonal line. First pass is outside child
  // render pass output rect.
  const gfx::PointF kFirstPoint(5, 5);
  const base::TimeTicks kFirstTimestamp = base::TimeTicks::Now();
  CreateAndSendPoint(kFirstPoint, kFirstTimestamp);
  // Subsequent points are inside the child render pass.
  CreateAndSendPointFromLastPoint(gfx::PointF(25, 25));
  CreateAndSendPointFromLastPoint(gfx::PointF(50, 50));

  const gfx::RectF kPresentationArea(0, 0, 200, 200);
  CreateAndSendMetadata(kFirstPoint, 5, SkColors::kRed, kFirstTimestamp,
                        kPresentationArea, /*render_pass_id=*/1);

  // Check that the ink trail is not drawn on the render pass because the
  // delegated ink metadata is outside the bounds of the render pass output
  // rect.
  EXPECT_TRUE(this->RunPixelTestWithCopyOutputRequest(
      &pass_list, child_pass_ptr,
      base::FilePath(FILE_PATH_LITERAL("no-trail-white.png")),
      cc::FuzzyPixelComparator().DiscardAlpha().SetErrorPixelsPercentageLimit(
          1.0f)));
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
                        kPointerId1StartTime, kPresentationArea,
                        /*render_pass_id=*/1);
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_pointer_id_1.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));

  // Then send metadata that matches the first point of the other pointer id.
  // These points should not have been erased, so all 3 points should be drawn.
  CreateAndSendMetadata(kPointerId2StartPoint, 2.4f, SkColors::kRed,
                        kPointerId2StartTime, kPresentationArea,
                        /*render_pass_id=*/1);
  base::FilePath expected_result_second =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_pointer_id_2.png"));
  if (is_skia_graphite()) {
    expected_result_second =
        expected_result_second.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result_second, /*render_pass_id=*/1));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(base::FilePath(FILE_PATH_LITERAL("white.png")),
                               /*render_pass_id=*/1));
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
                        gfx::RectF(0, 0, 175, 172), /*render_pass_id=*/1);
  // Confirm that the trail was drawn. Test three times as
  // the trail will persist for two more frames before being erased.
  base::FilePath expected_result =
      base::FilePath(FILE_PATH_LITERAL("delegated_ink_one_trail.png"));
  if (is_skia_graphite()) {
    expected_result = expected_result.InsertBeforeExtensionASCII(kGraphiteStr);
  }
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));

  // Send metadata again and expect the same trail to be drawn.
  CreateAndSendMetadata(kFirstPoint, 3.5f, SkColors::kBlack, kFirstTimestamp,
                        gfx::RectF(0, 0, 175, 172), /*render_pass_id=*/1);
  EXPECT_TRUE(DrawAndTestTrail(expected_result, /*render_pass_id=*/1));

  // The metadata should have been cleared after drawing, so confirm that there
  // is no trail after another draw.
  EXPECT_TRUE(DrawAndTestTrail(base::FilePath(FILE_PATH_LITERAL("white.png")),
                               /*render_pass_id=*/1));
}

}  // namespace
}  // namespace viz
