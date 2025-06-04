// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

const float kDefaultPageScaleFactor = 1.0f;
const float kDefaultMinPageScaleFactor = 0.5f;
const float kDefaultMaxPageScaleFactor = 2.0f;

class LayerContextImplTest : public testing::Test {
 public:
  LayerContextImplTest()
      : frame_sink_manager_(FrameSinkManagerImpl::InitParams()) {}

  void SetUp() override {
    compositor_frame_sink_support_ =
        std::make_unique<CompositorFrameSinkSupport>(
            &dummy_client_, &frame_sink_manager_, kDefaultFrameSinkId,
            /*is_root=*/true);
    layer_context_impl_ = LayerContextImpl::CreateForTesting(
        compositor_frame_sink_support_.get(), /*draw_mode_is_gpu=*/true);
  }

  mojom::LayerTreeUpdatePtr CreateDefaultUpdate() {
    auto update = mojom::LayerTreeUpdate::New();
    update->source_frame_number = 1;
    update->trace_id = 1;
    update->primary_main_frame_item_sequence_number = 1;
    update->device_viewport = gfx::Rect(100, 100);
    update->background_color = SkColors::kTransparent;

    // Valid scale factors by default
    update->page_scale_factor = kDefaultPageScaleFactor;
    update->min_page_scale_factor = kDefaultMinPageScaleFactor;
    update->max_page_scale_factor = kDefaultMaxPageScaleFactor;
    update->external_page_scale_factor = 1.0f;
    update->device_scale_factor = 1.0f;
    update->painted_device_scale_factor = 1.0f;

    // Minimal property tree setup
    update->num_transform_nodes = 1;
    update->num_clip_nodes = 1;
    update->num_effect_nodes = 1;
    update->num_scroll_nodes = 1;

    auto transform_node = mojom::TransformNode::New();
    transform_node->id = cc::kRootPropertyNodeId;
    transform_node->parent_id = cc::kInvalidPropertyNodeId;
    update->transform_nodes.push_back(std::move(transform_node));

    auto clip_node = mojom::ClipNode::New();
    clip_node->id = cc::kRootPropertyNodeId;
    clip_node->parent_id = cc::kInvalidPropertyNodeId;
    clip_node->transform_id = cc::kRootPropertyNodeId;
    update->clip_nodes.push_back(std::move(clip_node));

    auto effect_node = mojom::EffectNode::New();
    effect_node->id = cc::kRootPropertyNodeId;
    effect_node->parent_id = cc::kInvalidPropertyNodeId;
    effect_node->transform_id = cc::kRootPropertyNodeId;
    effect_node->clip_id = cc::kRootPropertyNodeId;
    effect_node->target_id = cc::kRootPropertyNodeId;
    update->effect_nodes.push_back(std::move(effect_node));

    auto scroll_node = mojom::ScrollNode::New();
    scroll_node->id = cc::kRootPropertyNodeId;
    scroll_node->parent_id = cc::kInvalidPropertyNodeId;
    scroll_node->transform_id = cc::kRootPropertyNodeId;
    update->scroll_nodes.push_back(std::move(scroll_node));

    // Viewport property IDs
    update->overscroll_elasticity_transform = cc::kInvalidPropertyNodeId;
    update->page_scale_transform = cc::kInvalidPropertyNodeId;
    update->inner_scroll = cc::kInvalidPropertyNodeId;
    update->outer_clip = cc::kInvalidPropertyNodeId;
    update->outer_scroll = cc::kInvalidPropertyNodeId;

    // Other defaults
    update->display_color_spaces = gfx::DisplayColorSpaces();
    update->begin_frame_args = BeginFrameArgs();

    return update;
  }

 protected:
  static constexpr FrameSinkId kDefaultFrameSinkId = FrameSinkId(1, 1);
  FakeCompositorFrameSinkClient dummy_client_;
  FrameSinkManagerImpl frame_sink_manager_;
  std::unique_ptr<CompositorFrameSinkSupport> compositor_frame_sink_support_;
  std::unique_ptr<LayerContextImpl> layer_context_impl_;
};

TEST_F(LayerContextImplTest, EmptyScrollingContentsCullRectsByDefault) {
  EXPECT_TRUE(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->property_trees()
                  ->scroll_tree()
                  .scrolling_contents_cull_rects()
                  .empty());

  auto result = layer_context_impl_->DoUpdateDisplayTree(CreateDefaultUpdate());
  ASSERT_TRUE(result.has_value());

  EXPECT_TRUE(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->property_trees()
                  ->scroll_tree()
                  .scrolling_contents_cull_rects()
                  .empty());
}

TEST_F(LayerContextImplTest, ScrollingContentsCullRectsAreSynchronized) {
  constexpr cc::ElementId kElementId = cc::ElementId(42);
  constexpr gfx::Rect kCullRect = gfx::Rect{100, 100};
  base::flat_map<cc::ElementId, gfx::Rect> scrolling_contents_cull_rects;
  scrolling_contents_cull_rects[kElementId] = kCullRect;

  auto scroll_tree_update = mojom::ScrollTreeUpdate::New();
  scroll_tree_update->scrolling_contents_cull_rects =
      scrolling_contents_cull_rects;

  auto update = CreateDefaultUpdate();
  update->scroll_tree_update = std::move(scroll_tree_update);

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  EXPECT_TRUE(result.has_value());

  auto synchronized_scrolling_contents_cull_rects =
      layer_context_impl_->host_impl()
          ->active_tree()
          ->property_trees()
          ->scroll_tree()
          .scrolling_contents_cull_rects();
  EXPECT_EQ(scrolling_contents_cull_rects,
            synchronized_scrolling_contents_cull_rects);
}

class LayerTreeContextUpdateDisplayTreeScaleFactorTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<float, bool>> {};

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest, PageScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->page_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    float expected_factor =
        std::min(std::max(scale_factor, kDefaultMinPageScaleFactor),
                 kDefaultMaxPageScaleFactor);
    EXPECT_EQ(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->current_page_scale_factor(),
              expected_factor);
  } else {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid page scale factors");
  }
}

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest, MinPageScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->min_page_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->min_page_scale_factor(),
              scale_factor);
  } else {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid page scale factors");
  }
}

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest, MaxPageScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->max_page_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->max_page_scale_factor(),
              scale_factor);
  } else {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid page scale factors");
  }
}

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest,
       ExternalPageScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->external_page_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->external_page_scale_factor(),
              scale_factor);
  } else {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid external page scale factor");
  }
}

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest, DeviceScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->device_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(
        layer_context_impl_->host_impl()->active_tree()->device_scale_factor(),
        scale_factor);
  } else {
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid device scale factor");
  }
}

TEST_P(LayerTreeContextUpdateDisplayTreeScaleFactorTest,
       PaintedDeviceScaleFactor) {
  const float scale_factor = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  update->painted_device_scale_factor = scale_factor;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->painted_device_scale_factor(),
              scale_factor);
  } else {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid painted device scale factor");
  }
}

INSTANTIATE_TEST_SUITE_P(
    TreeScaleFactors,
    LayerTreeContextUpdateDisplayTreeScaleFactorTest,
    ::testing::Values(
        // Test value below min_page_scale_factor.
        std::make_tuple(0.25f, true),

        // Test value inside min/max_page_scale_factor.
        std::make_tuple(1.23f, true),

        // Test value outside min/max_page_scale_factor.
        std::make_tuple(2.5, true),

        // Test invalid values.
        std::make_tuple(0.0f, false),
        std::make_tuple(-1.0f, false),
        std::make_tuple(std::numeric_limits<float>::infinity(), false),
        std::make_tuple(std::numeric_limits<float>::quiet_NaN(), false)),
    [](const testing::TestParamInfo<
        LayerTreeContextUpdateDisplayTreeScaleFactorTest::ParamType>& info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

}  // namespace
}  // namespace viz
