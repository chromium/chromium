// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"

namespace viz {
namespace {

class LayerContextImplLayerTreePropertiesTest : public LayerContextImplTest {};

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateBackgroundColor) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update with default color.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->background_color(), kDefaultBackgroundColor);

  // Update to a new color.
  const SkColor4f kColor1 = SkColors::kRed;
  auto update2 = CreateDefaultUpdate();
  update2->background_color = kColor1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->background_color(), kColor1);

  // Update to another color.
  const SkColor4f kColor2 = SkColors::kBlue;
  auto update3 = CreateDefaultUpdate();
  update3->background_color = kColor2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->background_color(), kColor2);

  // Update back to default (transparent).
  auto update4 = CreateDefaultUpdate();
  update4->background_color = kDefaultBackgroundColor;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->background_color(), kDefaultBackgroundColor);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateSourceFrameNumber) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->source_frame_number(), 1);

  // Update to a new number.
  auto update2 = CreateDefaultUpdate();
  update2->source_frame_number = 10;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->source_frame_number(), 10);

  // Update to 0.
  auto update3 = CreateDefaultUpdate();
  update3->source_frame_number = 0;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->source_frame_number(), 0);

  // Update to a large number.
  auto update4 = CreateDefaultUpdate();
  update4->source_frame_number = std::numeric_limits<int32_t>::max();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->source_frame_number(),
            std::numeric_limits<int32_t>::max());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateTraceId) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->trace_id(),
            cc::BeginMainFrameTraceId::FromUnsafeValue(1));

  // Update to a new number.
  auto update2 = CreateDefaultUpdate();
  update2->trace_id = 20;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->trace_id(),
            cc::BeginMainFrameTraceId::FromUnsafeValue(20));

  // Update to 0.
  auto update3 = CreateDefaultUpdate();
  update3->trace_id = 0;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->trace_id(),
            cc::BeginMainFrameTraceId::FromUnsafeValue(0));

  // Update to a large number.
  auto update4 = CreateDefaultUpdate();
  update4->trace_id = std::numeric_limits<int64_t>::max();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->trace_id(), cc::BeginMainFrameTraceId::FromUnsafeValue(
                                         std::numeric_limits<int64_t>::max()));
}

TEST_F(LayerContextImplLayerTreePropertiesTest,
       UpdatePrimaryMainFrameItemSequenceNumber) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->primary_main_frame_item_sequence_number(), 1);

  // Update to a new number.
  auto update2 = CreateDefaultUpdate();
  update2->primary_main_frame_item_sequence_number = 30;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->primary_main_frame_item_sequence_number(), 30);

  // Update to 0.
  auto update3 = CreateDefaultUpdate();
  update3->primary_main_frame_item_sequence_number = 0;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->primary_main_frame_item_sequence_number(), 0);

  // Update to a large number.
  auto update4 = CreateDefaultUpdate();
  update4->primary_main_frame_item_sequence_number =
      std::numeric_limits<int64_t>::max();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->primary_main_frame_item_sequence_number(),
            std::numeric_limits<int64_t>::max());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateDeviceViewport) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->GetDeviceViewport(), kDefaultDeviceViewportRect);

  // Update to a new rect.
  const gfx::Rect kRect1(10, 20, 30, 40);
  auto update2 = CreateDefaultUpdate();
  update2->device_viewport = kRect1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->GetDeviceViewport(), kRect1);

  // Update to an empty rect.
  const gfx::Rect kEmptyRect;
  auto update3 = CreateDefaultUpdate();
  update3->device_viewport = kEmptyRect;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->GetDeviceViewport(), kEmptyRect);

  // Update to a large rect.
  const gfx::Rect kLargeRect(0, 0, 10000, 10000);
  auto update4 = CreateDefaultUpdate();
  update4->device_viewport = kLargeRect;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->GetDeviceViewport(), kLargeRect);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateViewportDamageRect) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  // Default viewport_damage_rect is initially set to the entire viewport
  EXPECT_EQ(host_impl->viewport_damage_rect_for_testing(),
            kDefaultDeviceViewportRect);

  // Update to a new rect.
  const gfx::Rect kDamageRect1(5, 10, 15, 20);
  auto update2 = CreateDefaultUpdate();
  update2->viewport_damage_rect = kDamageRect1;
  host_impl->ResetViewportDamageRectForTesting();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(host_impl->viewport_damage_rect_for_testing(), kDamageRect1);

  // Update to an empty rect again.
  auto update3 = CreateDefaultUpdate();
  update3->viewport_damage_rect = gfx::Rect();
  host_impl->ResetViewportDamageRectForTesting();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(host_impl->viewport_damage_rect_for_testing(), gfx::Rect());

  // Update to a very large rect.
  const gfx::Rect kLargeDamageRect(0, 0, 10000, 10000);
  auto update4 = CreateDefaultUpdate();
  update4->viewport_damage_rect = kLargeDamageRect;
  host_impl->ResetViewportDamageRectForTesting();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(host_impl->viewport_damage_rect_for_testing(), kLargeDamageRect);

  // Update to a rect with negative origin (gfx::Rect normalizes this).
  const gfx::Rect kNegativeOriginRect(-10, -5, 20, 15);
  auto update5 = CreateDefaultUpdate();
  update5->viewport_damage_rect = kNegativeOriginRect;
  host_impl->ResetViewportDamageRectForTesting();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  EXPECT_EQ(host_impl->viewport_damage_rect_for_testing(), kNegativeOriginRect);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateDisplayColorSpaces) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  // Default is gfx::DisplayColorSpaces() as per CreateDefaultUpdate.
  EXPECT_EQ(active_tree->display_color_spaces(), gfx::DisplayColorSpaces());

  // Update to new color spaces.
  gfx::DisplayColorSpaces color_spaces1(gfx::ColorSpace::CreateSRGB(),
                                        SinglePlaneFormat::kRGBA_8888);
  auto update2 = CreateDefaultUpdate();
  update2->display_color_spaces = color_spaces1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->display_color_spaces(), color_spaces1);

  // Update back to default.
  auto update3 = CreateDefaultUpdate();
  update3->display_color_spaces = gfx::DisplayColorSpaces();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->display_color_spaces(), gfx::DisplayColorSpaces());

  // Update to different color spaces (e.g., P3).
  gfx::DisplayColorSpaces color_spaces2(gfx::ColorSpace::CreateDisplayP3D65(),
                                        SinglePlaneFormat::kBGRA_8888);
  auto update4 = CreateDefaultUpdate();
  update4->display_color_spaces = color_spaces2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->display_color_spaces(), color_spaces2);

  // Update with HDR color space.
  gfx::ColorSpace hdr_color_space_object(
      gfx::ColorSpace::PrimaryID::BT2020, gfx::ColorSpace::TransferID::PQ,
      gfx::ColorSpace::MatrixID::RGB, gfx::ColorSpace::RangeID::FULL);
  gfx::DisplayColorSpaces color_spaces_hdr(hdr_color_space_object,
                                           SinglePlaneFormat::kRGBA_F16);
  auto update5 = CreateDefaultUpdate();
  update5->display_color_spaces = color_spaces_hdr;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  EXPECT_EQ(active_tree->display_color_spaces(), color_spaces_hdr);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateLocalSurfaceId) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();
  cc::LayerTreeImpl* active_tree = host_impl->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  // Default is kDefaultLocalSurfaceId as per CreateDefaultUpdate.
  EXPECT_EQ(active_tree->local_surface_id_from_parent(),
            kDefaultLocalSurfaceId);
  EXPECT_EQ(host_impl->GetCurrentLocalSurfaceId(), kDefaultLocalSurfaceId);

  // Update to a new LocalSurfaceId.
  const LocalSurfaceId kNewLsi0(
      4, base::UnguessableToken::CreateForTesting(5u, 6u));
  const LocalSurfaceId kNewLsi1(
      7, base::UnguessableToken::CreateForTesting(8u, 9u));
  auto update2 = CreateDefaultUpdate();
  update2->local_surface_id_from_parent = kNewLsi0;
  update2->current_local_surface_id = kNewLsi1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->local_surface_id_from_parent(), kNewLsi0);
  EXPECT_EQ(host_impl->GetCurrentLocalSurfaceId(), kNewLsi1);

  // Update back to default.
  auto update_default_lsi = CreateDefaultUpdate();
  update_default_lsi->local_surface_id_from_parent = kDefaultLocalSurfaceId;
  update_default_lsi->current_local_surface_id = kDefaultLocalSurfaceId;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_default_lsi))
          .has_value());
  EXPECT_EQ(active_tree->local_surface_id_from_parent(),
            kDefaultLocalSurfaceId);
  EXPECT_EQ(host_impl->GetCurrentLocalSurfaceId(), kDefaultLocalSurfaceId);

  // Update to an invalid LocalSurfaceId (default constructed).
  // LayerTreeImpl stores it as is.
  const LocalSurfaceId kInvalidLsi;
  auto update_invalid_lsi = CreateDefaultUpdate();
  update_invalid_lsi->local_surface_id_from_parent = kInvalidLsi;
  update_invalid_lsi->current_local_surface_id = kInvalidLsi;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_invalid_lsi))
          .has_value());
  EXPECT_EQ(active_tree->local_surface_id_from_parent(), kInvalidLsi);
  EXPECT_EQ(host_impl->GetCurrentLocalSurfaceId(), kInvalidLsi);

  // Update with a different valid LocalSurfaceId.
  const LocalSurfaceId kAnotherValidLsi0(
      kDefaultLocalSurfaceId.parent_sequence_number() + 1,
      kDefaultLocalSurfaceId.child_sequence_number() + 1,
      base::UnguessableToken::CreateForTesting(10u, 11u));
  const LocalSurfaceId kAnotherValidLsi1(
      kDefaultLocalSurfaceId.parent_sequence_number() + 2,
      kDefaultLocalSurfaceId.child_sequence_number() + 2,
      base::UnguessableToken::CreateForTesting(12u, 13u));
  auto update_another_lsi = CreateDefaultUpdate();
  update_another_lsi->local_surface_id_from_parent = kAnotherValidLsi0;
  update_another_lsi->current_local_surface_id = kAnotherValidLsi1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_another_lsi))
          .has_value());
  EXPECT_EQ(active_tree->local_surface_id_from_parent(), kAnotherValidLsi0);
  EXPECT_EQ(host_impl->GetCurrentLocalSurfaceId(), kAnotherValidLsi1);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateBeginFrameArgs) {
  // begin_frame_args are not directly stored on LayerTreeImpl but are used
  // during the DoDraw call. This test primarily ensures that sending
  // different BeginFrameArgs doesn't cause crashes and that the update itself
  // is processed.

  // Initial update with default args.
  auto update1 = CreateDefaultUpdate();  // Uses default BeginFrameArgs
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  // Update with modified BeginFrameArgs.
  auto update2 = CreateDefaultUpdate();
  update2->begin_frame_args.frame_id.source_id++;
  update2->begin_frame_args.frame_id.sequence_number += 100;
  update2->begin_frame_args.frame_time += base::Milliseconds(10);
  update2->begin_frame_args.deadline += base::Milliseconds(5);
  update2->begin_frame_args.interval = base::Milliseconds(8);
  update2->begin_frame_args.type = BeginFrameArgs::MISSED;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  // Update with zero interval.
  auto update3 = CreateDefaultUpdate();
  update3->begin_frame_args.interval = base::TimeDelta();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());

  // Update with frame_time == deadline.
  auto update4 = CreateDefaultUpdate();
  update4->begin_frame_args.deadline = update4->begin_frame_args.frame_time;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateDisplayTransformHint) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const gfx::OverlayTransform kDefaultTransform =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE;

  // Initial update with default transform.
  auto update1 = CreateDefaultUpdate();
  update1->display_transform_hint = kDefaultTransform;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->display_transform_hint(), kDefaultTransform);
  // The first update will need to update draw properties due to other
  // unrelated properties being set for the first time.
  EXPECT_TRUE(active_tree->needs_update_draw_properties());
  active_tree->clear_needs_update_draw_properties_for_testing();

  // Update to a new transform.
  const gfx::OverlayTransform kTransform2 =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90;
  auto update2 = CreateDefaultUpdate();
  update2->display_transform_hint = kTransform2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->display_transform_hint(), kTransform2);
  // Updating the display transform hint does not affect property trees, so
  // MoveChangeTrackingToLayers is not called and draw properties do not need
  // an update.
  EXPECT_FALSE(active_tree->needs_update_draw_properties());

  // Update to another transform.
  const gfx::OverlayTransform kTransform3 =
      gfx::OverlayTransform::OVERLAY_TRANSFORM_FLIP_VERTICAL;
  auto update3 = CreateDefaultUpdate();
  update3->display_transform_hint = kTransform3;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->display_transform_hint(), kTransform3);
  // Updating the display transform hint does not affect property trees, so
  // MoveChangeTrackingToLayers is not called and draw properties do not need
  // an update.
  EXPECT_FALSE(active_tree->needs_update_draw_properties());

  // Note: No need to test invalid enum values as mojom handles that.
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateMaxSafeAreaInsetBottom) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const float kDefaultInset = 0.f;

  // Initial update with default inset.
  auto update1 = CreateDefaultUpdate();
  update1->max_safe_area_inset_bottom = kDefaultInset;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->max_safe_area_inset_bottom(), kDefaultInset);
  // The first update will need to update draw properties due to other
  // unrelated properties being set for the first time.
  EXPECT_TRUE(active_tree->needs_update_draw_properties());
  active_tree->clear_needs_update_draw_properties_for_testing();

  // Update to a new non-zero inset.
  const float kInset1 = 50.f;
  auto update2 = CreateDefaultUpdate();
  update2->max_safe_area_inset_bottom = kInset1;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->max_safe_area_inset_bottom(), kInset1);
  // Updating the max safe area inset does not affect property trees, so
  // MoveChangeTrackingToLayers is not called and draw properties do not need
  // an update.
  EXPECT_FALSE(active_tree->needs_update_draw_properties());

  // Update to a different non-zero inset (e.g. smaller).
  const float kInset2 = 20.f;
  auto update3 = CreateDefaultUpdate();
  update3->max_safe_area_inset_bottom = kInset2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->max_safe_area_inset_bottom(), kInset2);
  // Updating the max safe area inset does not affect property trees, so
  // MoveChangeTrackingToLayers is not called and draw properties do not need
  // an update.
  EXPECT_FALSE(active_tree->needs_update_draw_properties());
}
TEST_F(LayerContextImplLayerTreePropertiesTest,
       InvalidMaxSafeAreaInsetBottomFails) {
  // Create base update to test error handling
  auto update = CreateDefaultUpdate();

  // Negative value
  update->max_safe_area_inset_bottom = -10.f;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid max safe area inset bottom");

  update = CreateDefaultUpdate();
  update->max_safe_area_inset_bottom = std::numeric_limits<float>::infinity();
  result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_EQ(result.error(), "Invalid max safe area inset bottom");
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateBrowserControlsParams) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  cc::BrowserControlsParams kDefaultParams;

  // Initial update with default params.
  auto update1 = CreateDefaultUpdate();
  update1->browser_controls_params = kDefaultParams;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->browser_controls_params(), kDefaultParams);

  // Update to new params.
  cc::BrowserControlsParams params2;
  params2.top_controls_height = 50.f;
  params2.top_controls_min_height = 10.f;
  params2.bottom_controls_height = 30.f;
  params2.bottom_controls_min_height = 5.f;
  params2.animate_browser_controls_height_changes = true;
  params2.browser_controls_shrink_blink_size = true;
  params2.only_expand_top_controls_at_page_top = true;

  auto update2 = CreateDefaultUpdate();
  update2->browser_controls_params = params2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->browser_controls_params(), params2);

  // Update to different params.
  cc::BrowserControlsParams params3;
  params3.top_controls_height = 60.f;
  params3.top_controls_min_height = 0.f;
  params3.bottom_controls_height = 0.f;
  params3.bottom_controls_min_height = 0.f;
  params3.animate_browser_controls_height_changes = false;
  params3.browser_controls_shrink_blink_size = false;
  params3.only_expand_top_controls_at_page_top = false;

  auto update3 = CreateDefaultUpdate();
  update3->browser_controls_params = params3;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->browser_controls_params(), params3);

  // Update back to default params.
  auto update4 = CreateDefaultUpdate();
  update4->browser_controls_params = kDefaultParams;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->browser_controls_params(), kDefaultParams);

  // Update with no change.
  auto update5 = CreateDefaultUpdate();
  update5->browser_controls_params = kDefaultParams;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  EXPECT_EQ(active_tree->browser_controls_params(), kDefaultParams);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateTopControlsShownRatio) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const float kDefaultRatio = kDefaultTopControlsShownRatio;

  // Initial update with default ratio.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), kDefaultRatio);

  // Update to a new ratio.
  const float kRatio2 = 0.5f;
  auto update2 = CreateDefaultUpdate();
  update2->top_controls_shown_ratio = kRatio2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), kRatio2);

  // Update to another ratio.
  const float kRatio3 = 0.25f;
  auto update3 = CreateDefaultUpdate();
  update3->top_controls_shown_ratio = kRatio3;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), kRatio3);

  // Update with no change.
  auto update4 = CreateDefaultUpdate();
  update4->top_controls_shown_ratio = kRatio3;  // Same as previous
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), kRatio3);

  // Update with invalid ratio < 0 should succeeed.
  const float kRatio5 = -0.1;
  auto update5 = CreateDefaultUpdate();
  update5->top_controls_shown_ratio = kRatio5;
  auto result5 = layer_context_impl_->DoUpdateDisplayTree(std::move(update5));
  ASSERT_TRUE(result5.has_value());
  // Value gets clamped.
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), 0.f);

  // Update with invalid ratio > 1 should succeed.
  const float kRatio6 = 1.1;
  auto update6 = CreateDefaultUpdate();
  update6->top_controls_shown_ratio = kRatio6;
  auto result6 = layer_context_impl_->DoUpdateDisplayTree(std::move(update6));
  ASSERT_TRUE(result6.has_value());
  // Value gets clamped.
  EXPECT_EQ(active_tree->CurrentTopControlsShownRatio(), 1.f);
}

TEST_F(LayerContextImplLayerTreePropertiesTest,
       UpdateBottomControlsShownRatio) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const float kDefaultRatio = kDefaultBottomControlsShownRatio;

  // Initial update with default ratio.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->CurrentBottomControlsShownRatio(), kDefaultRatio);

  // Update to a new ratio.
  const float kRatio2 = 0.75f;
  auto update2 = CreateDefaultUpdate();
  update2->bottom_controls_shown_ratio = kRatio2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->CurrentBottomControlsShownRatio(), kRatio2);

  // Update with no change.
  auto update3 = CreateDefaultUpdate();
  update3->bottom_controls_shown_ratio = kRatio2;  // Same as previous
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->CurrentBottomControlsShownRatio(), kRatio2);

  // Update with invalid ratio < 0 should succeed.
  const float kRatio4 = -0.1;
  auto update4 = CreateDefaultUpdate();
  update4->bottom_controls_shown_ratio = kRatio4;
  auto result4 = layer_context_impl_->DoUpdateDisplayTree(std::move(update4));
  ASSERT_TRUE(result4.has_value());
  // Value gets clamped.
  EXPECT_EQ(active_tree->CurrentBottomControlsShownRatio(), 0.f);

  // Update with invalid ratio > 1 should succeed.
  const float kRatio5 = 1.1;
  auto update5 = CreateDefaultUpdate();
  update5->bottom_controls_shown_ratio = kRatio5;
  auto result5 = layer_context_impl_->DoUpdateDisplayTree(std::move(update5));
  ASSERT_TRUE(result5.has_value());
  // Balue gets clamped.
  EXPECT_EQ(active_tree->CurrentBottomControlsShownRatio(), 1.f);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateSelection) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->selection(), cc::LayerSelection());

  // Update to a new selection.
  cc::LayerSelection selection2;
  selection2.start.type = gfx::SelectionBound::Type::RIGHT;
  selection2.start.edge_start = gfx::Point(1, 3);
  selection2.start.edge_end = gfx::Point(2, 4);
  selection2.start.layer_id = 8;
  selection2.start.hidden = true;
  selection2.end.type = gfx::SelectionBound::Type::CENTER;
  selection2.end.edge_start = gfx::Point(7, 9);
  selection2.end.edge_end = gfx::Point(6, 11);
  selection2.end.layer_id = 12;
  selection2.end.hidden = false;
  auto update2 = CreateDefaultUpdate();
  update2->selection = selection2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->selection(), selection2);

  // Update back to an empty selection.
  auto update3 = CreateDefaultUpdate();
  update3->selection = cc::LayerSelection();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(active_tree->selection(), cc::LayerSelection());
}

class LayerContextImplBrowserControlsOffsetTagTest
    : public LayerContextImplTest {};

// Test that BrowserControlsOffsetTagModifications are deserialized and applied
// correctly.
TEST_F(LayerContextImplBrowserControlsOffsetTagTest,
       DeserializeBrowserControlsOffsetTagModifications) {
  auto update = CreateDefaultUpdate();
  cc::BrowserControlsOffsetTagModifications modifications;
  modifications.tags.top_controls_offset_tag = OffsetTag::CreateRandom();
  modifications.tags.content_offset_tag = OffsetTag::CreateRandom();
  modifications.tags.bottom_controls_offset_tag = OffsetTag::CreateRandom();
  modifications.top_controls_additional_height = 10;
  modifications.bottom_controls_additional_height = 20;
  update->browser_controls_offset_tag_modifications = modifications;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const auto& offset_tag_modifications = layer_context_impl_->host_impl()
                                             ->browser_controls_manager()
                                             ->GetOffsetTagModifications();
  EXPECT_EQ(offset_tag_modifications.tags.top_controls_offset_tag,
            modifications.tags.top_controls_offset_tag);
  EXPECT_EQ(offset_tag_modifications.tags.content_offset_tag,
            modifications.tags.content_offset_tag);
  EXPECT_EQ(offset_tag_modifications.tags.bottom_controls_offset_tag,
            modifications.tags.bottom_controls_offset_tag);
  EXPECT_EQ(offset_tag_modifications.top_controls_additional_height, 10);
  EXPECT_EQ(offset_tag_modifications.bottom_controls_additional_height, 20);
}

class LayerContextImplDebugStateTest : public LayerContextImplTest {};

TEST_F(LayerContextImplDebugStateTest, UpdateDebugState) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();
  const cc::LayerTreeDebugState kDefaultDebugState;

  // Default debug states
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(host_impl->debug_state(), kDefaultDebugState);

  // Updated to enabled debug states
  auto update2 = CreateDefaultUpdate();
  cc::LayerTreeDebugState debug_state2;
  debug_state2.debugger_paused = true;
  debug_state2.show_fps_counter = true;
  debug_state2.show_debug_borders.set(cc::DebugBorderType::RENDERPASS);
  debug_state2.show_debug_borders.set(cc::DebugBorderType::SURFACE);
  debug_state2.show_debug_borders.set(cc::DebugBorderType::LAYER);
  debug_state2.show_layout_shift_regions = true;
  debug_state2.show_paint_rects = true;
  debug_state2.show_property_changed_rects = true;
  debug_state2.show_surface_damage_rects = true;
  debug_state2.show_screen_space_rects = true;
  debug_state2.show_touch_event_handler_rects = true;
  debug_state2.show_wheel_event_handler_rects = true;
  debug_state2.show_scroll_event_handler_rects = true;
  debug_state2.show_main_thread_scroll_hit_test_rects = true;
  debug_state2.show_main_thread_scroll_repaint_rects = true;
  debug_state2.show_raster_inducing_scroll_rects = true;
  debug_state2.show_layer_animation_bounds_rects = true;
  debug_state2.slow_down_raster_scale_factor = 2;
  debug_state2.rasterize_only_visible_content = true;
  debug_state2.SetRecordRenderingStats(true);
  update2->debug_state = debug_state2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(host_impl->debug_state(), debug_state2);

  // Update back to the default states
  auto update3 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(host_impl->debug_state(), kDefaultDebugState);
}

}  // namespace
}  // namespace viz
