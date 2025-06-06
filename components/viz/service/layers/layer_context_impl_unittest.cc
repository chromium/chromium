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
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

// Default layer tree property values
const float kDefaultPageScaleFactor = 1.0f;
const float kDefaultMinPageScaleFactor = 0.5f;
const float kDefaultMaxPageScaleFactor = 2.0f;
const gfx::Rect kDefaultDeviceViewportRect(100, 100);
const SkColor4f kDefaultBackgroundColor = SkColors::kTransparent;
const float kDefaultExternalPageScaleFactor = 1.0f;
const float kDefaultDeviceScaleFactor = 1.0f;
const float kDefaultPaintedDeviceScaleFactor = 1.0f;
const FrameSinkId kDefaultFrameSinkId = FrameSinkId(1, 1);
const LocalSurfaceId kDefaultLocalSurfaceId(
    1,
    base::UnguessableToken::CreateForTesting(2u, 3u));
const SurfaceId kDefaultSurfaceId(kDefaultFrameSinkId, kDefaultLocalSurfaceId);
const SurfaceRange kDefaultSurfaceRange(std::nullopt, kDefaultSurfaceId);

// Default Layer property values
const gfx::Size kDefaultLayerBounds(10, 10);

// Default TextureLayer property values
const bool kDefaultBlendBackgroundColor = false;
const bool kDefaultForceTextureToOpaque = false;
const gfx::PointF kDefaultUVTopLeft = gfx::PointF();
const gfx::PointF kDefaultUVBottomRight = gfx::PointF(1.f, 1.f);

// Default SurfaceLayer property values
const uint32_t kDefaultDeadlineInFrames = 0u;
const bool kDefaultStretchContentToFillBounds = false;
const bool kDefaultSurfaceHitTestable = false;
const bool kDefaultHasPointerEventsNone = false;
const bool kDefaultIsReflection = false;
const bool kDefaultWillDrawNeedsReset = false;
const bool kDefaultOverrideChildPaintFlags = false;

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

  void ResetTestState() {
    // Property tree node IDs and layers are reinitialized in
    // CreateDefaultUpdate if first_update_ is true.
    first_update_ = true;
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
    update->device_viewport = kDefaultDeviceViewportRect;
    update->background_color = kDefaultBackgroundColor;

    // Valid scale factors by default
    update->page_scale_factor = kDefaultPageScaleFactor;
    update->min_page_scale_factor = kDefaultMinPageScaleFactor;
    update->max_page_scale_factor = kDefaultMaxPageScaleFactor;
    update->external_page_scale_factor = kDefaultExternalPageScaleFactor;
    update->device_scale_factor = kDefaultDeviceScaleFactor;
    update->painted_device_scale_factor = kDefaultPaintedDeviceScaleFactor;

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
    update->local_surface_id_from_parent = kDefaultLocalSurfaceId;

    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeDelta interval = base::Milliseconds(16);
    update->begin_frame_args = BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, BeginFrameArgs::kStartingSourceId,
        BeginFrameArgs::kStartingFrameNumber, now, now + interval, interval,
        BeginFrameArgs::NORMAL);
  }

  void AddFirstTimeDefaultProperties(mojom::LayerTreeUpdate* update) {
    // Set internal state to defaults.
    next_layer_id_ = 1;
    next_transform_id_ = 0;
    next_clip_id_ = 0;
    next_effect_id_ = 0;
    next_scroll_id_ = 0;
    layer_order_.clear();

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

  mojom::LayerExtraPtr CreateDefaultLayerExtra(cc::mojom::LayerType type) {
    switch (type) {
      case cc::mojom::LayerType::kTexture: {
        auto extra = mojom::TextureLayerExtra::New();
        extra->blend_background_color = kDefaultBlendBackgroundColor;
        extra->force_texture_to_opaque = kDefaultForceTextureToOpaque;
        extra->uv_top_left = kDefaultUVTopLeft;
        extra->uv_bottom_right = kDefaultUVBottomRight;
        return mojom::LayerExtra::NewTextureLayerExtra(std::move(extra));
      }
      case cc::mojom::LayerType::kSurface: {
        auto extra = mojom::SurfaceLayerExtra::New();
        extra->surface_range = kDefaultSurfaceRange;
        extra->deadline_in_frames = kDefaultDeadlineInFrames;
        extra->stretch_content_to_fill_bounds =
            kDefaultStretchContentToFillBounds;
        extra->surface_hit_testable = kDefaultSurfaceHitTestable;
        extra->has_pointer_events_none = kDefaultHasPointerEventsNone;
        extra->is_reflection = kDefaultIsReflection;
        extra->will_draw_needs_reset = kDefaultWillDrawNeedsReset;
        extra->override_child_paint_flags = kDefaultOverrideChildPaintFlags;
        return mojom::LayerExtra::NewSurfaceLayerExtra(std::move(extra));
      }

      default:
        // TODO(vmiura): Add each layer type's initialization.
        return nullptr;
    }
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
    layer->bounds = kDefaultLayerBounds;
    layer->layer_extra = CreateDefaultLayerExtra(type);

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
  FakeCompositorFrameSinkClient dummy_client_;
  FrameSinkManagerImpl frame_sink_manager_;

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

class LayerContextImplUpdateDisplayTreeTransformNodeTest
    : public LayerContextImplTest {
 protected:
  cc::TransformNode* GetTransformNodeFromActiveTree(int node_id) {
    if (node_id < static_cast<int>(layer_context_impl_->host_impl()
                                       ->active_tree()
                                       ->property_trees()
                                       ->transform_tree()
                                       .size())) {
      return layer_context_impl_->host_impl()
          ->active_tree()
          ->property_trees()
          ->transform_tree_mutable()
          .Node(node_id);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       UpdateExistingTransformNodeProperties) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  // Keep parent_id same as default.
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->local = gfx::Transform::MakeScale(2.0f);
  node_update->origin = gfx::Point3F(1.f, 2.f, 3.f);
  node_update->post_translation = gfx::Vector2dF(10.f, 20.f);
  node_update->scroll_offset = gfx::PointF(5.f, 6.f);
  node_update->sorting_context_id = 1;
  node_update->flattens_inherited_transform = true;
  node_update->will_change_transform = true;
  node_update->damage_reasons_bit_mask =
      (cc::DamageReasonSet{cc::DamageReason::kUntracked}).ToEnumBitmask();
  node_update->moved_by_safe_area_bottom = true;

  update2->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_TRUE(result.has_value());

  cc::TransformNode* node_impl =
      GetTransformNodeFromActiveTree(cc::kSecondaryRootPropertyNodeId);
  ASSERT_TRUE(node_impl);
  EXPECT_EQ(node_impl->local, gfx::Transform::MakeScale(2.0f));
  EXPECT_EQ(node_impl->origin, gfx::Point3F(1.f, 2.f, 3.f));
  EXPECT_EQ(node_impl->post_translation, gfx::Vector2dF(10.f, 20.f));
  EXPECT_EQ(node_impl->scroll_offset(), gfx::PointF(5.f, 6.f));
  EXPECT_EQ(node_impl->sorting_context_id, 1);
  EXPECT_TRUE(node_impl->flattens_inherited_transform);
  EXPECT_TRUE(node_impl->will_change_transform);
  EXPECT_TRUE(node_impl->damage_reasons().Has(cc::DamageReason::kUntracked));
  EXPECT_TRUE(node_impl->moved_by_safe_area_bottom);
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       AddRemoveTransformNodes) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  uint32_t initial_node_count = layer_context_impl_->host_impl()
                                    ->active_tree()
                                    ->property_trees()
                                    ->transform_tree()
                                    .nodes()
                                    .size();

  // Add a new node.
  auto update_add = CreateDefaultUpdate();
  int new_node_id =
      AddTransformNode(update_add.get(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(update_add->num_transform_nodes, initial_node_count + 1);

  auto result_add =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add));
  ASSERT_TRUE(result_add.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->transform_tree()
                .nodes()
                .size(),
            initial_node_count + 1);
  cc::TransformNode* added_node_impl =
      GetTransformNodeFromActiveTree(new_node_id);
  ASSERT_TRUE(added_node_impl);
  EXPECT_EQ(added_node_impl->parent_id, cc::kSecondaryRootPropertyNodeId);

  // Remove the added node.
  auto update_remove = CreateDefaultUpdate();
  update_remove->num_transform_nodes = initial_node_count;
  // To remove, we just send fewer nodes in num_transform_nodes.
  // The actual nodes in transform_nodes vector can be empty or partial.
  // Here we send an empty list for simplicity.
  update_remove->transform_nodes.clear();

  auto result_remove =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove));
  ASSERT_TRUE(result_remove.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->transform_tree()
                .nodes()
                .size(),
            initial_node_count);
  EXPECT_FALSE(GetTransformNodeFromActiveTree(new_node_id));
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       UpdateTransformTreeProperties) {
  auto update = CreateDefaultUpdate();
  auto tree_props = mojom::TransformTreeUpdate::New();
  tree_props->page_scale_factor = 1.5f;
  tree_props->device_scale_factor = 2.0f;
  tree_props->device_transform_scale_factor = 2.5f;
  tree_props->nodes_affected_by_outer_viewport_bounds_delta = {
      cc::kSecondaryRootPropertyNodeId};
  tree_props->nodes_affected_by_safe_area_bottom = {
      cc::kSecondaryRootPropertyNodeId};
  update->transform_tree_update = std::move(tree_props);

  // The top level page_scale_factor overrides whatever we set
  // in the transform tree, so set it to the same value.
  // TODO(vmiura): See if we could just remove syncing the
  // transform tree scale factors?
  update->page_scale_factor = 1.5f;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& transform_tree = layer_context_impl_->host_impl()
                                   ->active_tree()
                                   ->property_trees()
                                   ->transform_tree();
  EXPECT_EQ(transform_tree.page_scale_factor(), 1.5f);
  EXPECT_EQ(transform_tree.device_scale_factor(), 2.0f);
  EXPECT_EQ(transform_tree.device_transform_scale_factor(), 2.5f);
  EXPECT_THAT(transform_tree.nodes_affected_by_outer_viewport_bounds_delta(),
              testing::ElementsAre(cc::kSecondaryRootPropertyNodeId));
  EXPECT_THAT(transform_tree.nodes_affected_by_safe_area_bottom(),
              testing::ElementsAre(cc::kSecondaryRootPropertyNodeId));
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       StickyPositionDataValid) {
  auto update = CreateDefaultUpdate();
  int scroll_node_id = AddScrollNode(update.get(), cc::kRootPropertyNodeId);

  auto tree_props = mojom::TransformTreeUpdate::New();
  auto sticky_data = mojom::StickyPositionNodeData::New();
  sticky_data->scroll_ancestor = scroll_node_id;
  sticky_data->is_anchored_top = true;
  sticky_data->top_offset = 10.f;
  tree_props->sticky_position_data.push_back(std::move(sticky_data));
  update->transform_tree_update = std::move(tree_props);

  // Add a transform node that will use this sticky data.
  auto transform_node_update = mojom::TransformNode::New();
  transform_node_update->id =
      AddTransformNode(update.get(), cc::kRootPropertyNodeId);
  transform_node_update->parent_id = cc::kRootPropertyNodeId;
  transform_node_update->sticky_position_constraint_id = 0;
  update->transform_nodes.push_back(std::move(transform_node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& transform_tree = layer_context_impl_->host_impl()
                                   ->active_tree()
                                   ->property_trees()
                                   ->transform_tree();
  ASSERT_EQ(transform_tree.sticky_position_data().size(), 1u);
  EXPECT_EQ(transform_tree.sticky_position_data()[0].scroll_ancestor,
            scroll_node_id);
  EXPECT_TRUE(
      transform_tree.sticky_position_data()[0].constraints.is_anchored_top);
  EXPECT_EQ(transform_tree.sticky_position_data()[0].constraints.top_offset,
            10.f);
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       StickyPositionDataInvalidScrollAncestor) {
  auto update = CreateDefaultUpdate();
  auto tree_props = mojom::TransformTreeUpdate::New();
  auto sticky_data = mojom::StickyPositionNodeData::New();
  sticky_data->scroll_ancestor = 99;  // Invalid scroll node ID
  tree_props->sticky_position_data.push_back(std::move(sticky_data));
  update->transform_tree_update = std::move(tree_props);

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid scroll ancestor ID");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       AnchorPositionDataValid) {
  auto update = CreateDefaultUpdate();
  int adjustment_container_id =
      AddTransformNode(update.get(), cc::kRootPropertyNodeId);

  auto tree_props = mojom::TransformTreeUpdate::New();
  auto anchor_data = mojom::AnchorPositionScrollData::New();
  anchor_data->adjustment_container_ids.push_back(
      cc::ElementId(adjustment_container_id));
  anchor_data->accumulated_scroll_origin = gfx::Vector2d(5, 5);
  tree_props->anchor_position_scroll_data.push_back(std::move(anchor_data));
  update->transform_tree_update = std::move(tree_props);

  // Add a transform node that will use this anchor data.
  auto transform_node_update = mojom::TransformNode::New();
  transform_node_update->id =
      AddTransformNode(update.get(), cc::kRootPropertyNodeId);
  transform_node_update->parent_id = cc::kRootPropertyNodeId;
  transform_node_update->anchor_position_scroll_data_id = 0;
  update->transform_nodes.push_back(std::move(transform_node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& transform_tree = layer_context_impl_->host_impl()
                                   ->active_tree()
                                   ->property_trees()
                                   ->transform_tree();
  ASSERT_EQ(transform_tree.anchor_position_scroll_data().size(), 1u);
  EXPECT_THAT(
      transform_tree.anchor_position_scroll_data()[0].adjustment_container_ids,
      testing::ElementsAre(cc::ElementId(adjustment_container_id)));
  EXPECT_EQ(
      transform_tree.anchor_position_scroll_data()[0].accumulated_scroll_origin,
      gfx::Vector2d(5, 5));
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidTransformNodeParentId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = next_transform_id_++;  // New node
  node_update->parent_id = 99;             // Invalid parent ID
  update->transform_nodes.push_back(std::move(node_update));
  update->num_transform_nodes = next_transform_id_;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid property tree node parent_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidTransformNodeIdOnUpdate) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = 99;  // Invalid node ID to update
  node_update->parent_id = cc::kRootPropertyNodeId;
  update->transform_nodes.push_back(std::move(node_update));
  // num_transform_nodes remains the same as default.

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid property tree node ID");
}

class LayerContextImplUpdateDisplayTreeClipNodeTest
    : public LayerContextImplTest {
 protected:
  cc::ClipNode* GetClipNodeFromActiveTree(int node_id) {
    if (node_id < static_cast<int>(layer_context_impl_->host_impl()
                                       ->active_tree()
                                       ->property_trees()
                                       ->clip_tree()
                                       .size())) {
      return layer_context_impl_->host_impl()
          ->active_tree()
          ->property_trees()
          ->clip_tree_mutable()
          .Node(node_id);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeClipNodeTest,
       UpdateExistingClipNodeProperties) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::ClipNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  // Keep parent_id same as default.
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->clip = gfx::RectF(10.f, 20.f, 30.f, 40.f);
  // Use a valid existing transform node ID.
  node_update->transform_id = cc::kSecondaryRootPropertyNodeId;
  update2->clip_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_TRUE(result.has_value());

  cc::ClipNode* node_impl =
      GetClipNodeFromActiveTree(cc::kSecondaryRootPropertyNodeId);
  ASSERT_TRUE(node_impl);
  EXPECT_EQ(node_impl->clip, gfx::RectF(10.f, 20.f, 30.f, 40.f));
  EXPECT_EQ(node_impl->transform_id, cc::kSecondaryRootPropertyNodeId);
}

TEST_F(LayerContextImplUpdateDisplayTreeClipNodeTest, AddRemoveClipNodes) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  uint32_t initial_node_count = layer_context_impl_->host_impl()
                                    ->active_tree()
                                    ->property_trees()
                                    ->clip_tree()
                                    .nodes()
                                    .size();

  // Add a new node.
  auto update_add = CreateDefaultUpdate();
  int new_node_id =
      AddClipNode(update_add.get(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(update_add->num_clip_nodes, initial_node_count + 1);

  auto result_add =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add));
  ASSERT_TRUE(result_add.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->clip_tree()
                .nodes()
                .size(),
            initial_node_count + 1);
  cc::ClipNode* added_node_impl = GetClipNodeFromActiveTree(new_node_id);
  ASSERT_TRUE(added_node_impl);
  EXPECT_EQ(added_node_impl->parent_id, cc::kSecondaryRootPropertyNodeId);

  // Remove the added node.
  auto update_remove = CreateDefaultUpdate();
  update_remove->num_clip_nodes = initial_node_count;
  update_remove->clip_nodes.clear();

  auto result_remove =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove));
  ASSERT_TRUE(result_remove.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->clip_tree()
                .nodes()
                .size(),
            initial_node_count);
  EXPECT_FALSE(GetClipNodeFromActiveTree(new_node_id));
}

TEST_F(LayerContextImplUpdateDisplayTreeClipNodeTest, InvalidClipNodeParentId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ClipNode::New();
  node_update->id = next_clip_id_++;  // New node
  node_update->parent_id = 99;        // Invalid parent ID
  node_update->transform_id = cc::kRootPropertyNodeId;
  update->clip_nodes.push_back(std::move(node_update));
  update->num_clip_nodes = next_clip_id_;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid property tree node parent_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeClipNodeTest,
       InvalidClipNodeTransformId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ClipNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = 99;  // Invalid transform ID
  update->clip_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid transform_id for clip node");
}

class LayerContextImplUpdateDisplayTreeEffectNodeTest
    : public LayerContextImplTest {
 protected:
  cc::EffectNode* GetEffectNodeFromActiveTree(int node_id) {
    if (node_id < static_cast<int>(layer_context_impl_->host_impl()
                                       ->active_tree()
                                       ->property_trees()
                                       ->effect_tree()
                                       .size())) {
      return layer_context_impl_->host_impl()
          ->active_tree()
          ->property_trees()
          ->effect_tree_mutable()
          .Node(node_id);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       UpdateExistingEffectNodeProperties) {
  // Apply a default valid update, with a new effect node.
  auto update1 = CreateDefaultUpdate();
  int effect_node_id =
      AddEffectNode(update1.get(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = effect_node_id;
  // Keep parent_id same as default.
  node_update->parent_id = cc::kSecondaryRootPropertyNodeId;
  node_update->opacity = 0.5f;
  node_update->filters.Append(cc::FilterOperation::CreateBlurFilter(2.f));
  node_update->backdrop_filters.Append(
      cc::FilterOperation::CreateGrayscaleFilter(0.8f));
  node_update->blend_mode = static_cast<uint32_t>(SkBlendMode::kMultiply);
  node_update->render_surface_reason = cc::RenderSurfaceReason::kTest;

  // TODO(vmiura): If we have a render_surface_reason, without a valid
  // element_id, we can trigger crashes during property tree update. Fix that.
  node_update->element_id = cc::ElementId(42);

  node_update->cache_render_surface = true;

  const auto view_transition_token = blink::ViewTransitionToken();
  node_update->view_transition_element_resource_id =
      ViewTransitionElementResourceId(view_transition_token, 1, false);
  // Use valid existing transform and clip node IDs.
  node_update->transform_id = cc::kSecondaryRootPropertyNodeId;
  node_update->clip_id = cc::kSecondaryRootPropertyNodeId;
  node_update->target_id = cc::kRootPropertyNodeId;

  update2->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_TRUE(result.has_value());

  cc::EffectNode* node_impl = GetEffectNodeFromActiveTree(effect_node_id);
  ASSERT_TRUE(node_impl);
  EXPECT_EQ(node_impl->opacity, 0.5f);
  EXPECT_EQ(node_impl->filters.size(), 1u);
  EXPECT_EQ(node_impl->filters.at(0).type(),
            cc::FilterOperation::FilterType::BLUR);
  EXPECT_EQ(node_impl->backdrop_filters.size(), 1u);
  EXPECT_EQ(node_impl->backdrop_filters.at(0).type(),
            cc::FilterOperation::FilterType::GRAYSCALE);
  EXPECT_EQ(node_impl->blend_mode, SkBlendMode::kMultiply);
  EXPECT_EQ(node_impl->render_surface_reason, cc::RenderSurfaceReason::kTest);
  EXPECT_TRUE(node_impl->cache_render_surface);
  EXPECT_EQ(node_impl->view_transition_element_resource_id,
            ViewTransitionElementResourceId(view_transition_token, 1, false));
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest, AddRemoveEffectNodes) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  uint32_t initial_node_count = layer_context_impl_->host_impl()
                                    ->active_tree()
                                    ->property_trees()
                                    ->effect_tree()
                                    .nodes()
                                    .size();

  // Add a new node.
  auto update_add = CreateDefaultUpdate();
  int new_node_id =
      AddEffectNode(update_add.get(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(update_add->num_effect_nodes, initial_node_count + 1);

  auto result_add =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add));
  ASSERT_TRUE(result_add.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->effect_tree()
                .nodes()
                .size(),
            initial_node_count + 1);
  cc::EffectNode* added_node_impl = GetEffectNodeFromActiveTree(new_node_id);
  ASSERT_TRUE(added_node_impl);
  EXPECT_EQ(added_node_impl->parent_id, cc::kSecondaryRootPropertyNodeId);

  // Remove the added node.
  auto update_remove = CreateDefaultUpdate();
  update_remove->num_effect_nodes = initial_node_count;
  update_remove->effect_nodes.clear();

  auto result_remove =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove));
  ASSERT_TRUE(result_remove.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->effect_tree()
                .nodes()
                .size(),
            initial_node_count);
  EXPECT_FALSE(GetEffectNodeFromActiveTree(new_node_id));
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       AddRemoveCopyOutputRequests) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  // Add a copy request.
  auto update_add_request = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = cc::kSecondaryRootPropertyNodeId;
  node_update->clip_id = cc::kSecondaryRootPropertyNodeId;
  node_update->target_id = cc::kRootPropertyNodeId;
  node_update->copy_output_requests.push_back(
      CopyOutputRequest::CreateStubForTesting());
  update_add_request->effect_nodes.push_back(std::move(node_update));

  auto result_add =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add_request));
  ASSERT_TRUE(result_add.has_value());
  auto copy_requests = layer_context_impl_->host_impl()
                           ->active_tree()
                           ->property_trees()
                           ->effect_tree_mutable()
                           .TakeCopyRequests();
  EXPECT_EQ(copy_requests.count(cc::kSecondaryRootPropertyNodeId), 1u);

  // Remove the copy request (by not sending it).
  auto update_remove_request = CreateDefaultUpdate();
  auto node_update_no_request = mojom::EffectNode::New();
  node_update_no_request->id = cc::kSecondaryRootPropertyNodeId;
  node_update_no_request->parent_id = cc::kRootPropertyNodeId;
  node_update_no_request->transform_id = cc::kSecondaryRootPropertyNodeId;
  node_update_no_request->clip_id = cc::kSecondaryRootPropertyNodeId;
  node_update_no_request->target_id = cc::kRootPropertyNodeId;
  update_remove_request->effect_nodes.push_back(
      std::move(node_update_no_request));

  auto result_remove = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_remove_request));
  ASSERT_TRUE(result_remove.has_value());
  auto copy_requests_after_remove = layer_context_impl_->host_impl()
                                        ->active_tree()
                                        ->property_trees()
                                        ->effect_tree_mutable()
                                        .TakeCopyRequests();
  EXPECT_EQ(copy_requests_after_remove.count(cc::kSecondaryRootPropertyNodeId),
            0u);
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeParentId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = next_effect_id_++;  // New node
  node_update->parent_id = 99;          // Invalid parent ID
  node_update->transform_id = cc::kRootPropertyNodeId;
  node_update->clip_id = cc::kRootPropertyNodeId;
  node_update->target_id = cc::kRootPropertyNodeId;
  update->effect_nodes.push_back(std::move(node_update));
  update->num_effect_nodes = next_effect_id_;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid property tree node parent_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeTransformId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = 99;  // Invalid transform ID
  node_update->clip_id = cc::kRootPropertyNodeId;
  node_update->target_id = cc::kRootPropertyNodeId;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid transform_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeClipId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = cc::kRootPropertyNodeId;
  node_update->clip_id = 99;  // Invalid clip ID
  node_update->target_id = cc::kRootPropertyNodeId;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid clip_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeTargetId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = cc::kRootPropertyNodeId;
  node_update->clip_id = cc::kRootPropertyNodeId;
  node_update->target_id = 99;  // Invalid target ID
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid target_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest, InvalidBlendMode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = cc::kRootPropertyNodeId;
  node_update->clip_id = cc::kRootPropertyNodeId;
  node_update->target_id = cc::kRootPropertyNodeId;
  node_update->blend_mode = 999;  // Invalid blend mode
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid blend_mode for effect node");
}

class LayerContextImplUpdateDisplayTreeScrollNodeTest
    : public LayerContextImplTest {
 protected:
  cc::ScrollNode* GetScrollNodeFromActiveTree(int node_id) {
    if (node_id < static_cast<int>(layer_context_impl_->host_impl()
                                       ->active_tree()
                                       ->property_trees()
                                       ->scroll_tree()
                                       .size())) {
      return layer_context_impl_->host_impl()
          ->active_tree()
          ->property_trees()
          ->scroll_tree_mutable()
          .Node(node_id);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       UpdateExistingScrollNodeProperties) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::ScrollNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  // Keep parent_id same as default.
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->container_bounds = gfx::Size(50, 60);
  node_update->bounds = gfx::Size(70, 80);
  node_update->user_scrollable_horizontal = true;
  node_update->user_scrollable_vertical = true;
  node_update->element_id = cc::ElementId(123);
  // Use a valid existing transform node ID.
  node_update->transform_id = cc::kSecondaryRootPropertyNodeId;
  update2->scroll_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_TRUE(result.has_value());

  cc::ScrollNode* node_impl =
      GetScrollNodeFromActiveTree(cc::kSecondaryRootPropertyNodeId);
  ASSERT_TRUE(node_impl);
  EXPECT_EQ(node_impl->container_bounds, gfx::Size(50, 60));
  EXPECT_EQ(node_impl->bounds, gfx::Size(70, 80));
  EXPECT_TRUE(node_impl->user_scrollable_horizontal);
  EXPECT_TRUE(node_impl->user_scrollable_vertical);
  EXPECT_EQ(node_impl->element_id, cc::ElementId(123));
  EXPECT_EQ(node_impl->transform_id, cc::kSecondaryRootPropertyNodeId);
}

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest, AddRemoveScrollNodes) {
  // Apply a default valid update first.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  uint32_t initial_node_count = layer_context_impl_->host_impl()
                                    ->active_tree()
                                    ->property_trees()
                                    ->scroll_tree()
                                    .nodes()
                                    .size();

  // Add a new node.
  auto update_add = CreateDefaultUpdate();
  int new_node_id =
      AddScrollNode(update_add.get(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(update_add->num_scroll_nodes, initial_node_count + 1);

  auto result_add =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_add));
  ASSERT_TRUE(result_add.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->scroll_tree()
                .nodes()
                .size(),
            initial_node_count + 1);
  cc::ScrollNode* added_node_impl = GetScrollNodeFromActiveTree(new_node_id);
  ASSERT_TRUE(added_node_impl);
  EXPECT_EQ(added_node_impl->parent_id, cc::kSecondaryRootPropertyNodeId);

  // Remove the added node.
  auto update_remove = CreateDefaultUpdate();
  update_remove->num_scroll_nodes = initial_node_count;
  update_remove->scroll_nodes.clear();

  auto result_remove =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove));
  ASSERT_TRUE(result_remove.has_value());
  EXPECT_EQ(layer_context_impl_->host_impl()
                ->active_tree()
                ->property_trees()
                ->scroll_tree()
                .nodes()
                .size(),
            initial_node_count);
  EXPECT_FALSE(GetScrollNodeFromActiveTree(new_node_id));
}

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       InvalidScrollNodeParentId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ScrollNode::New();
  node_update->id = next_scroll_id_++;  // New node
  node_update->parent_id = 99;          // Invalid parent ID
  node_update->transform_id = cc::kRootPropertyNodeId;
  update->scroll_nodes.push_back(std::move(node_update));
  update->num_scroll_nodes = next_scroll_id_;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid property tree node parent_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       InvalidScrollNodeTransformId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ScrollNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Existing node
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->transform_id = 99;  // Invalid transform ID
  update->scroll_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid transform_id for scroll node");
}

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       UpdateScrollTreeProperties) {
  auto update = CreateDefaultUpdate();
  auto tree_props = mojom::ScrollTreeUpdate::New();
  cc::ElementId element_id(123);
  tree_props->synced_scroll_offsets[element_id] =
      base::MakeRefCounted<cc::SyncedScrollOffset>();
  tree_props->synced_scroll_offsets[element_id]->SetCurrent(
      gfx::PointF(10.f, 20.f));
  tree_props->scrolling_contents_cull_rects[element_id] =
      gfx::Rect(5, 5, 15, 15);
  update->scroll_tree_update = std::move(tree_props);

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& scroll_tree = layer_context_impl_->host_impl()
                                ->active_tree()
                                ->property_trees()
                                ->scroll_tree();
  EXPECT_EQ(scroll_tree.synced_scroll_offset_map()
                .at(element_id)
                ->Current(
                    /*is_active_tree=*/true),
            gfx::PointF(10.f, 20.f));
  EXPECT_EQ(scroll_tree.scrolling_contents_cull_rects().at(element_id),
            gfx::Rect(5, 5, 15, 15));
}

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       EmptyScrollingContentsCullRectsByDefault) {
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
      const gfx::Size& bounds = kDefaultLayerBounds,
      int transform_idx = cc::kSecondaryRootPropertyNodeId,
      int clip_idx = cc::kRootPropertyNodeId,
      int effect_idx = cc::kSecondaryRootPropertyNodeId,
      int scroll_idx = cc::kSecondaryRootPropertyNodeId) {
    auto layer = mojom::Layer::New();
    layer->id = id;
    layer->type = type;
    layer->bounds = bounds;
    layer->transform_tree_index = transform_idx;
    layer->clip_tree_index = clip_idx;
    layer->effect_tree_index = effect_idx;
    layer->scroll_tree_index = scroll_idx;
    layer->layer_extra = CreateDefaultLayerExtra(type);
    return layer;
  }
};

TEST_F(LayerContextImplLayerLifecycleTest, LayerLifecycleAndEdgeCases) {
  constexpr int kLayerId1 = 2;  // Start after default root layer (ID 1).
  constexpr int kLayerId2 = 3;
  constexpr int kLayerId3 = 4;
  constexpr int kNonExistentLayerId = 99;

  // Test Case 1: Basic Layer Lifecycle (Create, Update Bounds, Remove)
  // Update 1: Create Layer ID kLayerId1.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kLayer,
                          kLayerId1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kDefaultLayerBounds);  // Default bounds

  // Update 2: Update bounds of Layer ID kLayerId1.
  auto update2 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds1(20, 20);
  update2->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds1));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kUpdatedBounds1);

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
  const gfx::Size kUpdatedBounds2(30, 30);
  update6->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update6)).has_value());
  VerifyLayerBounds(kLayerId1, kUpdatedBounds2);
  VerifyLayerBounds(kLayerId2, kDefaultLayerBounds);  // Unaffected

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
  const gfx::Size kUpdatedBounds3(5, 5);
  update10->layers.push_back(
      CreateManualLayer(kNonExistentLayerId, cc::mojom::LayerType::kLayer,
                        kUpdatedBounds3));  // Update non-existent
  auto result10 = layer_context_impl_->DoUpdateDisplayTree(std::move(update10));
  ASSERT_FALSE(result10.has_value());
  EXPECT_EQ(result10.error(), "Invalid layer ID");
  VerifyLayerExists(kLayerId1, true);  // Unaffected
  VerifyLayerBounds(
      kLayerId1,
      kDefaultLayerBounds);  // Should be reset to default or last valid
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
  const gfx::Size kUpdatedBounds4(40, 40);
  update12->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds4));
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
  VerifyLayerBounds(kLayerId1, kDefaultLayerBounds);
  VerifyLayerOrder({1, kLayerId1});

  // Update 14: Try to add another instance of kLayerId1 with different bounds.
  auto update14 = CreateDefaultUpdate();
  const gfx::Size kUpdatedBounds5(50, 50);
  update14->layers.push_back(CreateManualLayer(
      kLayerId1, cc::mojom::LayerType::kLayer, kUpdatedBounds5));
  update14->layer_order = layer_order_;
  update14->layer_order->push_back(kLayerId1);

  auto result14 = layer_context_impl_->DoUpdateDisplayTree(std::move(update14));
  ASSERT_FALSE(result14.has_value());
  EXPECT_EQ(result14.error(), "Invalid or duplicate layer ID");
  VerifyLayerExists(kLayerId1, true);
  VerifyLayerBounds(kLayerId1, kUpdatedBounds5);  // Layer should be updated
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
                        kDefaultLayerBounds, /*transform_idx=*/999));
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

TEST_F(LayerContextImplLayerLifecycleTest, CreateLayersOfAllTypes) {
  auto update = CreateDefaultUpdate();

  // Test a subset of layer types that have distinct LayerImpl classes or
  // specific handling in CreateLayer.
  const std::vector<cc::mojom::LayerType> types_to_test = {
      cc::mojom::LayerType::kLayer,
      cc::mojom::LayerType::kMirror,
      cc::mojom::LayerType::kNinePatchThumbScrollbar,
      cc::mojom::LayerType::kPaintedScrollbar,
      cc::mojom::LayerType::kTileDisplay,
      cc::mojom::LayerType::kSolidColorScrollbar,
      cc::mojom::LayerType::kSurface,
      cc::mojom::LayerType::kTexture,
      cc::mojom::LayerType::kViewTransitionContent,
      // Add other relevant types here.
  };

  std::vector<int> layer_ids;
  for (cc::mojom::LayerType type : types_to_test) {
    int layer_id = next_layer_id_++;
    layer_ids.push_back(layer_id);
    auto layer = CreateManualLayer(layer_id, type);
    switch (type) {
      case cc::mojom::LayerType::kMirror: {
        auto extra = mojom::MirrorLayerExtra::New();
        // Mirroring the root layer (ID 1) by default for simplicity.
        extra->mirrored_layer_id = 1;
        layer->layer_extra =
            mojom::LayerExtra::NewMirrorLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kNinePatchThumbScrollbar: {
        auto extra = mojom::NinePatchThumbScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewNinePatchThumbScrollbarLayerExtra(
                std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kPaintedScrollbar: {
        auto extra = mojom::PaintedScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewPaintedScrollbarLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kSolidColorScrollbar: {
        auto extra = mojom::SolidColorScrollbarLayerExtra::New();
        extra->scrollbar_base_extra = mojom::ScrollbarLayerBaseExtra::New();
        layer->layer_extra =
            mojom::LayerExtra::NewSolidColorScrollbarLayerExtra(
                std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kSurface: {
        auto extra = mojom::SurfaceLayerExtra::New();
        extra->surface_range = kDefaultSurfaceRange;
        extra->deadline_in_frames = 0u;
        layer->layer_extra =
            mojom::LayerExtra::NewSurfaceLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kTexture: {
        auto extra = mojom::TextureLayerExtra::New();
        // TextureLayer can have an optional TransferableResource.
        // For this basic creation test, leaving it null is fine.
        layer->layer_extra =
            mojom::LayerExtra::NewTextureLayerExtra(std::move(extra));
        break;
      }
      case cc::mojom::LayerType::kViewTransitionContent: {
        auto extra = mojom::ViewTransitionContentLayerExtra::New();
        extra->resource_id = ViewTransitionElementResourceId(
            blink::ViewTransitionToken(), 1, false);
        layer->layer_extra =
            mojom::LayerExtra::NewViewTransitionContentLayerExtra(
                std::move(extra));
        break;
      }
      default:
        // No layer_extra needed for other types in this test.
        break;
    }
    update->layers.push_back(std::move(layer));
    layer_order_.push_back(layer_id);
  }
  update->layer_order = layer_order_;

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  for (size_t i = 0; i < types_to_test.size(); ++i) {
    VerifyLayerExists(layer_ids[i], true);
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_ids[i]);
    ASSERT_NE(nullptr, layer);
    EXPECT_EQ(layer->GetLayerType(), types_to_test[i]);
  }
}

TEST_F(LayerContextImplLayerLifecycleTest, UpdateMultipleLayerProperties) {
  const gfx::Size kUpdatedBounds(50, 50);

  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  auto update_props = CreateDefaultUpdate();
  auto layer1_props = CreateManualLayer(layer_id1);
  layer1_props->bounds = kUpdatedBounds;
  layer1_props->contents_opaque = true;
  layer1_props->contents_opaque_for_text = true;
  layer1_props->background_color = SkColors::kRed;
  layer1_props->transform_tree_index = cc::kRootPropertyNodeId;
  update_props->layers.push_back(std::move(layer1_props));

  auto layer2_props = CreateManualLayer(layer_id2);
  layer2_props->is_drawable = false;
  layer2_props->clip_tree_index = cc::kSecondaryRootPropertyNodeId;
  layer2_props->effect_tree_index = cc::kRootPropertyNodeId;
  update_props->layers.push_back(std::move(layer2_props));

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_props))
                  .has_value());

  cc::LayerImpl* layer1_impl = GetLayerFromActiveTree(layer_id1);
  ASSERT_NE(nullptr, layer1_impl);
  EXPECT_EQ(layer1_impl->bounds(), kUpdatedBounds);
  EXPECT_TRUE(layer1_impl->contents_opaque());
  EXPECT_TRUE(layer1_impl->contents_opaque_for_text());
  EXPECT_EQ(layer1_impl->background_color(), SkColors::kRed);
  EXPECT_EQ(layer1_impl->transform_tree_index(), cc::kRootPropertyNodeId);

  cc::LayerImpl* layer2_impl = GetLayerFromActiveTree(layer_id2);
  ASSERT_NE(nullptr, layer2_impl);
  EXPECT_FALSE(layer2_impl->draws_content());
  EXPECT_EQ(layer2_impl->clip_tree_index(), cc::kSecondaryRootPropertyNodeId);
  EXPECT_EQ(layer2_impl->effect_tree_index(), cc::kRootPropertyNodeId);
}

TEST_F(LayerContextImplLayerLifecycleTest, ReorderLayers) {
  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  int layer_id3 = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());
  VerifyLayerOrder({1, layer_id1, layer_id2, layer_id3});

  // Move layer_id1 to the end.
  auto update_reorder1 = CreateDefaultUpdate();
  layer_order_ = {1, layer_id2, layer_id3, layer_id1};
  update_reorder1->layer_order = layer_order_;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_reorder1))
          .has_value());
  VerifyLayerOrder({1, layer_id2, layer_id3, layer_id1});

  // Move layer_id3 to the beginning (after root).
  auto update_reorder2 = CreateDefaultUpdate();
  layer_order_ = {1, layer_id3, layer_id2, layer_id1};
  update_reorder2->layer_order = layer_order_;
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_reorder2))
          .has_value());
  VerifyLayerOrder({1, layer_id3, layer_id2, layer_id1});
}

TEST_F(LayerContextImplLayerLifecycleTest, RemoveLayers) {
  auto update = CreateDefaultUpdate();
  int layer_id1 = AddDefaultLayerToUpdate(update.get());
  int layer_id2 = AddDefaultLayerToUpdate(update.get());
  int layer_id3 = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());
  VerifyLayerOrder({1, layer_id1, layer_id2, layer_id3});

  // Remove from the middle.
  auto update_remove1 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove1.get(), layer_id2);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove1))
          .has_value());
  VerifyLayerExists(layer_id2, false);
  VerifyLayerOrder({1, layer_id1, layer_id3});

  // Remove from the beginning (after root).
  auto update_remove2 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove2.get(), layer_id1);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove2))
          .has_value());
  VerifyLayerExists(layer_id1, false);
  VerifyLayerOrder({1, layer_id3});

  // Remove from the end.
  auto update_remove3 = CreateDefaultUpdate();
  RemoveLayerInUpdate(update_remove3.get(), layer_id3);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_remove3))
          .has_value());
  VerifyLayerExists(layer_id3, false);
  VerifyLayerOrder({1});
}

TEST_F(LayerContextImplLayerLifecycleTest, LayerPropertyChangedFlags) {
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  // Test layer_property_changed_not_from_property_trees
  auto update_flag1 = CreateDefaultUpdate();
  auto layer_props1 = CreateManualLayer(layer_id);
  layer_props1->layer_property_changed_not_from_property_trees = true;
  update_flag1->layers.push_back(std::move(layer_props1));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_flag1))
                  .has_value());
  cc::LayerImpl* layer_impl_flag1 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_flag1);
  EXPECT_TRUE(layer_impl_flag1->LayerPropertyChangedNotFromPropertyTrees());

  // Test layer_property_changed_from_property_trees
  auto update_flag2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(layer_id);
  layer_props2->layer_property_changed_from_property_trees = true;
  update_flag2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_flag2))
                  .has_value());
  // This flag is reset after processing, so we can't directly verify it here
  // without more complex state tracking or inspecting internal LayerImpl
  // states that are affected by it. For now, we ensure the update passes.
  // A more thorough test would involve checking if draw properties were
  // actually updated.
  ASSERT_NE(nullptr, GetLayerFromActiveTree(layer_id));
}

TEST_F(LayerContextImplLayerLifecycleTest, RareProperties) {
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  const auto kFirstId = RegionCaptureCropId::CreateRandom();
  const auto kSecondId = RegionCaptureCropId::CreateRandom();
  const RegionCaptureBounds kLayerBounds{
      {{kFirstId, gfx::Rect{0, 0, 250, 250}}, {kSecondId, gfx::Rect{}}}};

  auto update_rare = CreateDefaultUpdate();
  auto layer_props = CreateManualLayer(layer_id);
  layer_props->rare_properties = mojom::RareProperties::New();
  layer_props->rare_properties->filter_quality =
      cc::PaintFlags::FilterQuality::kMedium;
  layer_props->rare_properties->dynamic_range_limit =
      cc::PaintFlags::DynamicRangeLimitMixture(1.f, 0.5f);
  layer_props->rare_properties->capture_bounds = kLayerBounds;
  update_rare->layers.push_back(std::move(layer_props));

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_rare))
                  .has_value());

  cc::LayerImpl* layer_impl_rare = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_rare);
  EXPECT_EQ(layer_impl_rare->GetFilterQuality(),
            cc::PaintFlags::FilterQuality::kMedium);
  EXPECT_EQ(layer_impl_rare->GetDynamicRangeLimit(),
            cc::PaintFlags::DynamicRangeLimitMixture(1.f, 0.5f));
  ASSERT_TRUE(layer_impl_rare->capture_bounds());
  EXPECT_EQ(*layer_impl_rare->capture_bounds(), kLayerBounds);
}

TEST_F(LayerContextImplLayerLifecycleTest, ContentsOpaqueFlags) {
  ResetTestState();
  auto update = CreateDefaultUpdate();
  int layer_id = AddDefaultLayerToUpdate(update.get());
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update)).has_value());

  // Valid: contents_opaque = true, contents_opaque_for_text = true
  auto update_valid1 = CreateDefaultUpdate();
  auto layer_props_valid1 = CreateManualLayer(layer_id);
  layer_props_valid1->contents_opaque = true;
  layer_props_valid1->contents_opaque_for_text = true;
  update_valid1->layers.push_back(std::move(layer_props_valid1));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid1))
                  .has_value());
  cc::LayerImpl* layer_impl_valid1 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid1);
  EXPECT_TRUE(layer_impl_valid1->contents_opaque());
  EXPECT_TRUE(layer_impl_valid1->contents_opaque_for_text());

  // Invalid: contents_opaque = true, contents_opaque_for_text = false
  auto update_invalid = CreateDefaultUpdate();
  auto layer_props_invalid = CreateManualLayer(layer_id);
  layer_props_invalid->contents_opaque = true;
  layer_props_invalid->contents_opaque_for_text = false;
  update_invalid->layers.push_back(std::move(layer_props_invalid));
  auto result_invalid =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_invalid));
  ASSERT_FALSE(result_invalid.has_value());
  EXPECT_EQ(result_invalid.error(),
            "Invalid contents_opaque_for_text: cannot be false if "
            "contents_opaque is true.");
  // Verify properties remain from the last valid update
  cc::LayerImpl* layer_impl_invalid = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_invalid);
  EXPECT_TRUE(layer_impl_invalid->contents_opaque());
  EXPECT_TRUE(layer_impl_invalid->contents_opaque_for_text());

  // Valid: contents_opaque = false, contents_opaque_for_text = true
  auto update_valid2 = CreateDefaultUpdate();
  auto layer_props_valid2 = CreateManualLayer(layer_id);
  layer_props_valid2->contents_opaque = false;
  layer_props_valid2->contents_opaque_for_text = true;
  update_valid2->layers.push_back(std::move(layer_props_valid2));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid2))
                  .has_value());
  cc::LayerImpl* layer_impl_valid2 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid2);
  EXPECT_FALSE(layer_impl_valid2->contents_opaque());
  EXPECT_TRUE(layer_impl_valid2->contents_opaque_for_text());

  // Valid: contents_opaque = false, contents_opaque_for_text = false
  auto update_valid3 = CreateDefaultUpdate();
  auto layer_props_valid3 = CreateManualLayer(layer_id);
  layer_props_valid3->contents_opaque = false;
  layer_props_valid3->contents_opaque_for_text = false;
  update_valid3->layers.push_back(std::move(layer_props_valid3));
  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(update_valid3))
                  .has_value());
  cc::LayerImpl* layer_impl_valid3 = GetLayerFromActiveTree(layer_id);
  ASSERT_NE(nullptr, layer_impl_valid3);
  EXPECT_FALSE(layer_impl_valid3->contents_opaque());
  EXPECT_FALSE(layer_impl_valid3->contents_opaque_for_text());
}

TEST_F(LayerContextImplLayerLifecycleTest, MissingLayerExtra) {
  const std::vector<cc::mojom::LayerType> types_requiring_extra = {
      cc::mojom::LayerType::kMirror,
      cc::mojom::LayerType::kNinePatchThumbScrollbar,
      cc::mojom::LayerType::kPaintedScrollbar,
      cc::mojom::LayerType::kSolidColorScrollbar,
      cc::mojom::LayerType::kSurface,
      cc::mojom::LayerType::kTexture,
      cc::mojom::LayerType::kViewTransitionContent,
  };

  for (cc::mojom::LayerType type : types_requiring_extra) {
    SCOPED_TRACE(testing::Message()
                 << "Testing LayerType: " << static_cast<int>(type));
    ResetTestState();
    // Create a valid root layer first.
    auto initial_update = CreateDefaultUpdate();
    EXPECT_TRUE(
        layer_context_impl_->DoUpdateDisplayTree(std::move(initial_update))
            .has_value());

    auto update_missing_extra = CreateDefaultUpdate();
    int layer_id = next_layer_id_++;
    // Create the layer manually without setting layer_extra.
    auto layer = CreateManualLayer(layer_id, type);
    // Ensure layer_extra is indeed null.
    layer->layer_extra = nullptr;

    update_missing_extra->layers.push_back(std::move(layer));
    update_missing_extra->layer_order = layer_order_;
    update_missing_extra->layer_order->push_back(layer_id);

    auto result = layer_context_impl_->DoUpdateDisplayTree(
        std::move(update_missing_extra));
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "Invalid layer_extra");
  }
}

TEST_F(LayerContextImplLayerLifecycleTest,
       UpdateExistingLayerWithInvalidPropertyTreeIndicesFails) {
  constexpr int kLayerId = 2;
  constexpr int kValidIndex = 1;     // Assumes root (0) and secondary_root (1)
  constexpr int kInvalidIndex = 99;  // An index that will be out of bounds.

  // Setup: Create a layer with valid indices and small property trees.
  auto setup_update = CreateDefaultUpdate();

  AddDefaultLayerToUpdate(setup_update.get(), cc::mojom::LayerType::kLayer,
                          kLayerId);
  // Set initial valid indices for the layer.
  setup_update->layers.back()->transform_tree_index = kValidIndex;
  setup_update->layers.back()->clip_tree_index = kValidIndex;
  setup_update->layers.back()->effect_tree_index = kValidIndex;
  setup_update->layers.back()->scroll_tree_index = kValidIndex;

  EXPECT_TRUE(layer_context_impl_->DoUpdateDisplayTree(std::move(setup_update))
                  .has_value());
  VerifyLayerExists(kLayerId, true);

  // Test Case 1: Update with invalid transform_tree_index.
  auto update_invalid_transform = CreateDefaultUpdate();
  update_invalid_transform->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds,
      kInvalidIndex, kValidIndex, kValidIndex, kValidIndex));
  auto result_transform = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_invalid_transform));
  ASSERT_FALSE(result_transform.has_value());
  EXPECT_THAT(result_transform.error(),
              testing::StartsWith("Invalid transform tree ID"));

  // Test Case 2: Update with invalid clip_tree_index.
  auto update_invalid_clip = CreateDefaultUpdate();
  update_invalid_clip->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds, kValidIndex,
      kInvalidIndex, kValidIndex, kValidIndex));
  auto result_clip =
      layer_context_impl_->DoUpdateDisplayTree(std::move(update_invalid_clip));
  ASSERT_FALSE(result_clip.has_value());
  EXPECT_THAT(result_clip.error(), testing::StartsWith("Invalid clip tree ID"));

  // Test Case 3: Update with invalid effect_tree_index (similar for scroll).
  auto update_invalid_effect = CreateDefaultUpdate();
  update_invalid_effect->layers.push_back(CreateManualLayer(
      kLayerId, cc::mojom::LayerType::kLayer, kDefaultLayerBounds, kValidIndex,
      kValidIndex, kInvalidIndex, kValidIndex));
  auto result_effect = layer_context_impl_->DoUpdateDisplayTree(
      std::move(update_invalid_effect));
  ASSERT_FALSE(result_effect.has_value());
  EXPECT_THAT(result_effect.error(),
              testing::StartsWith("Invalid effect tree ID"));

  // Verify layer properties remain from the last successful update.
  cc::LayerImpl* layer_impl_after_invalid = GetLayerFromActiveTree(kLayerId);
  ASSERT_NE(nullptr, layer_impl_after_invalid);
  EXPECT_EQ(layer_impl_after_invalid->transform_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->clip_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->effect_tree_index(), kValidIndex);
  EXPECT_EQ(layer_impl_after_invalid->scroll_tree_index(), kValidIndex);
}

class LayerContextImplUpdateDisplayTreeTextureLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::TextureLayerImpl* GetTextureLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kTexture) {
      return static_cast<cc::TextureLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest, UpdateUVRect) {
  constexpr int kTextureLayerId = 2;
  const gfx::PointF kUpdatedUVTopLeft(0.1f, 0.2f);
  const gfx::PointF kUpdatedUVBottomRight(0.8f, 0.9f);

  // Initial update: Create TextureLayer.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);

  EXPECT_EQ(texture_layer_impl->uv_top_left(), kDefaultUVTopLeft);
  EXPECT_EQ(texture_layer_impl->uv_bottom_right(), kDefaultUVBottomRight);

  // Second update: Update UV rect.
  auto update2 = CreateDefaultUpdate();
  auto layer_props =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra = layer_props->layer_extra->get_texture_layer_extra();
  texture_extra->uv_top_left = kUpdatedUVTopLeft;
  texture_extra->uv_bottom_right = kUpdatedUVBottomRight;
  update2->layers.push_back(std::move(layer_props));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->uv_top_left(), kUpdatedUVTopLeft);
  EXPECT_EQ(texture_layer_impl->uv_bottom_right(), kUpdatedUVBottomRight);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateBlendBackgroundColor) {
  constexpr int kTextureLayerId = 2;
  constexpr bool kUpdatedBlendBackgroundColor = true;

  // Initial update: Create TextureLayer with default blend_background_color.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->blend_background_color(),
            kDefaultBlendBackgroundColor);

  // Second update: Update blend_background_color to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  texture_extra2->blend_background_color = kUpdatedBlendBackgroundColor;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->blend_background_color(),
            kUpdatedBlendBackgroundColor);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateForceTextureToOpaque) {
  constexpr int kTextureLayerId = 2;
  constexpr bool kUpdatedForceTextureToOpaque = true;

  // Initial update: Create TextureLayer with default
  // kDefaultForceTextureToOpaque.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  // Default is false.
  // No need to explicitly set texture_extra1->force_texture_to_opaque = false;
  // as it's the default from CreateDefaultLayerExtra.

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->force_texture_to_opaque(),
            kDefaultForceTextureToOpaque);

  // Second update: Update force_texture_to_opaque to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kTextureLayerId, cc::mojom::LayerType::kTexture);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  texture_extra2->force_texture_to_opaque = kUpdatedForceTextureToOpaque;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_EQ(texture_layer_impl->force_texture_to_opaque(),
            kUpdatedForceTextureToOpaque);
}

TEST_F(LayerContextImplUpdateDisplayTreeTextureLayerTest,
       UpdateTransferableResource) {
  constexpr int kTextureLayerId = 2;
  const gpu::Mailbox kMailbox1 = gpu::Mailbox::Generate();
  const gpu::Mailbox kMailbox2 = gpu::Mailbox::Generate();
  const gpu::SyncToken kSyncToken1(
      gpu::GPU_IO, gpu::CommandBufferId::FromUnsafeValue(0x123), 42);
  const gpu::SyncToken kSyncToken2(
      gpu::GPU_IO, gpu::CommandBufferId::FromUnsafeValue(0x123), 43);
  const gfx::Size kResourceSize1(10, 10);
  const gfx::Size kResourceSize2(12, 12);

  // Initial update: Create TextureLayer without a resource.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kTexture,
                          kTextureLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kTextureLayerId, true);

  cc::TextureLayerImpl* texture_layer_impl =
      GetTextureLayerFromActiveTree(kTextureLayerId);
  ASSERT_NE(nullptr, texture_layer_impl);
  EXPECT_TRUE(texture_layer_impl->transferable_resource().is_empty());

  // Second update: Set transferable_resource1.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize1);
  auto& texture_extra2 = layer_props2->layer_extra->get_texture_layer_extra();
  TransferableResource resource1 = TransferableResource::MakeGpu(
      kMailbox1, GL_TEXTURE_2D, kSyncToken1, kResourceSize1,
      SinglePlaneFormat::kRGBA_8888, false);
  texture_extra2->transferable_resource = resource1;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  // The TransferableResource matches what we sent.
  EXPECT_EQ(texture_layer_impl->transferable_resource(), resource1);

  // Third update: Set transferable_resource2 (different resource).
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize2);
  auto& texture_extra3 = layer_props3->layer_extra->get_texture_layer_extra();
  TransferableResource resource2 = TransferableResource::MakeGpu(
      kMailbox2, GL_TEXTURE_RECTANGLE_ARB, kSyncToken2, kResourceSize2,
      SinglePlaneFormat::kRGBA_8888, false);
  texture_extra3->transferable_resource = resource2;
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  EXPECT_EQ(texture_layer_impl->transferable_resource(), resource2);

  // Fourth update: Clear the resource.
  auto update4 = CreateDefaultUpdate();
  auto layer_props4 = CreateManualLayer(
      kTextureLayerId, cc::mojom::LayerType::kTexture, kResourceSize1);
  // Clearing has to be via an explicit empty resource.
  auto& texture_extra4 = layer_props4->layer_extra->get_texture_layer_extra();
  texture_extra4->transferable_resource = TransferableResource();
  update4->layers.push_back(std::move(layer_props4));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update4)).has_value());
  EXPECT_TRUE(texture_layer_impl->transferable_resource().is_empty());
}

class LayerContextImplUpdateDisplayTreeSurfaceLayerTest
    : public LayerContextImplLayerLifecycleTest {
 protected:
  cc::SurfaceLayerImpl* GetSurfaceLayerFromActiveTree(int layer_id) {
    cc::LayerImpl* layer = GetLayerFromActiveTree(layer_id);
    if (layer && layer->GetLayerType() == cc::mojom::LayerType::kSurface) {
      return static_cast<cc::SurfaceLayerImpl*>(layer);
    }
    return nullptr;
  }
};

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateBooleanProperties) {
  constexpr int kSurfaceLayerId = 2;

  // Initial update: Create SurfaceLayer with default boolean values.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  VerifyLayerExists(kSurfaceLayerId, true);

  cc::SurfaceLayerImpl* layer_impl =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl);

  // Defaults should be false from CreateDefaultLayerExtra.
  EXPECT_EQ(layer_impl->stretch_content_to_fill_bounds(),
            kDefaultStretchContentToFillBounds);
  EXPECT_EQ(layer_impl->surface_hit_testable(), kDefaultSurfaceHitTestable);
  EXPECT_EQ(layer_impl->has_pointer_events_none(),
            kDefaultHasPointerEventsNone);
  EXPECT_EQ(layer_impl->is_reflection(), kDefaultIsReflection);
  EXPECT_EQ(layer_impl->override_child_paint_flags(),
            kDefaultOverrideChildPaintFlags);

  // Second update: Update all boolean properties to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  surface_extra2->stretch_content_to_fill_bounds = true;
  surface_extra2->surface_hit_testable = true;
  surface_extra2->has_pointer_events_none = true;
  surface_extra2->is_reflection = true;
  surface_extra2->override_child_paint_flags = true;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_TRUE(layer_impl->stretch_content_to_fill_bounds());
  EXPECT_TRUE(layer_impl->surface_hit_testable());
  EXPECT_TRUE(layer_impl->has_pointer_events_none());
  EXPECT_TRUE(layer_impl->is_reflection());
  EXPECT_TRUE(layer_impl->override_child_paint_flags());

  // Third update: Update all boolean properties back to false.
  auto update3 = CreateDefaultUpdate();
  auto layer_props3 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra3 = layer_props3->layer_extra->get_surface_layer_extra();
  surface_extra3->stretch_content_to_fill_bounds = false;
  surface_extra3->surface_hit_testable = false;
  surface_extra3->has_pointer_events_none = false;
  surface_extra3->is_reflection = false;
  surface_extra3->override_child_paint_flags = false;
  update3->layers.push_back(std::move(layer_props3));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update3)).has_value());
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_FALSE(layer_impl->stretch_content_to_fill_bounds());
  EXPECT_FALSE(layer_impl->surface_hit_testable());
  EXPECT_FALSE(layer_impl->has_pointer_events_none());
  EXPECT_FALSE(layer_impl->is_reflection());
  EXPECT_FALSE(layer_impl->override_child_paint_flags());
}

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateSurfaceRangeAndDeadline) {
  constexpr int kSurfaceLayerId = 2;
  constexpr uint32_t kUpdatedDeadlineInFrames = 5u;

  // Initial update: Create SurfaceLayer with default range and deadline.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SurfaceLayerImpl* layer_impl =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl);
  EXPECT_EQ(layer_impl->range(), kDefaultSurfaceRange);
  EXPECT_EQ(layer_impl->deadline_in_frames(), kDefaultDeadlineInFrames);

  // Second update: Update surface_range and deadline_in_frames.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  LocalSurfaceId new_lsi(4, base::UnguessableToken::CreateForTesting(5, 6));
  surface_extra2->surface_range =
      SurfaceRange(std::nullopt, SurfaceId(kDefaultFrameSinkId, new_lsi));
  surface_extra2->deadline_in_frames = kUpdatedDeadlineInFrames;
  update2->layers.push_back(std::move(layer_props2));

  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  EXPECT_EQ(layer_impl->range().end(), SurfaceId(kDefaultFrameSinkId, new_lsi));
  EXPECT_EQ(layer_impl->deadline_in_frames(), kUpdatedDeadlineInFrames);
}

TEST_F(LayerContextImplUpdateDisplayTreeSurfaceLayerTest,
       UpdateWillDrawNeedsReset) {
  constexpr int kSurfaceLayerId = 2;

  // Initial update: Create SurfaceLayer with default will_draw_needs_reset.
  auto update1 = CreateDefaultUpdate();
  AddDefaultLayerToUpdate(update1.get(), cc::mojom::LayerType::kSurface,
                          kSurfaceLayerId);
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  cc::SurfaceLayerImpl* layer_impl1 =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl1);
  EXPECT_EQ(layer_impl1->will_draw_needs_reset(), kDefaultWillDrawNeedsReset);

  // Second update: Set will_draw_needs_reset to true.
  auto update2 = CreateDefaultUpdate();
  auto layer_props2 =
      CreateManualLayer(kSurfaceLayerId, cc::mojom::LayerType::kSurface);
  auto& surface_extra2 = layer_props2->layer_extra->get_surface_layer_extra();
  surface_extra2->will_draw_needs_reset = true;
  update2->layers.push_back(std::move(layer_props2));
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update2)).has_value());
  cc::SurfaceLayerImpl* layer_impl2 =
      GetSurfaceLayerFromActiveTree(kSurfaceLayerId);
  ASSERT_NE(nullptr, layer_impl2);
  EXPECT_TRUE(layer_impl2->will_draw_needs_reset());
}

}  // namespace viz
