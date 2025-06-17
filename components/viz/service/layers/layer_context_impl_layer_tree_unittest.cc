// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "cc/trees/layer_tree_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

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

}  // namespace
}  // namespace viz
