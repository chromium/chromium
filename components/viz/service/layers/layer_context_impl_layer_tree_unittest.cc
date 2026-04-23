// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "cc/debug/layer_tree_debug_state.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/trees/latency_info_swap_promise.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/latency/latency_info.h"

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

TEST_F(LayerContextImplLayerTreePropertiesTest,
       UpdateIsViewportMobileOptimized) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->is_viewport_mobile_optimized = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_FALSE(host_impl->viewport_mobile_optimized());

  // Update to true.
  auto update2 = CreateDefaultUpdate();
  update2->is_viewport_mobile_optimized = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(host_impl->viewport_mobile_optimized());

  // Update back to false.
  auto update3 = CreateDefaultUpdate();
  update3->is_viewport_mobile_optimized = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_FALSE(host_impl->viewport_mobile_optimized());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateIsAnimatingHUDContents) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->is_animating_hud_contents = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_FALSE(active_tree->IsAnimatingHUDContents());

  // Update to true.
  auto update2 = CreateDefaultUpdate();
  update2->is_animating_hud_contents = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(active_tree->IsAnimatingHUDContents());

  // Update back to false.
  auto update3 = CreateDefaultUpdate();
  update3->is_animating_hud_contents = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_FALSE(active_tree->IsAnimatingHUDContents());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateIsHandlingInteraction) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->is_handling_interaction = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_FALSE(host_impl->IsHandlingInteraction());

  // Update to true.
  auto update2 = CreateDefaultUpdate();
  update2->is_handling_interaction = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(host_impl->IsHandlingInteraction());

  // Update back to false.
  auto update3 = CreateDefaultUpdate();
  update3->is_handling_interaction = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_FALSE(host_impl->IsHandlingInteraction());
}

TEST_F(LayerContextImplLayerTreePropertiesTest,
       UpdateMayThrottleIfUndrawnFrames) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->may_throttle_if_undrawn_frames = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_TRUE(host_impl->may_throttle_if_undrawn_frames());

  // Update to false.
  auto update2 = CreateDefaultUpdate();
  update2->may_throttle_if_undrawn_frames = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_FALSE(host_impl->may_throttle_if_undrawn_frames());

  // Update back to true.
  auto update3 = CreateDefaultUpdate();
  update3->may_throttle_if_undrawn_frames = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_TRUE(host_impl->may_throttle_if_undrawn_frames());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateFullTreeDamaged) {
  cc::PropertyTrees* property_trees =
      layer_context_impl_->host_impl()->active_tree()->property_trees();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->full_tree_damaged = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_FALSE(property_trees->full_tree_damaged());

  // Update to true.
  auto update2 = CreateDefaultUpdate();
  update2->full_tree_damaged = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(property_trees->full_tree_damaged());

  // Update back to false (should stay true as it's a transient property that
  // we only set).
  auto update3 = CreateDefaultUpdate();
  update3->full_tree_damaged = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_TRUE(property_trees->full_tree_damaged());
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
  update2->browser_controls_shrink_blink_size =
      params2.browser_controls_shrink_blink_size;
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
  update3->browser_controls_shrink_blink_size =
      params3.browser_controls_shrink_blink_size;
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

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateDelegatedInkMetadata) {
  auto update1 = CreateDefaultUpdate();
  const gfx::RectF kFrame(0, 0, 100, 100);
  const base::TimeTicks kTimestamp = base::TimeTicks::Now();
  update1->delegated_ink_metadata = std::make_unique<gfx::DelegatedInkMetadata>(
      gfx::PointF(10, 10), 2.0f, SK_ColorRED, kTimestamp, kFrame,
      /*hovering=*/false);

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  const auto* metadata =
      layer_context_impl_->host_impl()->active_tree()->delegated_ink_metadata();
  ASSERT_TRUE(metadata);
  EXPECT_EQ(metadata->point(), gfx::PointF(10, 10));
  EXPECT_EQ(metadata->diameter(), 2.0f);
  EXPECT_EQ(metadata->color(), SK_ColorRED);
  EXPECT_EQ(metadata->timestamp(), kTimestamp);
  EXPECT_EQ(metadata->presentation_area(), kFrame);

  // Clear metadata.
  auto update2 = CreateDefaultUpdate();
  update2->delegated_ink_metadata.reset();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_FALSE(layer_context_impl_->host_impl()
                   ->active_tree()
                   ->delegated_ink_metadata());
}

TEST_F(LayerContextImplLayerTreePropertiesTest,
       UpdateViewportContainerBoundsDelta) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();
  const auto& property_trees = *active_tree->property_trees();

  const gfx::Vector2dF kDelta1(10.f, 20.f);
  const gfx::Vector2dF kDelta2(5.f, 15.f);

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->inner_viewport_container_bounds_delta = kDelta1;
  update1->outer_viewport_container_bounds_delta = kDelta2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(property_trees.inner_viewport_container_bounds_delta(), kDelta1);
  EXPECT_EQ(property_trees.outer_viewport_container_bounds_delta(), kDelta2);

  // Update to new values.
  const gfx::Vector2dF kDelta3(30.f, 40.f);
  auto update2 = CreateDefaultUpdate();
  update2->inner_viewport_container_bounds_delta = kDelta3;
  update2->outer_viewport_container_bounds_delta = kDelta2;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(property_trees.inner_viewport_container_bounds_delta(), kDelta3);
  EXPECT_EQ(property_trees.outer_viewport_container_bounds_delta(), kDelta2);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateViewportPropertyIds) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  auto update = CreateDefaultUpdate();

  // Add extra nodes using helper methods.
  int transform_id = AddTransformNode(update.get(), cc::kRootPropertyNodeId);
  int clip_id = AddClipNode(update.get(), cc::kRootPropertyNodeId);
  AddEffectNode(update.get(), cc::kRootPropertyNodeId);
  int scroll_id = AddScrollNode(update.get(), cc::kRootPropertyNodeId);

  // Update num nodes to match what was added.
  update->num_transform_nodes = next_transform_id_;
  update->num_clip_nodes = next_clip_id_;
  update->num_effect_nodes = next_effect_id_;
  update->num_scroll_nodes = next_scroll_id_;

  update->overscroll_elasticity_transform = transform_id;
  update->page_scale_transform = transform_id;
  update->inner_scroll = scroll_id;
  update->outer_clip = clip_id;
  update->outer_scroll = scroll_id;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value()) << result.error();

  const cc::ViewportPropertyIds& ids = active_tree->viewport_property_ids();
  EXPECT_EQ(ids.overscroll_elasticity_transform, transform_id);
  EXPECT_EQ(ids.page_scale_transform, transform_id);
  EXPECT_EQ(ids.inner_scroll, scroll_id);
  EXPECT_EQ(ids.outer_clip, clip_id);
  EXPECT_EQ(ids.outer_scroll, scroll_id);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdatePageScaleFactors) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->page_scale_factor = 1.2f;
  update1->min_page_scale_factor = 0.8f;
  update1->max_page_scale_factor = 2.5f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->page_scale_factor()->Current(true), 1.2f);
  EXPECT_EQ(active_tree->min_page_scale_factor(), 0.8f);
  EXPECT_EQ(active_tree->max_page_scale_factor(), 2.5f);

  // Update to new values.
  auto update2 = CreateDefaultUpdate();
  update2->page_scale_factor = 1.5f;
  update2->min_page_scale_factor = 1.0f;
  update2->max_page_scale_factor = 3.0f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->page_scale_factor()->Current(true), 1.5f);
  EXPECT_EQ(active_tree->min_page_scale_factor(), 1.0f);
  EXPECT_EQ(active_tree->max_page_scale_factor(), 3.0f);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateExternalPageScaleFactor) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->external_page_scale_factor = 1.2f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->external_page_scale_factor(), 1.2f);

  // Update to new value.
  auto update2 = CreateDefaultUpdate();
  update2->external_page_scale_factor = 0.9f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->external_page_scale_factor(), 0.9f);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateSurfaceRanges) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  const SurfaceRange ranges[] = {
      {SurfaceId(kDefaultFrameSinkId,
                 {1, base::UnguessableToken::CreateForTesting(2, 3)}),
       SurfaceId(kDefaultFrameSinkId,
                 {10, base::UnguessableToken::CreateForTesting(11, 12)})},
      {SurfaceId(kDefaultFrameSinkId,
                 {4, base::UnguessableToken::CreateForTesting(5, 6)}),
       SurfaceId(kDefaultFrameSinkId,
                 {13, base::UnguessableToken::CreateForTesting(14, 15)})}};

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->surface_ranges.emplace({ranges[0], ranges[1]});
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->SurfaceRanges().size(), 2U);
  EXPECT_TRUE(active_tree->SurfaceRanges().contains(ranges[0]));
  EXPECT_TRUE(active_tree->SurfaceRanges().contains(ranges[1]));

  // Update with different ranges.
  auto update2 = CreateDefaultUpdate();
  update2->surface_ranges.emplace({ranges[0]});
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->SurfaceRanges().size(), 1U);
  EXPECT_TRUE(active_tree->SurfaceRanges().contains(ranges[0]));
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateDeviceScaleFactors) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->device_scale_factor = 2.0f;
  update1->painted_device_scale_factor = 1.5f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_EQ(active_tree->device_scale_factor(), 2.0f);
  EXPECT_EQ(active_tree->painted_device_scale_factor(), 1.5f);

  // Update to new values.
  auto update2 = CreateDefaultUpdate();
  update2->device_scale_factor = 3.0f;
  update2->painted_device_scale_factor = 1.0f;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(active_tree->device_scale_factor(), 3.0f);
  EXPECT_EQ(active_tree->painted_device_scale_factor(), 1.0f);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateLatencyInfo) {
  cc::LayerTreeImpl* active_tree =
      layer_context_impl_->host_impl()->active_tree();

  // Initial update with latency info.
  auto update1 = CreateDefaultUpdate();
  ui::LatencyInfo latency1;
  latency1.set_trace_id(123);
  update1->latency_info.push_back(latency1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  // Verify it was queued by finishing swap promises.
  CompositorFrameMetadata metadata;
  active_tree->FinishSwapPromises(&metadata);
  ASSERT_EQ(metadata.latency_info.size(), 1u);
  EXPECT_EQ(metadata.latency_info[0].trace_id(), 123);
  active_tree->ClearSwapPromises();

  // Update with multiple latency infos.
  auto update2 = CreateDefaultUpdate();
  ui::LatencyInfo latency2;
  latency2.set_trace_id(456);
  ui::LatencyInfo latency3;
  latency3.set_trace_id(789);
  update2->latency_info.push_back(latency2);
  update2->latency_info.push_back(latency3);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());

  metadata.latency_info.clear();
  active_tree->FinishSwapPromises(&metadata);
  ASSERT_EQ(metadata.latency_info.size(), 2u);
  EXPECT_EQ(metadata.latency_info[0].trace_id(), 456);
  EXPECT_EQ(metadata.latency_info[1].trace_id(), 789);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateThrottleAndInteraction) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial update.
  auto update1 = CreateDefaultUpdate();
  update1->may_throttle_if_undrawn_frames = false;
  update1->is_handling_interaction = true;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_FALSE(host_impl->may_throttle_if_undrawn_frames());
  EXPECT_TRUE(host_impl->IsHandlingInteraction());

  // Update to different values.
  auto update2 = CreateDefaultUpdate();
  update2->may_throttle_if_undrawn_frames = true;
  update2->is_handling_interaction = false;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_TRUE(host_impl->may_throttle_if_undrawn_frames());
  EXPECT_FALSE(host_impl->IsHandlingInteraction());
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateWithFrameHasDamage) {
  // Verify that updates with/without frame_has_damage can be processed.
  auto update1 = CreateDefaultUpdate();
  const BeginFrameArgs args1 = update1->begin_frame_args;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  layer_context_impl_->DoDraw(args1, base::TimeTicks::Now(),
                              /*frame_has_damage=*/true);

  auto update2 = CreateDefaultUpdate();
  const BeginFrameArgs args2 = update2->begin_frame_args;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  layer_context_impl_->DoDraw(args2, base::TimeTicks::Now(),
                              /*frame_has_damage=*/false);
}

TEST_F(LayerContextImplLayerTreePropertiesTest, UpdateWithIsFlush) {
  // Regular updates must have a current_local_surface_id.
  auto update1 = CreateDefaultUpdate();
  update1->is_flush = false;
  update1->current_local_surface_id = std::nullopt;
  auto result1 = layer_context_impl_->DoUpdateDisplayTree(std::move(update1));
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error(),
            "Missing current_local_surface_id in non-flush update");

  // Flush updates can omit it.
  auto update2 = CreateDefaultUpdate();
  update2->is_flush = true;
  update2->current_local_surface_id = std::nullopt;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
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
