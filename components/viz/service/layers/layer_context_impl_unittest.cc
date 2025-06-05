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
#include "gpu/GLES2/gl2extchromium.h"
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
      : frame_sink_manager_(FrameSinkManagerImpl::InitParams()),
        default_local_surface_id_(
            LocalSurfaceId(1,
                           base::UnguessableToken::CreateForTesting(2u, 3u))) {}

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

    if (first_update_) {
      AddFirstTimeDefaultProperties(update.get());
      first_update_ = false;
    }
    AddDefaultPropertyUpdates(update.get());

    return update;
  }

  void AddDefaultPropertyUpdates(mojom::LayerTreeUpdate* update) {
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

    update->num_transform_nodes = next_transform_id_;
    update->num_clip_nodes = next_clip_id_;
    update->num_effect_nodes = next_effect_id_;
    update->num_scroll_nodes = next_scroll_id_;

    // Viewport property IDs
    update->overscroll_elasticity_transform =
        viewport_property_ids.overscroll_elasticity_transform;
    update->page_scale_transform = viewport_property_ids.page_scale_transform;
    update->inner_scroll = viewport_property_ids.inner_scroll;
    update->outer_clip = viewport_property_ids.outer_clip;
    update->outer_scroll = viewport_property_ids.outer_scroll;

    // Other defaults
    update->display_color_spaces = gfx::DisplayColorSpaces();
    update->local_surface_id_from_parent = default_local_surface_id_;

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta interval = base::Milliseconds(16);
    update->begin_frame_args = BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, BeginFrameArgs::kStartingSourceId,
        BeginFrameArgs::kStartingFrameNumber, now, now + interval, interval,
        BeginFrameArgs::NORMAL);
  }

  void AddFirstTimeDefaultProperties(mojom::LayerTreeUpdate* update) {
    // Root & Secondary transform nodes are always expected 1st
    AddTransformNode(update, cc::kInvalidPropertyNodeId);
    AddTransformNode(update, cc::kRootPropertyNodeId);

    // Updating the page scale requires that a page_scale_transform node is
    // set up.
    viewport_property_ids.overscroll_elasticity_transform =
        AddTransformNode(update, cc::kSecondaryRootPropertyNodeId);
    viewport_property_ids.page_scale_transform = AddTransformNode(
        update, viewport_property_ids.overscroll_elasticity_transform);
    update->transform_nodes.back()->in_subtree_of_page_scale_layer = true;

    // Root & Secondary clip nodes are always expected
    AddClipNode(update, cc::kInvalidPropertyNodeId);
    AddClipNode(update, cc::kRootPropertyNodeId);

    // Root & Secondary effect nodes are always expected
    AddEffectNode(update, cc::kInvalidPropertyNodeId);
    AddEffectNode(update, cc::kRootPropertyNodeId);
    update->effect_nodes.back()->render_surface_reason =
        cc::RenderSurfaceReason::kRoot;
    update->effect_nodes.back()->element_id = cc::ElementId(1ULL);

    // Root & Secondary scroll nodes are always expected
    AddScrollNode(update, cc::kInvalidPropertyNodeId);
    AddScrollNode(update, cc::kRootPropertyNodeId);

    // Root layer
    AddDefaultLayerToUpdate(update);
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
      cc::mojom::LayerType type = cc::mojom::LayerType::kLayer,
      int id = -1) {
    auto layer = mojom::Layer::New();
    if (id == -1) {
      id = next_layer_id_++;
    }
    layer->id = id;
    layer->type = type;
    layer->transform_tree_index = cc::kSecondaryRootPropertyNodeId;
    layer->clip_tree_index = cc::kRootPropertyNodeId;
    layer->effect_tree_index = cc::kSecondaryRootPropertyNodeId;

    update->layers.push_back(std::move(layer));

    // Update the local layer order, and in the LayerTreeUpdate.
    layer_order_.push_back(id);
    update->layer_order = layer_order_;
    return id;
  }

  void RemoveLayerInUpdate(mojom::LayerTreeUpdate* update, int id) {
    // Remove the ID from the local list of layers if it exists.
    auto it = std::find(layer_order_.begin(), layer_order_.end(), id);
    if (it != layer_order_.end()) {
      layer_order_.erase(it);
    }

    // Update the layer order in the LayerTreeUpdate.
    update->layer_order = layer_order_;
  }

 protected:
  static constexpr FrameSinkId kDefaultFrameSinkId = FrameSinkId(1, 1);

  FakeCompositorFrameSinkClient dummy_client_;
  FrameSinkManagerImpl frame_sink_manager_;
  LocalSurfaceId default_local_surface_id_;

  std::unique_ptr<CompositorFrameSinkSupport> compositor_frame_sink_support_;
  std::unique_ptr<LayerContextImpl> layer_context_impl_;
  bool first_update_ = true;
  // Layer IDs start at 1, as 0 is reserved for cc::kInvalidLayerId.
  int next_layer_id_ = 1;
  // Property tree IDs start at 0.
  int next_transform_id_ = 0;
  int next_clip_id_ = 0;
  int next_effect_id_ = 0;
  int next_scroll_id_ = 0;
  cc::ViewportPropertyIds viewport_property_ids;
  std::vector<int> layer_order_;
};

namespace {

mojom::TransferableUIResourceRequestPtr CreateUIResourceRequest(
    int uid,
    mojom::TransferableUIResourceRequest::Type type) {
  auto request = mojom::TransferableUIResourceRequest::New();
  request->uid = uid;
  request->type = type;
  if (type == mojom::TransferableUIResourceRequest::Type::kCreate) {
    // Add a minimal valid resource.
    request->transferable_resource = TransferableResource::MakeGpu(
        gpu::Mailbox::Generate(), GL_TEXTURE_2D,
        gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                       gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
        gfx::Size(1, 1), SinglePlaneFormat::kRGBA_8888,
        false /* is_overlay_candidate */);
  }
  return request;
}

}  // namespace

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

class LayerContextImplUpdateDisplayTreeUIResourceRequestTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<gfx::Size, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeUIResourceRequestTest, ResourceSize) {
  const gfx::Size resource_size = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());

  auto update = CreateDefaultUpdate();
  auto request = mojom::TransferableUIResourceRequest::New();
  request->type = mojom::TransferableUIResourceRequest::Type::kCreate;
  request->uid = 42;
  request->transferable_resource = TransferableResource::MakeGpu(
      gpu::Mailbox::Generate(), GL_TEXTURE_2D,
      gpu::SyncToken(gpu::CommandBufferNamespace::GPU_IO,
                     gpu::CommandBufferId::FromUnsafeValue(0x234), 0x456),
      resource_size, SinglePlaneFormat::kRGBA_8888,
      false /* is_overlay_candidate */);
  update->ui_resource_requests.push_back(std::move(request));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    EXPECT_NE(layer_context_impl_->host_impl()->ResourceIdForUIResource(
                  /*uid=*/42),
              kInvalidResourceId);
  } else {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(),
              "Invalid dimensions for transferable UI resource.");
  }
}

INSTANTIATE_TEST_SUITE_P(
    UIResourceRequestDimensions,
    LayerContextImplUpdateDisplayTreeUIResourceRequestTest,
    ::testing::Values(std::make_tuple(gfx::Size(10, 10), true),
                      std::make_tuple(gfx::Size(0, 10), false),
                      std::make_tuple(gfx::Size(10, 0), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeUIResourceRequestTest::ParamType>&
           info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

class LayerContextImplUpdateDisplayTreeTilingTest
    : public LayerContextImplTest,
      public ::testing::WithParamInterface<std::tuple<gfx::Size, bool>> {};

TEST_P(LayerContextImplUpdateDisplayTreeTilingTest, TileSize) {
  const gfx::Size tile_size = std::get<0>(GetParam());
  const bool is_valid = std::get<1>(GetParam());
  auto update = CreateDefaultUpdate();
  int layer_id =
      AddDefaultLayerToUpdate(update.get(), cc::mojom::LayerType::kTileDisplay);

  auto tiling = mojom::Tiling::New();
  tiling->layer_id = layer_id;
  tiling->scale_key = 1.0f;
  tiling->raster_scale = gfx::Vector2dF(1.0f, 1.0f);
  tiling->tile_size = tile_size;
  tiling->tiling_rect = gfx::Rect(100, 100);

  auto tile = mojom::Tile::New();
  tile->column_index = 0;
  tile->row_index = 0;
  tile->contents = mojom::TileContents::NewSolidColor(SkColors::kRed);
  tiling->tiles.push_back(std::move(tile));

  update->tilings.push_back(std::move(tiling));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));

  if (is_valid) {
    EXPECT_TRUE(result.has_value());
    cc::LayerTreeImpl* active_tree =
        layer_context_impl_->host_impl()->active_tree();
    ASSERT_TRUE(active_tree);
    cc::LayerImpl* layer_impl = active_tree->LayerById(layer_id);
    ASSERT_TRUE(layer_impl);
    ASSERT_EQ(layer_impl->GetLayerType(), cc::mojom::LayerType::kTileDisplay);
    const auto* tile_display_layer =
        static_cast<const cc::TileDisplayLayerImpl*>(layer_impl);
    EXPECT_NE(nullptr,
              tile_display_layer->GetTilingForTesting(/*scale_key=*/1.0));
  } else {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid tile_size dimensions in Tiling");
  }
}

INSTANTIATE_TEST_SUITE_P(
    TileSize,
    LayerContextImplUpdateDisplayTreeTilingTest,
    ::testing::Values(std::make_tuple(gfx::Size(10, 10), true),
                      std::make_tuple(gfx::Size(0, 10), false),
                      std::make_tuple(gfx::Size(10, 0), false)),
    [](const testing::TestParamInfo<
        LayerContextImplUpdateDisplayTreeTilingTest::ParamType>& info) {
      std::stringstream name;
      name << (std::get<1>(info.param) ? "Valid" : "Invalid") << "_"
           << info.index;
      return name.str();
    });

}  // namespace

TEST_F(LayerContextImplTest, TransferableUIResourceLifecycleAndEdgeCases) {
  cc::LayerTreeHostImpl* host_impl = layer_context_impl_->host_impl();

  // Initial state: No resources.
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(3), kInvalidResourceId);

  // Test Case 1: Create UIResource 1. Verify it exists.
  auto update1 = CreateDefaultUpdate();
  update1->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 2: Create UIResource 2. Verify both 1 and 2 exist.
  auto update2 = CreateDefaultUpdate();
  update2->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 3: Remove UIResource 1. Verify 1 is gone, 2 exists.
  auto update3 = CreateDefaultUpdate();
  update3->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 4: Edge Case - Try to remove UIResource 1 again (already
  // removed). Verify no crash and 2 still exists.
  auto update4 = CreateDefaultUpdate();
  update4->ui_resource_requests.push_back(CreateUIResourceRequest(
      1, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(1), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 5: Edge Case - Try to remove UIResource 3 (never created).
  // Verify no crash and 2 still exists.
  auto update5 = CreateDefaultUpdate();
  update5->ui_resource_requests.push_back(CreateUIResourceRequest(
      3, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(3), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 6: Edge Case - Try to create UIResource 2 again (duplicate
  // create). Verify 2 still exists (it's replaced).
  ResourceId old_resource_2_id = host_impl->ResourceIdForUIResource(2);
  auto update6 = CreateDefaultUpdate();
  update6->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update6)).has_value());
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);
  // The resource ID might change upon re-creation.
  EXPECT_NE(host_impl->ResourceIdForUIResource(2), old_resource_2_id);

  // Test Case 7: Remove UIResource 2. Verify it's gone.
  auto update7 = CreateDefaultUpdate();
  update7->ui_resource_requests.push_back(CreateUIResourceRequest(
      2, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update7)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(2), kInvalidResourceId);

  // Test Case 8: Operations within the same update.
  // Create UID 10, then Delete UID 10. Should result in no resource 10.
  // Delete UID 11 (non-existent), then Create UID 11. Should result in
  // resource 11. Create UID 12, then Create UID 12 again. Should result in
  // resource 12.
  auto update8 = CreateDefaultUpdate();
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      10, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      10, mojom::TransferableUIResourceRequest::Type::kDelete));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kDelete));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kCreate));
  update8->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kCreate));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update8)).has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(10), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(11), kInvalidResourceId);
  EXPECT_NE(host_impl->ResourceIdForUIResource(12), kInvalidResourceId);

  // Cleanup remaining resources
  auto update_cleanup = CreateDefaultUpdate();
  update_cleanup->ui_resource_requests.push_back(CreateUIResourceRequest(
      11, mojom::TransferableUIResourceRequest::Type::kDelete));
  update_cleanup->ui_resource_requests.push_back(CreateUIResourceRequest(
      12, mojom::TransferableUIResourceRequest::Type::kDelete));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_cleanup))
          .has_value());
  EXPECT_EQ(host_impl->ResourceIdForUIResource(11), kInvalidResourceId);
  EXPECT_EQ(host_impl->ResourceIdForUIResource(12), kInvalidResourceId);
}

class LayerContextImplLayerLifecycleTest : public LayerContextImplTest {
 protected:
  cc::LayerImpl* GetLayerFromActiveTree(int layer_id) {
    return layer_context_impl_->host_impl()->active_tree()->LayerById(layer_id);
  }

  void VerifyLayerExists(int layer_id, bool should_exist) {
    if (should_exist) {
      EXPECT_NE(nullptr, GetLayerFromActiveTree(layer_id))
          << "Layer " << layer_id << " should exist.";
    } else {
      EXPECT_EQ(nullptr, GetLayerFromActiveTree(layer_id))
          << "Layer " << layer_id << " should not exist.";
    }
  }

  void VerifyLayerBounds(int layer_id, const gfx::Size& expected_bounds) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    ASSERT_NE(nullptr, layer) << "Layer " << layer_id << " not found.";
    EXPECT_EQ(expected_bounds, layer->bounds());
  }

  void VerifyLayerOrder(const std::vector<int>& expected_order) {
    cc::LayerTreeImpl* active_tree =
        layer_context_impl_->host_impl()->active_tree();
    ASSERT_EQ(expected_order.size(), active_tree->NumLayers());
    size_t i = 0;
    for (cc::LayerImpl* layer : *active_tree) {
      ASSERT_LT(i, expected_order.size());
      EXPECT_EQ(expected_order[i], layer->id()) << "Mismatch at index " << i;
      i++;
    }
  }

  // Helper to manually add a layer to an update, bypassing AddDefaultLayer.
  // This is useful for testing specific ID scenarios or invalid properties.
  mojom::LayerPtr CreateManualLayer(
      int id,
      cc::mojom::LayerType type = cc::mojom::LayerType::kLayer,
      const gfx::Size& bounds = gfx::Size(10, 10),
      int transform_idx = cc::kSecondaryRootPropertyNodeId,
      int clip_idx = cc::kRootPropertyNodeId,
      int effect_idx = cc::kSecondaryRootPropertyNodeId) {
    auto layer = mojom::Layer::New();
    layer->id = id;
    layer->type = type;
    layer->bounds = bounds;
    layer->transform_tree_index = transform_idx;
    layer->clip_tree_index = clip_idx;
    layer->effect_tree_index = effect_idx;
    return layer;
  }
};

TEST_F(LayerContextImplLayerLifecycleTest, LayerLifecycleAndEdgeCases) {
  constexpr int kLayerId1 = 2;  // Start after default root layer (ID 1).
  constexpr int kLayerId2 = 3;
  constexpr int kLayerId3 = 4;
  constexpr int kNonExistentLayerId = 99;

  (void)kLayerId2;
  (void)kLayerId3;
  (void)kNonExistentLayerId;

  // Test Case 1: Basic Layer Lifecycle (Create, Update Bounds, Remove)
  // Update 1: Create Layer ID kLayerId1.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, gfx::Size(0, 0));  // Default bounds

  // Update 2: Update bounds of Layer ID kLayerId1.
  auto update2 = CreateDefaultUpdate();
  update2->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, gfx::Size(20, 20)));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, gfx::Size(20, 20));

  // Update 3: Remove Layer ID kLayerId1.
  auto update3 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update3.get(), kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  VerifyLayerExists(kLayerId1, false);

  // Test Case 2: Multiple Layers and Interleaved Operations
  // Update 4: Re-Create Layer ID kLayerId1.
  auto update4 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update4.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerOrder({1, kLayerId1});

  // Update 5: Create Layer ID kLayerId2.
  auto update5 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update5.get(), cc::mojom::LayerType::kLayer,
                          kLayerId2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update5)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerExists(kLayerId2, true);
  VerifyLayerOrder({1, kLayerId1, kLayerId2});

  // Update 6: Update Layer ID kLayerId1.
  auto update6 = CreateDefaultUpdate();
  update6->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, gfx::Size(30, 30)));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update6)).has_value());
  VerifyLayerBounds(kLayerId1, gfx::Size(30, 30));
  VerifyLayerBounds(kLayerId2, gfx::Size(0, 0));  // Unaffected

  // Update 7: Remove Layer ID kLayerId1.
  auto update7 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update7.get(), kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update7)).has_value());
  VerifyLayerExists(kLayerId1, false);
  VerifyLayerExists(kLayerId2, true);
  VerifyLayerOrder({1, kLayerId2});

  // Update 8: Remove Layer ID kLayerId2.
  auto update8 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update8.get(), kLayerId2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update8)).has_value());
  VerifyLayerExists(kLayerId2, false);
  VerifyLayerOrder({1});

  // Test Case 3: Updating a Never Existing Layer should fail
  // Update 9: Create kLayerId1 again.
  auto update9 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update9.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update9)).has_value());
  VerifyLayerExists(kLayerId1, true);

  // Update 10: Attempt to update kNonExistentLayerId.
  auto update10 = CreateDefaultUpdate();
  update10->layers.push_back(
      CreateManualLayer(kNonExistentLayerId, cc::mojom::LayerType::kLayer,
                        gfx::Size(5, 5)));  // Update non-existent
  auto result10 = layer_context_impl_->DoUpdateDisplayTree(std::move(update10));
  ASSERT_FALSE(result10.has_value());
  EXPECT_EQ(result10.error(), "Invalid layer ID");
  VerifyLayerExists(kLayerId1, true);  // Unaffected
  VerifyLayerExists(kNonExistentLayerId, false);
  VerifyLayerOrder({1, kLayerId1});

  // Test Case 4: Updating on Previously Removed Layer shoulf fail
  // Update 11: Remove kLayerId1.
  auto update11 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update11.get(), kLayerId1);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update11))
                  .has_value());
  VerifyLayerExists(kLayerId1, false);

  // Update 12: Attempt to update kLayerId1 (removed).
  auto update12 = CreateDefaultUpdate();
  update12->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, gfx::Size(40, 40)));
  auto result12 = layer_context_impl_->DoUpdateDisplayTree(std::move(update12));
  ASSERT_FALSE(result12.has_value());
  EXPECT_EQ(result12.error(), "Invalid layer ID");

  VerifyLayerExists(kLayerId1, false);  // Should not be re-created

  // Test Case 5: Duplicate or non existent layer IDs in the Layer Order should
  // fail. Update 13: Create kLayerId1 again.
  auto update13 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update13.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update13))
                  .has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, gfx::Size(0, 0));
  VerifyLayerOrder({1, kLayerId1});

  // Update 14: Try to add another instance of kLayerId1 with different bounds.
  auto update14 = CreateDefaultUpdate();
  update14->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, gfx::Size(50, 50)));
  update14->layer_order = layer_order_;
  update14->layer_order->push_back(kLayerId1);

  auto result14 = layer_context_impl_->DoUpdateDisplayTree(std::move(update14));
  ASSERT_FALSE(result14.has_value());
  EXPECT_EQ(result14.error(), "Invalid or duplicate layer ID");

  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, gfx::Size(50, 50));  // Layer should be updated
  VerifyLayerOrder({1, kLayerId1});  // Layer Order should not update

  // Update 15: Try to add a Non Existent layer to Layer Order
  auto update15 = CreateDefaultUpdate();
  update15->layer_order = layer_order_;
  update15->layer_order->push_back(kNonExistentLayerId);

  auto result15 = layer_context_impl_->DoUpdateDisplayTree(std::move(update15));
  ASSERT_FALSE(result15.has_value());
  EXPECT_EQ(result15.error(), "Invalid or duplicate layer ID");

  // Test Case 7: Invalid Property Tree Indices on Creation
  // Update 16: Try to send a layer update with an invalid transform node index
  auto update16 = CreateDefaultUpdate();
  update16->layers.push_back(
      CreateManualLayer(kLayerId2, cc::mojom::LayerType::kLayer,
                        gfx::Size(10, 10), /*transform_idx=*/999));
  update16->layer_order = layer_order_;
  update16->layer_order->push_back(kLayerId2);
  EXPECT_FALSE(layer_context_impl_->DoUpdateDisplayTree(std::move(update16))
                   .has_value());
  VerifyLayerExists(kLayerId2, false);  // Should not have been created

  // Test Case 8: Layer Order Manipulation
  // Update 17: Re-Create 1, kLayerId1, kLayerId2. Order [1, kLayerId1,
  // kLayerId2]
  auto update17 = CreateDefaultUpdate();
  layer_order_.clear();
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer, 1);
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  AddDefaultLayerToUpdate(update17.get(), cc::mojom::LayerType::kLayer,
                          kLayerId2);
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update17))
                  .has_value());
  VerifyLayerOrder({1, kLayerId1, kLayerId2});

  // Update 18: Change order to [1, kLayerId2, kLayerId1]
  auto update18 = CreateDefaultUpdate();
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId2);
  layer_order_.push_back(kLayerId1);
  update18->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update18))
                  .has_value());
  VerifyLayerOrder({1, kLayerId2, kLayerId1});

  // Update 19: Create kLayerId3. Order [1, kLayerId2, kLayerId3, kLayerId1]
  auto update19 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update19.get(), cc::mojom::LayerType::kLayer,
                          kLayerId3);  // kLayerId3
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId2);
  layer_order_.push_back(kLayerId3);
  layer_order_.push_back(kLayerId1);
  update19->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update19))
                  .has_value());
  VerifyLayerOrder({1, kLayerId2, kLayerId3, kLayerId1});

  // Update 20: Remove kLayerId2. Order [1, kLayerId3, kLayerId1]
  auto update20 = CreateDefaultUpdate();
  layer_order_.clear();
  layer_order_.push_back(1);
  layer_order_.push_back(kLayerId3);
  layer_order_.push_back(kLayerId1);
  update20->layer_order = layer_order_;
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update20))
                  .has_value());
  VerifyLayerExists(kLayerId2, false);
  VerifyLayerOrder({1, kLayerId3, kLayerId1});
}

}  // namespace viz
