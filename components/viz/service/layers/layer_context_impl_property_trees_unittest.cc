// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "cc/layers/mirror_layer_impl.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/layers/layer_context_impl.h"
#include "components/viz/service/layers/layer_context_impl_base_unittest.h"
#include "components/viz/test/fake_compositor_frame_sink_client.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {
namespace {

class LayerContextImplPropertyTreesTestBase : public LayerContextImplTest {
 protected:
  mojom::TransformNodePtr CreateDefaultSecondaryRootTransformNode() {
    auto node = mojom::TransformNode::New();
    node->id = cc::kSecondaryRootPropertyNodeId;
    node->parent_id = cc::kRootPropertyNodeId;
    return node;
  }

  mojom::ClipNodePtr CreateDefaultSecondaryRootClipNode() {
    auto node = mojom::ClipNode::New();
    node->id = cc::kSecondaryRootPropertyNodeId;
    node->parent_id = cc::kRootPropertyNodeId;
    // Default transform_id for clip nodes often points to a page scale
    // transform or similar, let's use a common default.
    node->transform_id = viewport_property_ids.page_scale_transform;
    return node;
  }

  mojom::EffectNodePtr CreateDefaultSecondaryRootEffectNode() {
    auto node = mojom::EffectNode::New();
    node->id = cc::kSecondaryRootPropertyNodeId;
    node->parent_id = cc::kRootPropertyNodeId;
    node->transform_id = viewport_property_ids.page_scale_transform;
    node->clip_id = cc::kRootPropertyNodeId;
    node->target_id = cc::kRootPropertyNodeId;
    return node;
  }

  mojom::ScrollNodePtr CreateDefaultSecondaryRootScrollNode() {
    auto node = mojom::ScrollNode::New();
    node->id = cc::kSecondaryRootPropertyNodeId;
    node->parent_id = cc::kRootPropertyNodeId;
    node->transform_id = viewport_property_ids.page_scale_transform;
    return node;
  }
};

class LayerContextImplUpdateDisplayTreeTransformNodeTest
    : public LayerContextImplPropertyTreesTestBase {
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
  auto node_update = CreateDefaultSecondaryRootTransformNode();
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

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidParentFrameId) {
  // Apply a default valid update first to set up the tree.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;  // Update existing node
  node_update->parent_id = cc::kRootPropertyNodeId;    // Valid tree parent
  // transform_tree by default has 2 nodes (0, 1).
  // next_available_id() will be 2, which is an invalid parent_frame_id.
  node_update->parent_frame_id = layer_context_impl_->host_impl()
                                     ->active_tree()
                                     ->property_trees()
                                     ->transform_tree()
                                     .next_available_id();
  update2->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid parent_frame_id");

  // Test with another invalid ID like -2 (kInvalidPropertyNodeId is -1).
  auto update3 = CreateDefaultUpdate();
  auto node_update3 = mojom::TransformNode::New();
  node_update3->id = cc::kSecondaryRootPropertyNodeId;
  node_update3->parent_id = cc::kRootPropertyNodeId;
  node_update3->parent_frame_id = -2;
  update3->transform_nodes.push_back(std::move(node_update3));
  result = layer_context_impl_->DoUpdateDisplayTree(std::move(update3));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid parent_frame_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidStickyPositionConstraintId_EmptyData) {
  // Apply a default valid update. sticky_position_data will be empty by
  // default.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  ASSERT_TRUE(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->property_trees()
                  ->transform_tree()
                  .sticky_position_data()
                  .empty());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->parent_id = cc::kRootPropertyNodeId;
  node_update->sticky_position_constraint_id = 0;  // Invalid, data size is 0.
  update2->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid sticky_position_constraint_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidStickyPositionConstraintId_NonEmptyData) {
  auto update1 = CreateDefaultUpdate();
  auto tree_props = mojom::TransformTreeUpdate::New();
  auto sticky_data = mojom::StickyPositionNodeData::New();
  // AddScrollNode to update1 to make scroll_ancestor valid for
  // DeserializeStickyPositionData.
  int scroll_node_id = AddScrollNode(update1.get(), cc::kRootPropertyNodeId);
  sticky_data->scroll_ancestor = scroll_node_id;
  tree_props->sticky_position_data.push_back(std::move(sticky_data));
  update1->transform_tree_update = std::move(tree_props);

  // The node update is part of the same LayerTreeUpdate.
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->parent_id = cc::kRootPropertyNodeId;
  // sticky_position_data has size 1, so ID 1 is out of bounds.
  node_update->sticky_position_constraint_id = 1;
  update1->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update1));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid sticky_position_constraint_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidAnchorPositionScrollDataId_EmptyData) {
  // Apply a default valid update. anchor_position_scroll_data will be empty by
  // default.
  auto update1 = CreateDefaultUpdate();
  EXPECT_TRUE(
      layer_context_impl_->DoUpdateDisplayTree(std::move(update1)).has_value());
  ASSERT_TRUE(layer_context_impl_->host_impl()
                  ->active_tree()
                  ->property_trees()
                  ->transform_tree()
                  .anchor_position_scroll_data()
                  .empty());

  auto update2 = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->parent_id = cc::kRootPropertyNodeId;
  // anchor_position_scroll_data is empty, so ID 0 is out of bounds.
  node_update->anchor_position_scroll_data_id = 0;
  update2->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update2));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid anchor_position_scroll_data_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidAnchorPositionScrollDataId_NonEmptyData) {
  auto update1 = CreateDefaultUpdate();
  auto tree_props = mojom::TransformTreeUpdate::New();
  auto anchor_data = mojom::AnchorPositionScrollData::New();
  // anchor_data can be default-constructed for
  // DeserializeAnchorPositionScrollData.
  tree_props->anchor_position_scroll_data.push_back(std::move(anchor_data));
  update1->transform_tree_update = std::move(tree_props);

  // The node update is part of the same LayerTreeUpdate.
  auto node_update = mojom::TransformNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->parent_id = cc::kRootPropertyNodeId;
  // anchor_position_scroll_data has size 1, so ID 1 is out of bounds.
  node_update->anchor_position_scroll_data_id = 1;
  update1->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update1));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid anchor_position_scroll_data_id");
}

TEST_F(LayerContextImplUpdateDisplayTreeTransformNodeTest,
       InvalidParentIdForNonRootTransformNode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::TransformNode::New();
  // Use an ID that is not a root ID.
  node_update->id = AddTransformNode(update.get(), cc::kRootPropertyNodeId);
  node_update->parent_id = cc::kInvalidPropertyNodeId;  // Invalid parent
  update->transform_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid parent_id for non-root property tree node");
}

class LayerContextImplUpdateDisplayTreeClipNodeTest
    : public LayerContextImplPropertyTreesTestBase {
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
  auto node_update = CreateDefaultSecondaryRootClipNode();
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

TEST_F(LayerContextImplUpdateDisplayTreeClipNodeTest,
       InvalidParentIdForNonRootClipNode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ClipNode::New();
  node_update->id = AddClipNode(update.get(), cc::kRootPropertyNodeId);
  node_update->parent_id = cc::kInvalidPropertyNodeId;  // Invalid parent
  node_update->transform_id = cc::kRootPropertyNodeId;
  update->clip_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid parent_id for non-root property tree node");
}

class LayerContextImplUpdateDisplayTreeEffectNodeTest
    : public LayerContextImplPropertyTreesTestBase {
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
  auto node_update = CreateDefaultSecondaryRootEffectNode();
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
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->transform_id = 99;  // Invalid transform ID
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid transform_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeClipId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->clip_id = 99;  // Invalid clip ID
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid clip_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidEffectNodeTargetId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->target_id = 99;  // Invalid target ID
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid target_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest, InvalidBlendMode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->blend_mode = 999;  // Invalid blend mode
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid blend_mode for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidParentIdForNonRootEffectNode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = AddEffectNode(update.get(), cc::kRootPropertyNodeId);
  node_update->parent_id = cc::kInvalidPropertyNodeId;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid parent_id for non-root property tree node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidClosestAncestorWithCachedRenderSurfaceId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->closest_ancestor_with_cached_render_surface_id = next_effect_id_;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid closest_ancestor_with_cached_render_surface_id for "
            "effect node");

  auto update_neg = CreateDefaultUpdate();
  auto node_update_neg = mojom::EffectNode::New();
  node_update_neg->id = cc::kSecondaryRootPropertyNodeId;
  node_update_neg->closest_ancestor_with_cached_render_surface_id = -2;
  update_neg->effect_nodes.push_back(std::move(node_update_neg));

  result = layer_context_impl_->DoUpdateDisplayTree(std::move(update_neg));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid closest_ancestor_with_cached_render_surface_id for "
            "effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidClosestAncestorWithCopyRequestId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->closest_ancestor_with_copy_request_id = next_effect_id_;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid closest_ancestor_with_copy_request_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidClosestAncestorBeingCapturedId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->closest_ancestor_being_captured_id = next_effect_id_;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid closest_ancestor_being_captured_id for effect node");
}

TEST_F(LayerContextImplUpdateDisplayTreeEffectNodeTest,
       InvalidClosestAncestorWithSharedElementId) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::EffectNode::New();
  node_update->id = cc::kSecondaryRootPropertyNodeId;
  node_update->closest_ancestor_with_shared_element_id = next_effect_id_;
  update->effect_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid closest_ancestor_with_shared_element_id for effect node");
}

class LayerContextImplUpdateDisplayTreeScrollNodeTest
    : public LayerContextImplPropertyTreesTestBase {
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

TEST_F(LayerContextImplUpdateDisplayTreeScrollNodeTest,
       InvalidParentIdForNonRootScrollNode) {
  auto update = CreateDefaultUpdate();
  auto node_update = mojom::ScrollNode::New();
  node_update->id = AddScrollNode(update.get(), cc::kRootPropertyNodeId);
  node_update->parent_id = cc::kInvalidPropertyNodeId;  // Invalid parent
  node_update->transform_id = cc::kRootPropertyNodeId;
  update->scroll_nodes.push_back(std::move(node_update));

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Invalid parent_id for non-root property tree node");
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
        std::make_tuple(LayerContextImplTest::kDefaultMaxPageScaleFactor - 0.1f,
                        true),

        // Test value equal to max_page_scale_factor.
        std::make_tuple(LayerContextImplTest::kDefaultMaxPageScaleFactor, true),

        // Test value greater than max_page_scale_factor.
        std::make_tuple(LayerContextImplTest::kDefaultMaxPageScaleFactor + 0.1f,
                        false),

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
        std::make_tuple(
            LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest::
                kDefaultMinPageScaleFactor,
            true),

        // Test value above min_page_scale_factor.
        std::make_tuple(
            LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest::
                    kDefaultMinPageScaleFactor +
                0.1f,
            true),

        // Test value below min_page_scale_factor.
        std::make_tuple(
            LayerContextImplUpdateDisplayTreeMaxPageScaleFactorTest::
                    kDefaultMinPageScaleFactor -
                0.1f,
            false),

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

class LayerContextImplViewportPropertyIdsTest : public LayerContextImplTest {};

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateValidViewportPropertyIds) {
  auto update = CreateDefaultUpdate();

  // Add dummy nodes to ensure the IDs we use below are valid and unique.
  for (int i = 0; i < 3; ++i) {
    AddTransformNode(update.get(), cc::kRootPropertyNodeId);
    AddClipNode(update.get(), cc::kRootPropertyNodeId);
    AddScrollNode(update.get(), cc::kRootPropertyNodeId);
  }

  const int kOverscrollElasticityTransformId = 4;
  const int kPageScaleTransformId = 5;
  const int kInnerScrollId = 2;
  const int kOuterClipId = 3;
  const int kOuterScrollId = 4;

  update->overscroll_elasticity_transform = kOverscrollElasticityTransformId;
  update->page_scale_transform = kPageScaleTransformId;
  update->inner_scroll = kInnerScrollId;
  update->outer_clip = kOuterClipId;
  update->outer_scroll = kOuterScrollId;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& viewport_property_ids =
      layer_context_impl_->host_impl()->active_tree()->viewport_property_ids();
  EXPECT_EQ(viewport_property_ids.overscroll_elasticity_transform,
            kOverscrollElasticityTransformId);
  EXPECT_EQ(viewport_property_ids.page_scale_transform, kPageScaleTransformId);
  EXPECT_EQ(viewport_property_ids.inner_scroll, kInnerScrollId);
  EXPECT_EQ(viewport_property_ids.outer_clip, kOuterClipId);
  EXPECT_EQ(viewport_property_ids.outer_scroll, kOuterScrollId);
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsWithInvalidInnerScroll) {
  auto update = CreateDefaultUpdate();
  update->inner_scroll = 99;  // Invalid ID
  update->outer_clip = 1;
  update->outer_scroll = 1;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid inner_scroll");
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsWithInvalidOuter) {
  auto update = CreateDefaultUpdate();
  update->inner_scroll = 1;
  update->outer_clip = 99;  // Invalid ID
  update->outer_scroll = 1;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid outer_clip");

  update = CreateDefaultUpdate();
  update->inner_scroll = 1;
  update->outer_clip = 1;
  update->outer_scroll = 99;  // Invalid ID

  result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid outer_scroll");
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsToInvalid) {
  auto update = CreateDefaultUpdate();
  update->overscroll_elasticity_transform = cc::kInvalidPropertyNodeId;
  update->page_scale_transform = cc::kInvalidPropertyNodeId;
  update->inner_scroll = cc::kInvalidPropertyNodeId;
  update->outer_clip = cc::kInvalidPropertyNodeId;
  update->outer_scroll = cc::kInvalidPropertyNodeId;

  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_TRUE(result.has_value());

  const auto& viewport_property_ids =
      layer_context_impl_->host_impl()->active_tree()->viewport_property_ids();
  EXPECT_EQ(viewport_property_ids.overscroll_elasticity_transform,
            cc::kInvalidPropertyNodeId);
  EXPECT_EQ(viewport_property_ids.page_scale_transform,
            cc::kInvalidPropertyNodeId);
  EXPECT_EQ(viewport_property_ids.inner_scroll, cc::kInvalidPropertyNodeId);
  EXPECT_EQ(viewport_property_ids.outer_clip, cc::kInvalidPropertyNodeId);
  EXPECT_EQ(viewport_property_ids.outer_scroll, cc::kInvalidPropertyNodeId);
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsWithInvalidOverscrollElasticityTransform) {
  auto update = CreateDefaultUpdate();
  update->overscroll_elasticity_transform = 99;  // Invalid ID
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid overscroll_elasticity_transform");
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsWithInvalidPageScaleTransform) {
  auto update = CreateDefaultUpdate();
  update->page_scale_transform = 99;  // Invalid ID
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), "Invalid page_scale_transform");
}

TEST_F(LayerContextImplViewportPropertyIdsTest,
       UpdateViewportPropertyIdsWithOuterScrollAndInvalidInnerScroll) {
  auto update = CreateDefaultUpdate();
  update->inner_scroll = cc::kInvalidPropertyNodeId;
  update->outer_scroll = 1;
  auto result = layer_context_impl_->DoUpdateDisplayTree(std::move(update));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            "Cannot set outer_clip or outer_scroll without valid inner_scroll");
}

}  // namespace
}  // namespace viz
