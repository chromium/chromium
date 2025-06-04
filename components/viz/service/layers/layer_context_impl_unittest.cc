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

    // Root & Secondary transform nodes are always expected 1st
    update->num_transform_nodes = 0;
    AddTransformNode(update.get(), cc::kInvalidPropertyNodeId);
    AddTransformNode(update.get(), cc::kRootPropertyNodeId);

    // Updating the page scale requires that a page_scale_transform node is
    // set up.
    viewport_property_ids.overscroll_elasticity_transform =
        AddTransformNode(update.get(), cc::kSecondaryRootPropertyNodeId);
    viewport_property_ids.page_scale_transform = AddTransformNode(
        update.get(), viewport_property_ids.overscroll_elasticity_transform);
    update->transform_nodes.back()->in_subtree_of_page_scale_layer = true;

    // Root & Secondary clip nodes are always expected
    update->num_clip_nodes = 0;
    AddClipNode(update.get(), cc::kInvalidPropertyNodeId);
    AddClipNode(update.get(), cc::kRootPropertyNodeId);

    // Root & Secondary effect nodes are always expected
    update->num_effect_nodes = 0;
    AddEffectNode(update.get(), cc::kInvalidPropertyNodeId);
    AddEffectNode(update.get(), cc::kRootPropertyNodeId);
    update->effect_nodes.back()->render_surface_reason =
        cc::RenderSurfaceReason::kRoot;
    update->effect_nodes.back()->element_id = cc::ElementId(1ULL);

    // Root & Secondary scroll nodes are always expected
    update->num_scroll_nodes = 0;
    AddScrollNode(update.get(), cc::kInvalidPropertyNodeId);
    AddScrollNode(update.get(), cc::kRootPropertyNodeId);

    // Viewport property IDs
    update->overscroll_elasticity_transform =
        viewport_property_ids.overscroll_elasticity_transform;
    update->page_scale_transform = viewport_property_ids.page_scale_transform;
    update->inner_scroll = viewport_property_ids.inner_scroll;
    update->outer_clip = viewport_property_ids.outer_clip;
    update->outer_scroll = viewport_property_ids.outer_scroll;

    // Root layer
    AddDefaultLayerToUpdate(update.get());

    // Other defaults
    update->display_color_spaces = gfx::DisplayColorSpaces();
    update->local_surface_id_from_parent =
        LocalSurfaceId(1, base::UnguessableToken::CreateForTesting(2u, 3u));

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta interval = base::Milliseconds(16);
    update->begin_frame_args = BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, BeginFrameArgs::kStartingSourceId,
        BeginFrameArgs::kStartingFrameNumber, now, now + interval, interval,
        BeginFrameArgs::NORMAL);

    return update;
  }

  int AddTransformNode(mojom::LayerTreeUpdate* update, int parent) {
    auto node = mojom::TransformNode::New();
    int id = next_transform_id_++;
    node->id = id;
    node->parent_id = parent;
    update->transform_nodes.push_back(std::move(node));
    update->num_transform_nodes = next_transform_id_;
    return id;
  }

  int AddClipNode(mojom::LayerTreeUpdate* update, int parent) {
    auto node = mojom::ClipNode::New();
    int id = next_clip_id_++;
    node->id = id;
    node->parent_id = parent;
    node->transform_id = viewport_property_ids.page_scale_transform;
    update->clip_nodes.push_back(std::move(node));
    update->num_clip_nodes = next_clip_id_;
    return id;
  }

  int AddEffectNode(mojom::LayerTreeUpdate* update, int parent) {
    auto node = mojom::EffectNode::New();
    int id = next_effect_id_++;
    node->id = id;
    node->parent_id = parent;
    node->transform_id = viewport_property_ids.page_scale_transform;
    node->clip_id = cc::kRootPropertyNodeId;
    node->target_id = cc::kRootPropertyNodeId;
    update->effect_nodes.push_back(std::move(node));
    update->num_effect_nodes = next_effect_id_;
    return id;
  }

  int AddScrollNode(mojom::LayerTreeUpdate* update, int parent) {
    auto node = mojom::ScrollNode::New();
    int id = next_scroll_id_++;
    node->id = id;
    node->parent_id = parent;
    node->transform_id = viewport_property_ids.page_scale_transform;
    update->scroll_nodes.push_back(std::move(node));
    update->num_scroll_nodes = next_scroll_id_;
    return id;
  }

  // Helper to add a default layer to the update.
  // Returns the ID of the added layer.
  int AddDefaultLayerToUpdate(
      mojom::LayerTreeUpdate* update,
      cc::mojom::LayerType type = cc::mojom::LayerType::kLayer) {
    auto layer = mojom::Layer::New();
    const auto id = next_layer_id_++;
    layer->id = id;
    layer->type = type;
    layer->transform_tree_index = cc::kSecondaryRootPropertyNodeId;
    layer->clip_tree_index = cc::kRootPropertyNodeId;
    layer->effect_tree_index = cc::kSecondaryRootPropertyNodeId;

    update->layers.push_back(std::move(layer));

    if (!update->layer_order) {
      update->layer_order.emplace();
    }
    update->layer_order->push_back(id);
    return id;
  }

 protected:
  static constexpr FrameSinkId kDefaultFrameSinkId = FrameSinkId(1, 1);
  FakeCompositorFrameSinkClient dummy_client_;
  FrameSinkManagerImpl frame_sink_manager_;
  std::unique_ptr<CompositorFrameSinkSupport> compositor_frame_sink_support_;
  std::unique_ptr<LayerContextImpl> layer_context_impl_;
  // Layer IDs start at 1, as 0 is reserved for cc::kInvalidLayerId.
  int next_layer_id_ = 1;
  // Property tree IDs start at 0.
  int next_transform_id_ = 0;
  int next_clip_id_ = 0;
  int next_effect_id_ = 0;
  int next_scroll_id_ = 0;
  cc::ViewportPropertyIds viewport_property_ids;
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

class LayerContextImplUpdateDisplayTreePageScaleFactorTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<float, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreePageScaleFactorTest, PageScaleFactor) {
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

INSTANTIATE_TEST_SUITE_P(
    PageScaleFactor,
    LayerContextImplUpdateDisplayTreePageScaleFactorTest,
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
        LayerContextImplUpdateDisplayTreePageScaleFactorTest::ParamType>&
           info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

class LayerContextImplUpdateDisplayTreeMinPageScaleFactorTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<float, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeMinPageScaleFactorTest,
       MinPageScaleFactor) {
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

INSTANTIATE_TEST_SUITE_P(
    MinPageScaleFactor,
    LayerContextImplUpdateDisplayTreeMinPageScaleFactorTest,
    ::testing::Values(
        // Test value below max_page_scale_factor.
        std::make_tuple(kDefaultMaxPageScaleFactor - 0.1f, true),

        // Test value equal to max_page_scale_factor.
        std::make_tuple(kDefaultMaxPageScaleFactor, true),

        // Test value greater than max_page_scale_factor.
        std::make_tuple(kDefaultMaxPageScaleFactor + 0.1f, false),

        // Test invalid values.
        std::make_tuple(0.0f, false),
        std::make_tuple(-1.0f, false),
        std::make_tuple(std::numeric_limits<float>::infinity(), false),
        std::make_tuple(std::numeric_limits<float>::quiet_NaN(), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeMinPageScaleFactorTest::ParamType>&
           info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

class LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<float, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest,
       MaxPageScaleFactor) {
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

INSTANTIATE_TEST_SUITE_P(
    MaxPageScaleFactor,
    LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest,
    ::testing::Values(
        // Test value equal to min_page_scale_factor.
        std::make_tuple(kDefaultMinPageScaleFactor, true),

        // Test value above min_page_scale_factor.
        std::make_tuple(kDefaultMinPageScaleFactor + 0.1f, true),

        // Test value below min_page_scale_factor.
        std::make_tuple(kDefaultMinPageScaleFactor - 0.1f, false),

        // Test invalid values.
        std::make_tuple(0.0f, false),
        std::make_tuple(-1.0f, false),
        std::make_tuple(std::numeric_limits<float>::infinity(), false),
        std::make_tuple(std::numeric_limits<float>::quiet_NaN(), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest::ParamType>&
           info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

class LayerContextImplUpdateDisplayTreeScaleFactorTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<float, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeScaleFactorTest,
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
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid external page scale factor");
  }
}

TEST_P(LayerContextImplUpdateDisplayTreeScaleFactorTest, DeviceScaleFactor) {
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
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid device scale factor");
  }
}

TEST_P(LayerContextImplUpdateDisplayTreeScaleFactorTest,
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
    LayerContextImplUpdateDisplayTreeScaleFactorTest,
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
        LayerContextImplUpdateDisplayTreeScaleFactorTest::ParamType>& info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

}  // namespace
}  // namespace viz
