// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/expected_macros.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/tile_display_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace viz {

namespace {

int GenerateNextDisplayTreeId() {
  static int next_id = 1;
  return next_id++;
}

cc::LayerTreeSettings GetDisplayTreeSettings() {
  cc::LayerTreeSettings settings;
  settings.use_layer_lists = true;
  settings.is_display_tree = true;
  return settings;
}

std::unique_ptr<cc::LayerImpl> CreateLayer(LayerContextImpl& context,
                                           cc::LayerTreeImpl& tree,
                                           cc::mojom::LayerType type,
                                           int id) {
  switch (type) {
    case cc::mojom::LayerType::kLayer:
      return cc::LayerImpl::Create(&tree, id);

    case cc::mojom::LayerType::kPicture:
      return std::make_unique<cc::TileDisplayLayerImpl>(context, tree, id);

    default:
      // TODO(rockot): Support other layer types.
      return cc::SolidColorLayerImpl::Create(&tree, id);
  }
}

template <typename TreeType>
bool IsPropertyTreeIndexValid(const TreeType& tree, int32_t index) {
  return index >= 0 && index < tree.next_available_id();
}

template <typename TreeType>
bool IsOptionalPropertyTreeIndexValid(const TreeType& tree, int32_t index) {
  return index == cc::kInvalidPropertyNodeId ||
         IsPropertyTreeIndexValid(tree, index);
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::TransformNode& node,
    const mojom::TransformNode& wire) {
  auto& tree = trees.transform_tree_mutable();
  if (!IsOptionalPropertyTreeIndexValid(tree, wire.parent_frame_id)) {
    return base::unexpected("Invalid parent_frame_id");
  }
  node.parent_frame_id = wire.parent_frame_id;
  node.element_id = wire.element_id;
  if (node.element_id) {
    tree.SetElementIdForNodeId(node.id, node.element_id);
  }
  if (node.local != wire.local || node.origin != wire.origin ||
      node.scroll_offset != wire.scroll_offset) {
    node.needs_local_transform_update = true;
  }
  node.local = wire.local;
  node.origin = wire.origin;
  node.scroll_offset = wire.scroll_offset;
  node.snap_amount = wire.snap_amount;

  if (!wire.sticky_position_constraint_id) {
    node.sticky_position_constraint_id = -1;
  } else if (*wire.sticky_position_constraint_id >=
             tree.sticky_position_data().size()) {
    return base::unexpected("Invalid sticky_position_constraint_id");
  } else {
    node.sticky_position_constraint_id =
        base::saturated_cast<int>(*wire.sticky_position_constraint_id);
  }

  if (!wire.anchor_position_scroll_data_id) {
  } else if (*wire.anchor_position_scroll_data_id >=
             tree.anchor_position_scroll_data().size()) {
    return base::unexpected("Invalid anchor_position_scroll_data_id");
  } else {
    node.anchor_position_scroll_data_id =
        base::saturated_cast<int>(*wire.anchor_position_scroll_data_id);
  }

  node.sorting_context_id = wire.sorting_context_id;

  node.has_potential_animation = wire.has_potential_animation;
  node.is_currently_animating = wire.is_currently_animating;
  node.flattens_inherited_transform = wire.flattens_inherited_transform;
  node.scrolls = wire.scrolls;
  node.should_undo_overscroll = wire.should_undo_overscroll;
  node.should_be_snapped = wire.should_be_snapped;
  node.moved_by_outer_viewport_bounds_delta_y =
      wire.moved_by_outer_viewport_bounds_delta_y;
  node.in_subtree_of_page_scale_layer = wire.in_subtree_of_page_scale_layer;
  node.delegates_to_parent_for_backface = wire.delegates_to_parent_for_backface;
  node.will_change_transform = wire.will_change_transform;

  node.visible_frame_element_id = wire.visible_frame_element_id;
  node.transform_changed = true;
  return base::ok();
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::ClipNode& node,
    const mojom::ClipNode& wire) {
  if (!IsPropertyTreeIndexValid(trees.transform_tree(), wire.transform_id) ||
      !IsOptionalPropertyTreeIndexValid(trees.effect_tree(),
                                        wire.pixel_moving_filter_id)) {
    return base::unexpected("Invalid transform_id for clip node");
  }
  node.transform_id = wire.transform_id;
  node.clip = wire.clip;
  node.pixel_moving_filter_id = wire.pixel_moving_filter_id;
  return base::ok();
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::EffectNode& node,
    const mojom::EffectNode& wire) {
  if (!IsPropertyTreeIndexValid(trees.transform_tree(), wire.transform_id)) {
    return base::unexpected("Invalid transform_id for effect node");
  }
  if (!IsPropertyTreeIndexValid(trees.clip_tree(), wire.clip_id)) {
    return base::unexpected("Invalid clip_id for effect node");
  }
  if (!IsPropertyTreeIndexValid(trees.effect_tree(), wire.target_id)) {
    return base::unexpected("Invalid target_id for effect node");
  }
  node.transform_id = wire.transform_id;
  node.clip_id = wire.clip_id;
  node.element_id = wire.element_id;
  if (node.element_id) {
    trees.effect_tree_mutable().SetElementIdForNodeId(node.id, node.element_id);
  }
  node.opacity = wire.opacity;
  node.effect_changed = true;

  if (wire.has_render_surface) {
    // TODO(rockot): Plumb the real reason over IPC. It's only used for metrics
    // so we make something up for now.
    node.render_surface_reason = cc::RenderSurfaceReason::kRoot;
  } else {
    node.render_surface_reason = cc::RenderSurfaceReason::kNone;
  }

  node.surface_contents_scale = wire.surface_contents_scale;
  if (wire.blend_mode > static_cast<uint32_t>(SkBlendMode::kLastMode)) {
    return base::unexpected("Invalid blend_mode for effect node");
  }
  node.blend_mode = static_cast<SkBlendMode>(wire.blend_mode);
  node.target_id = wire.target_id;
  return base::ok();
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::ScrollNode& node,
    const mojom::ScrollNode& wire) {
  if (wire.transform_id != cc::kInvalidPropertyNodeId &&
      !IsPropertyTreeIndexValid(trees.transform_tree(), wire.transform_id)) {
    return base::unexpected("Invalid transform_id for scroll node");
  }
  node.transform_id = wire.transform_id;
  node.container_bounds = wire.container_bounds;
  node.bounds = wire.bounds;
  node.max_scroll_offset_affected_by_page_scale =
      wire.max_scroll_offset_affected_by_page_scale;
  node.scrolls_inner_viewport = wire.scrolls_inner_viewport;
  node.scrolls_outer_viewport = wire.scrolls_outer_viewport;
  node.prevent_viewport_scrolling_from_inner =
      wire.prevent_viewport_scrolling_from_inner;
  node.user_scrollable_horizontal = wire.user_scrollable_horizontal;
  node.user_scrollable_vertical = wire.user_scrollable_vertical;
  node.is_composited = wire.is_composited;
  node.element_id = wire.element_id;
  if (node.element_id) {
    trees.scroll_tree_mutable().SetElementIdForNodeId(node.id, node.element_id);
  }
  return base::ok();
}

template <typename TreeType>
bool ResizePropertyTree(TreeType& tree, uint32_t num_nodes) {
  if (num_nodes == tree.nodes().size()) {
    return false;
  }

  if (num_nodes < tree.nodes().size()) {
    tree.RemoveNodes(tree.nodes().size() - num_nodes);
    return true;
  }

  for (size_t i = tree.nodes().size(); i < num_nodes; ++i) {
    tree.Insert(typename TreeType::NodeType(), cc::kRootPropertyNodeId);
  }
  return true;
}

template <typename TreeType, typename WireContainerType>
base::expected<bool, std::string> UpdatePropertyTree(
    cc::PropertyTrees& trees,
    TreeType& tree,
    const WireContainerType& wire_updates) {
  if (wire_updates.empty()) {
    return false;
  }

  for (const auto& wire : wire_updates) {
    if (!IsPropertyTreeIndexValid(tree, wire->id)) {
      return base::unexpected("Invalid property tree node ID");
    }

    if (!IsOptionalPropertyTreeIndexValid(tree, wire->parent_id)) {
      return base::unexpected("Invalid property tree node parent_id");
    }

    if (wire->parent_id == cc::kInvalidPropertyNodeId &&
        wire->id != cc::kRootPropertyNodeId &&
        wire->id != cc::kSecondaryRootPropertyNodeId) {
      return base::unexpected(
          "Invalid parent_id for non-root property tree node");
    }

    auto& node = *tree.Node(wire->id);
    node.id = wire->id;
    node.parent_id = wire->parent_id;
    RETURN_IF_ERROR(UpdatePropertyTreeNode(trees, node, *wire));
  }
  return true;
}

base::expected<std::vector<cc::StickyPositionNodeData>, std::string>
DeserializeStickyPositionData(
    cc::PropertyTrees& trees,
    std::vector<mojom::StickyPositionNodeDataPtr>& wire_data) {
  std::vector<cc::StickyPositionNodeData> sticky_position_node_data;
  sticky_position_node_data.reserve(wire_data.size());
  for (auto& wire : wire_data) {
    if (!IsPropertyTreeIndexValid(trees.scroll_tree(), wire->scroll_ancestor)) {
      return base::unexpected("Invalid scroll ancestor ID");
    }

    cc::StickyPositionNodeData& data = sticky_position_node_data.emplace_back();
    data.scroll_ancestor = wire->scroll_ancestor;
    data.constraints.is_anchored_left = wire->is_anchored_left;
    data.constraints.is_anchored_right = wire->is_anchored_right;
    data.constraints.is_anchored_top = wire->is_anchored_top;
    data.constraints.is_anchored_bottom = wire->is_anchored_bottom;
    data.constraints.left_offset = wire->left_offset;
    data.constraints.right_offset = wire->right_offset;
    data.constraints.top_offset = wire->top_offset;
    data.constraints.bottom_offset = wire->bottom_offset;
    data.constraints.constraint_box_rect = wire->constraint_box_rect;
    data.constraints.scroll_container_relative_sticky_box_rect =
        wire->scroll_container_relative_sticky_box_rect;
    data.constraints.scroll_container_relative_containing_block_rect =
        wire->scroll_container_relative_containing_block_rect;
    data.nearest_node_shifting_sticky_box =
        wire->nearest_node_shifting_sticky_box;
    data.nearest_node_shifting_containing_block =
        wire->nearest_node_shifting_containing_block;
    data.total_sticky_box_sticky_offset = wire->total_sticky_box_sticky_offset;
    data.total_containing_block_sticky_offset =
        wire->total_containing_block_sticky_offset;
  }
  return sticky_position_node_data;
}

base::expected<std::vector<cc::AnchorPositionScrollData>, std::string>
DeserializeAnchorPositionScrollData(
    std::vector<mojom::AnchorPositionScrollDataPtr>& wire_data) {
  std::vector<cc::AnchorPositionScrollData> anchor_position_scroll_data;
  for (auto& wire : wire_data) {
    cc::AnchorPositionScrollData& data =
        anchor_position_scroll_data.emplace_back();
    data.adjustment_container_ids = wire->adjustment_container_ids;
    data.accumulated_scroll_origin = wire->accumulated_scroll_origin;
    data.needs_scroll_adjustment_in_x = wire->needs_scroll_adjustment_in_x;
    data.needs_scroll_adjustment_in_y = wire->needs_scroll_adjustment_in_y;
  }
  return anchor_position_scroll_data;
}

base::expected<void, std::string> UpdateTransformTreeProperties(
    cc::PropertyTrees& trees,
    cc::TransformTree& tree,
    mojom::TransformTreeUpdate& update) {
  tree.set_page_scale_factor(update.page_scale_factor);
  tree.set_device_scale_factor(update.device_scale_factor);
  tree.set_device_transform_scale_factor(update.device_transform_scale_factor);
  tree.set_nodes_affected_by_outer_viewport_bounds_delta(
      std::move(update.nodes_affected_by_outer_viewport_bounds_delta));
  ASSIGN_OR_RETURN(
      tree.sticky_position_data(),
      DeserializeStickyPositionData(trees, update.sticky_position_data));
  ASSIGN_OR_RETURN(
      tree.anchor_position_scroll_data(),
      DeserializeAnchorPositionScrollData(update.anchor_position_scroll_data));
  return base::ok();
}

base::expected<void, std::string> UpdateLayer(const mojom::Layer& wire,
                                              cc::LayerImpl& layer) {
  layer.SetBounds(wire.bounds);
  layer.SetContentsOpaque(wire.contents_opaque);
  layer.SetContentsOpaqueForText(wire.contents_opaque_for_text);
  layer.SetDrawsContent(wire.is_drawable);
  layer.SetBackgroundColor(wire.background_color);
  layer.SetSafeOpaqueBackgroundColor(wire.safe_opaque_background_color);
  layer.SetElementId(wire.element_id);
  layer.UnionUpdateRect(wire.update_rect);
  layer.SetOffsetToTransformParent(wire.offset_to_transform_parent);

  const cc::PropertyTrees& property_trees =
      *layer.layer_tree_impl()->property_trees();
  if (!IsPropertyTreeIndexValid(property_trees.transform_tree(),
                                wire.transform_tree_index)) {
    return base::unexpected(
        base::StrCat({"Invalid transform tree ID: ",
                      base::NumberToString(wire.transform_tree_index)}));
  }

  if (!IsPropertyTreeIndexValid(property_trees.clip_tree(),
                                wire.clip_tree_index)) {
    return base::unexpected(
        base::StrCat({"Invalid clip tree ID: ",
                      base::NumberToString(wire.clip_tree_index)}));
  }

  if (!IsPropertyTreeIndexValid(property_trees.effect_tree(),
                                wire.effect_tree_index)) {
    return base::unexpected(
        base::StrCat({"Invalid effect tree ID: ",
                      base::NumberToString(wire.effect_tree_index)}));
  }

  if (!IsPropertyTreeIndexValid(property_trees.scroll_tree(),
                                wire.scroll_tree_index)) {
    return base::unexpected(
        base::StrCat({"Invalid scroll tree ID: ",
                      base::NumberToString(wire.scroll_tree_index)}));
  }

  layer.SetTransformTreeIndex(wire.transform_tree_index);
  layer.SetClipTreeIndex(wire.clip_tree_index);
  layer.SetEffectTreeIndex(wire.effect_tree_index);
  layer.SetScrollTreeIndex(wire.scroll_tree_index);
  return base::ok();
}

base::expected<void, std::string> CreateOrUpdateLayers(
    LayerContextImpl& context,
    const std::vector<mojom::LayerPtr>& updates,
    std::optional<std::vector<int32_t>>& layer_order,
    cc::LayerTreeImpl& layers) {
  if (!layer_order) {
    // No layer list changes. Only update existing layers.
    for (auto& wire : updates) {
      cc::LayerImpl* layer = layers.LayerById(wire->id);
      if (!layer) {
        return base::unexpected("Invalid layer ID");
      }
      RETURN_IF_ERROR(UpdateLayer(*wire, *layer));
    }
    return base::ok();
  }

  // The layer list contents changed, so we need to rebuild the tree.
  cc::OwnedLayerImplList old_layers = layers.DetachLayers();
  cc::OwnedLayerImplMap layer_map;
  for (auto& layer : old_layers) {
    const int id = layer->id();
    layer_map[id] = std::move(layer);
  }
  for (auto& wire : updates) {
    auto& layer = layer_map[wire->id];
    if (!layer) {
      layer = CreateLayer(context, layers, wire->type, wire->id);
    }
    RETURN_IF_ERROR(UpdateLayer(*wire, *layer));
  }
  for (auto id : *layer_order) {
    auto& layer = layer_map[id];
    if (!layer) {
      return base::unexpected("Invalid or duplicate layer ID");
    }
    layers.AddLayer(std::move(layer));
  }
  return base::ok();
}

base::expected<void, std::string> UpdateViewportPropertyIds(
    cc::LayerTreeImpl& layers,
    cc::PropertyTrees& trees,
    mojom::LayerTreeUpdate& update) {
  const auto& transform_tree = trees.transform_tree();
  const auto& scroll_tree = trees.scroll_tree();
  const auto& clip_tree = trees.clip_tree();
  if (!IsOptionalPropertyTreeIndexValid(
          transform_tree, update.overscroll_elasticity_transform)) {
    return base::unexpected("Invalid overscroll_elasticity_transform");
  }
  if (!IsOptionalPropertyTreeIndexValid(transform_tree,
                                        update.page_scale_transform)) {
    return base::unexpected("Invalid page_scale_transform");
  }
  if (!IsOptionalPropertyTreeIndexValid(scroll_tree, update.inner_scroll)) {
    return base::unexpected("Invalid inner_scroll");
  }
  if (update.inner_scroll == cc::kInvalidPropertyNodeId &&
      (update.outer_clip != cc::kInvalidPropertyNodeId ||
       update.outer_scroll != cc::kInvalidPropertyNodeId)) {
    return base::unexpected(
        "Cannot set outer_clip or outer_scroll without valid inner_scroll");
  }
  if (!IsOptionalPropertyTreeIndexValid(clip_tree, update.outer_clip)) {
    return base::unexpected("Invalid outer_clip");
  }
  if (!IsOptionalPropertyTreeIndexValid(scroll_tree, update.outer_scroll)) {
    return base::unexpected("Invalid outer_scroll");
  }
  layers.SetViewportPropertyIds(cc::ViewportPropertyIds{
      .overscroll_elasticity_transform = update.overscroll_elasticity_transform,
      .page_scale_transform = update.page_scale_transform,
      .inner_scroll = update.inner_scroll,
      .outer_clip = update.outer_clip,
      .outer_scroll = update.outer_scroll,
  });
  return base::ok();
}

base::expected<cc::TileDisplayLayerImpl::TileResource, std::string>
DeserializeTileResource(mojom::TileResource& wire) {
  if (wire.resource.id == kInvalidResourceId) {
    return base::unexpected("Invalid tile resource");
  }
  return cc::TileDisplayLayerImpl::TileResource(
      wire.resource, wire.is_premultiplied, wire.is_checkered);
}

base::expected<cc::TileDisplayLayerImpl::TileContents, std::string>
DeserializeTileContents(mojom::TileContents& wire) {
  switch (wire.which()) {
    case mojom::TileContents::Tag::kMissingReason:
      return cc::TileDisplayLayerImpl::TileContents(
          cc::TileDisplayLayerImpl::NoContents());

    case mojom::TileContents::Tag::kResource:
      return DeserializeTileResource(*wire.get_resource());

    case mojom::TileContents::Tag::kSolidColor:
      return cc::TileDisplayLayerImpl::TileContents(wire.get_solid_color());
  }
}

base::expected<void, std::string> DeserializeTiling(
    cc::TileDisplayLayerImpl& layer,
    mojom::Tiling& wire) {
  const float scale_key =
      std::max(wire.raster_scale.x(), wire.raster_scale.y());
  auto& tiling = layer.GetOrCreateTilingFromScaleKey(scale_key);
  tiling.SetRasterTransform(gfx::AxisTransform2d::FromScaleAndTranslation(
      wire.raster_scale, wire.raster_translation));
  tiling.SetTileSize(wire.tile_size);
  tiling.SetTilingRect(wire.tiling_rect);
  for (auto& wire_tile : wire.tiles) {
    ASSIGN_OR_RETURN(auto contents,
                     DeserializeTileContents(*wire_tile->contents));
    tiling.SetTileContents(
        cc::TileIndex{base::saturated_cast<int>(wire_tile->column_index),
                      base::saturated_cast<int>(wire_tile->row_index)},
        std::move(contents));
  }
  return base::ok();
}

gfx::StepsTimingFunction::StepPosition DeserializeTimingStepPosition(
    mojom::TimingStepPosition step_position) {
  switch (step_position) {
    case mojom::TimingStepPosition::kStart:
      return gfx::StepsTimingFunction::StepPosition::START;
    case mojom::TimingStepPosition::kEnd:
      return gfx::StepsTimingFunction::StepPosition::END;
    case mojom::TimingStepPosition::kJumpBoth:
      return gfx::StepsTimingFunction::StepPosition::JUMP_BOTH;
    case mojom::TimingStepPosition::kJumpEnd:
      return gfx::StepsTimingFunction::StepPosition::JUMP_END;
    case mojom::TimingStepPosition::kJumpNone:
      return gfx::StepsTimingFunction::StepPosition::JUMP_NONE;
    case mojom::TimingStepPosition::kJumpStart:
      return gfx::StepsTimingFunction::StepPosition::JUMP_START;
  }
}

std::unique_ptr<gfx::TimingFunction> DeserializeTimingFunction(
    mojom::TimingFunction& wire) {
  switch (wire.which()) {
    case mojom::TimingFunction::Tag::kLinear: {
      const auto& wire_points = wire.get_linear();
      std::vector<gfx::LinearEasingPoint> points;
      points.reserve(wire_points.size());
      for (const auto& wire_point : wire_points) {
        points.emplace_back(wire_point->in, wire_point->out);
      }
      if (points.empty()) {
        return gfx::LinearTimingFunction::Create();
      }
      return gfx::LinearTimingFunction::Create(std::move(points));
    }
    case mojom::TimingFunction::Tag::kCubicBezier: {
      const auto& bezier = *wire.get_cubic_bezier();
      return gfx::CubicBezierTimingFunction::Create(bezier.x1, bezier.y1,
                                                    bezier.x2, bezier.y2);
    }
    case mojom::TimingFunction::Tag::kSteps: {
      const auto& steps = *wire.get_steps();
      return gfx::StepsTimingFunction::Create(
          base::saturated_cast<int32_t>(steps.num_steps),
          DeserializeTimingStepPosition(steps.step_position));
    }
  }
}

gfx::TransformOperations DeserializeTransformOperations(
    const std::vector<mojom::TransformOperationPtr>& wire_ops) {
  gfx::TransformOperations transform;
  for (const auto& wire_op : wire_ops) {
    switch (wire_op->which()) {
      case mojom::TransformOperation::Tag::kIdentity:
        transform.AppendIdentity();
        break;
      case mojom::TransformOperation::Tag::kPerspectiveDepth: {
        std::optional<float> depth;
        if (wire_op->get_perspective_depth()) {
          depth = wire_op->get_perspective_depth();
        }
        transform.AppendPerspective(depth);
        break;
      }
      case mojom::TransformOperation::Tag::kSkew: {
        const auto& skew = wire_op->get_skew();
        transform.AppendSkew(skew.x(), skew.y());
        break;
      }
      case mojom::TransformOperation::Tag::kScale: {
        const auto& scale = wire_op->get_scale();
        transform.AppendScale(scale.x(), scale.y(), scale.z());
        break;
      }
      case mojom::TransformOperation::Tag::kTranslate: {
        const auto& translate = wire_op->get_translate();
        transform.AppendTranslate(translate.x(), translate.y(), translate.z());
        break;
      }
      case mojom::TransformOperation::Tag::kRotate: {
        const auto& axis = wire_op->get_rotate()->axis;
        const float angle = wire_op->get_rotate()->angle;
        transform.AppendRotate(axis.x(), axis.y(), axis.z(), angle);
        break;
      }
      case mojom::TransformOperation::Tag::kMatrix:
        transform.AppendMatrix(wire_op->get_matrix());
        break;
    }
  }
  return transform;
}

template <typename CurveType>
using KeyframeType = CurveType::Keyframes::value_type::element_type;

template <typename CurveType>
base::expected<std::unique_ptr<KeyframeType<CurveType>>, std::string>
DeserializeKeyframe(const mojom::AnimationKeyframeValue& value,
                    base::TimeDelta start_time,
                    std::unique_ptr<gfx::TimingFunction> timing_function) {
  using ValueType = std::remove_cvref_t<
      decltype(std::declval<KeyframeType<CurveType>>().Value())>;
  std::unique_ptr<KeyframeType<CurveType>> keyframe;
  if constexpr (std::is_same_v<ValueType, float>) {
    if (value.is_scalar()) {
      keyframe = gfx::FloatKeyframe::Create(start_time, value.get_scalar(),
                                            std::move(timing_function));
    }
  } else if constexpr (std::is_same_v<ValueType, SkColor>) {
    if (value.is_color()) {
      keyframe = gfx::ColorKeyframe::Create(start_time, value.get_color(),
                                            std::move(timing_function));
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::SizeF>) {
    if (value.is_size()) {
      keyframe = gfx::SizeKeyframe::Create(start_time, value.get_size(),
                                           std::move(timing_function));
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::Rect>) {
    if (value.is_rect()) {
      keyframe = gfx::RectKeyframe::Create(start_time, value.get_rect(),
                                           std::move(timing_function));
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::TransformOperations>) {
    if (value.is_transform()) {
      keyframe = gfx::TransformKeyframe::Create(
          start_time, DeserializeTransformOperations(value.get_transform()),
          std::move(timing_function));
    }
  } else {
    static_assert(false, "Unsupported curve type");
  }

  if (!keyframe) {
    return base::unexpected("Invalid keyframe value");
  }
  return keyframe;
}

cc::KeyframeModel::Direction DeserializeAnimationDirection(
    mojom::AnimationDirection direction) {
  switch (direction) {
    case mojom::AnimationDirection::kNormal:
      return cc::KeyframeModel::Direction::NORMAL;
    case mojom::AnimationDirection::kReverse:
      return cc::KeyframeModel::Direction::REVERSE;
    case mojom::AnimationDirection::kAlternateNormal:
      return cc::KeyframeModel::Direction::ALTERNATE_NORMAL;
    case mojom::AnimationDirection::kAlternateReverse:
      return cc::KeyframeModel::Direction::ALTERNATE_REVERSE;
  }
}

cc::KeyframeModel::FillMode DeserializeAnimationFillMode(
    mojom::AnimationFillMode fill_mode) {
  switch (fill_mode) {
    case mojom::AnimationFillMode::kNone:
      return cc::KeyframeModel::FillMode::NONE;
    case mojom::AnimationFillMode::kForwards:
      return cc::KeyframeModel::FillMode::FORWARDS;
    case mojom::AnimationFillMode::kBackwards:
      return cc::KeyframeModel::FillMode::BACKWARDS;
    case mojom::AnimationFillMode::kBoth:
      return cc::KeyframeModel::FillMode::BOTH;
    case mojom::AnimationFillMode::kAuto:
      return cc::KeyframeModel::FillMode::AUTO;
  }
}

template <typename CurveType>
base::expected<void, std::string> DeserializeAnimationCurve(
    const mojom::AnimationKeyframeModel& wire,
    cc::Animation& animation) {
  auto curve = CurveType::Create();
  curve->SetTimingFunction(DeserializeTimingFunction(*wire.timing_function));
  curve->set_scaled_duration(wire.scaled_duration);
  for (const auto& wire_keyframe : wire.keyframes) {
    std::unique_ptr<gfx::TimingFunction> keyframe_timing_function;
    if (wire_keyframe->timing_function) {
      keyframe_timing_function =
          DeserializeTimingFunction(*wire_keyframe->timing_function);
    }
    ASSIGN_OR_RETURN(auto keyframe,
                     DeserializeKeyframe<CurveType>(
                         *wire_keyframe->value, wire_keyframe->start_time,
                         std::move(keyframe_timing_function)));
    curve->AddKeyframe(std::move(keyframe));
  }

  auto model = cc::KeyframeModel::Create(
      std::move(curve), wire.id, wire.group_id,
      cc::KeyframeModel::TargetPropertyId(wire.target_property_type));
  model->set_direction(DeserializeAnimationDirection(wire.direction));
  model->set_fill_mode(DeserializeAnimationFillMode(wire.fill_mode));
  model->set_playback_rate(wire.playback_rate);
  model->set_iterations(wire.iterations);
  model->set_iteration_start(wire.iteration_start);
  model->set_time_offset(wire.time_offset);
  model->set_element_id(wire.element_id);
  animation.keyframe_effect()->AddKeyframeModel(std::move(model));
  return base::ok();
}

base::expected<void, std::string> DeserializeAnimation(
    const mojom::Animation& wire,
    cc::AnimationTimeline& timeline) {
  if (timeline.GetAnimationById(wire.id)) {
    return base::unexpected("Unexpected duplicate animation ID");
  }

  scoped_refptr<cc::Animation> animation = cc::Animation::Create(wire.id);
  timeline.AttachAnimation(animation);

  if (wire.element_id) {
    animation->AttachElement(wire.element_id);
  }

  for (const auto& wire_model : wire.keyframe_models) {
    if (wire_model->keyframes.empty()) {
      return base::unexpected("Unexpected anmation with no keyframes");
    }
    // We use the first keyframe to determine the curve type. All keyframes will
    // be validated against this type.
    switch (wire_model->keyframes[0]->value->which()) {
      case mojom::AnimationKeyframeValue::Tag::kScalar:
        RETURN_IF_ERROR(
            DeserializeAnimationCurve<gfx::KeyframedFloatAnimationCurve>(
                *wire_model, *animation));
        break;
      case mojom::AnimationKeyframeValue::Tag::kColor:
        RETURN_IF_ERROR(
            DeserializeAnimationCurve<gfx::KeyframedColorAnimationCurve>(
                *wire_model, *animation));
        break;
      case mojom::AnimationKeyframeValue::Tag::kSize:
        RETURN_IF_ERROR(
            DeserializeAnimationCurve<gfx::KeyframedSizeAnimationCurve>(
                *wire_model, *animation));
        break;
      case mojom::AnimationKeyframeValue::Tag::kRect:
        RETURN_IF_ERROR(
            DeserializeAnimationCurve<gfx::KeyframedRectAnimationCurve>(
                *wire_model, *animation));
        break;
      case mojom::AnimationKeyframeValue::Tag::kTransform:
        RETURN_IF_ERROR(
            DeserializeAnimationCurve<gfx::KeyframedTransformAnimationCurve>(
                *wire_model, *animation));
        break;
    }
  }
  animation->keyframe_effect()->UpdateTickingState();
  return base::ok();
}

base::expected<void, std::string> DeserializeAnimationTimeline(
    const mojom::AnimationTimeline& wire,
    cc::AnimationHost& host) {
  scoped_refptr<cc::AnimationTimeline> timeline = host.GetTimelineById(wire.id);
  if (!timeline) {
    timeline = cc::AnimationTimeline::Create(wire.id);
    host.AddAnimationTimeline(timeline);
  }
  for (int32_t id : wire.removed_animations) {
    if (auto* animation = timeline->GetAnimationById(id)) {
      timeline->DetachAnimation(animation);
    }
  }
  for (const auto& wire_animation : wire.new_animations) {
    RETURN_IF_ERROR(DeserializeAnimation(*wire_animation, *timeline));
  }
  return base::ok();
}

base::expected<void, std::string> DeserializeAnimationUpdates(
    const mojom::LayerTreeUpdate& update,
    cc::AnimationHost& host) {
  if (update.removed_animation_timelines) {
    for (int32_t id : *update.removed_animation_timelines) {
      if (auto* timeline = host.GetTimelineById(id)) {
        host.RemoveAnimationTimeline(timeline);
      }
    }
  }

  if (update.animation_timelines) {
    for (const auto& wire : *update.animation_timelines) {
      RETURN_IF_ERROR(DeserializeAnimationTimeline(*wire, host));
    }
  }

  return base::ok();
}

}  // namespace

LayerContextImpl::LayerContextImpl(CompositorFrameSinkSupport* compositor_sink,
                                   mojom::PendingLayerContext& context)
    : compositor_sink_(compositor_sink),
      receiver_(this, std::move(context.receiver)),
      client_(std::move(context.client)),
      task_runner_provider_(cc::TaskRunnerProvider::CreateForDisplayTree(
          base::SingleThreadTaskRunner::GetCurrentDefault())),
      rendering_stats_(cc::RenderingStatsInstrumentation::Create()),
      host_impl_(
          cc::LayerTreeHostImpl::Create(GetDisplayTreeSettings(),
                                        this,
                                        task_runner_provider_.get(),
                                        rendering_stats_.get(),
                                        /*task_graph_runner=*/nullptr,
                                        animation_host_->CreateImplInstance(),
                                        /*dark_mode_filter=*/nullptr,
                                        GenerateNextDisplayTreeId(),
                                        /*image_worker_task_runner=*/nullptr,
                                        /*scheduling_client=*/nullptr)) {
  CHECK(host_impl_->InitializeFrameSink(this));
}

LayerContextImpl::~LayerContextImpl() {
  host_impl_->ReleaseLayerTreeFrameSink();
}

void LayerContextImpl::BeginFrame(const BeginFrameArgs& args) {
  // TODO(rockot): Manage these flags properly.
  const bool has_damage = true;
  compositor_sink_->SetLayerContextWantsBeginFrames(false);
  if (!host_impl_->CanDraw()) {
    return;
  }

  host_impl_->WillBeginImplFrame(args);

  cc::LayerTreeHostImpl::FrameData frame;
  frame.begin_frame_ack = BeginFrameAck(args, has_damage);
  frame.origin_begin_main_frame_args = args;
  host_impl_->PrepareToDraw(&frame);
  host_impl_->DrawLayers(&frame);
  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(args);
}

void LayerContextImpl::ReturnResources(
    std::vector<ReturnedResource> resources) {
  // TODO(crbug.com/40902503): Release resources at some point.
  NOTIMPLEMENTED();
}

void LayerContextImpl::DidLoseLayerTreeFrameSinkOnImplThread() {
  NOTREACHED();
}

void LayerContextImpl::SetBeginFrameSource(BeginFrameSource* source) {}

void LayerContextImpl::DidReceiveCompositorFrameAckOnImplThread() {
  NOTIMPLEMENTED();
}

void LayerContextImpl::OnCanDrawStateChanged(bool can_draw) {}

void LayerContextImpl::NotifyReadyToActivate() {}

bool LayerContextImpl::IsReadyToActivate() {
  return false;
}

void LayerContextImpl::NotifyReadyToDraw() {}

void LayerContextImpl::SetNeedsRedrawOnImplThread() {
  compositor_sink_->SetLayerContextWantsBeginFrames(true);
}

void LayerContextImpl::SetNeedsOneBeginImplFrameOnImplThread() {
  compositor_sink_->SetLayerContextWantsBeginFrames(true);
}

void LayerContextImpl::SetNeedsUpdateDisplayTreeOnImplThread() {
  NOTREACHED();
}

void LayerContextImpl::SetNeedsPrepareTilesOnImplThread() {
  NOTREACHED();
}

void LayerContextImpl::SetNeedsCommitOnImplThread() {
  NOTIMPLEMENTED();
}

void LayerContextImpl::SetVideoNeedsBeginFrames(bool needs_begin_frames) {}

void LayerContextImpl::SetDeferBeginMainFrameFromImpl(
    bool defer_begin_main_frame) {}

bool LayerContextImpl::IsInsideDraw() {
  return false;
}

void LayerContextImpl::RenewTreePriority() {}

void LayerContextImpl::PostDelayedAnimationTaskOnImplThread(
    base::OnceClosure task,
    base::TimeDelta delay) {}

void LayerContextImpl::DidActivateSyncTree() {}

void LayerContextImpl::DidPrepareTiles() {}

void LayerContextImpl::DidCompletePageScaleAnimationOnImplThread() {}

void LayerContextImpl::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {}

void LayerContextImpl::SetNeedsImplSideInvalidation(
    bool needs_first_draw_on_activation) {}

void LayerContextImpl::NotifyImageDecodeRequestFinished(int request_id,
                                                        bool decode_succeeded) {
}

void LayerContextImpl::NotifyTransitionRequestFinished(uint32_t sequence_id) {}

void LayerContextImpl::DidPresentCompositorFrameOnImplThread(
    uint32_t frame_token,
    cc::PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
    const FrameTimingDetails& details) {
  NOTIMPLEMENTED();
}

void LayerContextImpl::NotifyAnimationWorkletStateChange(
    cc::AnimationWorkletMutationState state,
    cc::ElementListType element_list_type) {}

void LayerContextImpl::NotifyPaintWorkletStateChange(
    cc::Scheduler::PaintWorkletState state) {}

void LayerContextImpl::NotifyThroughputTrackerResults(
    cc::CustomTrackerResults results) {}

bool LayerContextImpl::IsInSynchronousComposite() const {
  return false;
}

void LayerContextImpl::FrameSinksToThrottleUpdated(
    const base::flat_set<FrameSinkId>& ids) {}

void LayerContextImpl::ClearHistory() {}

void LayerContextImpl::SetHasActiveThreadedScroll(bool is_scrolling) {}

void LayerContextImpl::SetWaitingForScrollEvent(bool waiting_for_scroll_event) {
}

size_t LayerContextImpl::CommitDurationSampleCountForTesting() const {
  return 0;
}

void LayerContextImpl::DidObserveFirstScrollDelay(
    int source_frame_number,
    base::TimeDelta first_scroll_delay,
    base::TimeTicks first_scroll_timestamp) {}

bool LayerContextImpl::BindToClient(cc::LayerTreeFrameSinkClient* client) {
  frame_sink_client_ = client;
  return true;
}

void LayerContextImpl::DetachFromClient() {
  frame_sink_client_ = nullptr;
}

void LayerContextImpl::SetLocalSurfaceId(
    const LocalSurfaceId& local_surface_id) {
  host_impl_->SetTargetLocalSurfaceId(local_surface_id);
}

void LayerContextImpl::SubmitCompositorFrame(CompositorFrame frame,
                                             bool hit_test_data_changed) {
  if (!host_impl_->target_local_surface_id().is_valid()) {
    return;
  }

  frame.resource_list.insert(frame.resource_list.end(),
                             next_frame_resources_.begin(),
                             next_frame_resources_.end());
  next_frame_resources_.clear();
  compositor_sink_->SubmitCompositorFrame(host_impl_->target_local_surface_id(),
                                          std::move(frame));

  constexpr bool start_ready_animations = true;
  host_impl_->UpdateAnimationState(start_ready_animations);
}

void LayerContextImpl::DidNotProduceFrame(const BeginFrameAck& ack,
                                          cc::FrameSkippedReason reason) {
  compositor_sink_->DidNotProduceFrame(ack);
}

void LayerContextImpl::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const SharedBitmapId& id) {}

void LayerContextImpl::DidDeleteSharedBitmap(const SharedBitmapId& id) {}

void LayerContextImpl::DidAppendQuadsWithResources(
    const std::vector<TransferableResource>& resources) {
  next_frame_resources_.insert(next_frame_resources_.end(), resources.begin(),
                               resources.end());
}

void LayerContextImpl::SetVisible(bool visible) {
  host_impl_->SetVisible(visible);
}

void LayerContextImpl::UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) {
  auto result = DoUpdateDisplayTree(std::move(update));
  if (!result.has_value()) {
    receiver_.ReportBadMessage(result.error());
  }
}

base::expected<void, std::string> LayerContextImpl::DoUpdateDisplayTree(
    mojom::LayerTreeUpdatePtr update) {
  cc::LayerTreeImpl& layers = *host_impl_->active_tree();

  // We resize all property trees first, as layers and property tree nodes
  // themselves may index one or more other property tree nodes. These indices
  // need to be validated, and the dependency can be cyclic (e.g. scroll nodes
  // may index transform nodes and transform nodes may index scroll nodes).
  cc::PropertyTrees& property_trees = *layers.property_trees();
  const bool transform_size_changed = ResizePropertyTree(
      property_trees.transform_tree_mutable(), update->num_transform_nodes);
  const bool clip_size_changed = ResizePropertyTree(
      property_trees.clip_tree_mutable(), update->num_clip_nodes);
  const bool effect_size_changed = ResizePropertyTree(
      property_trees.effect_tree_mutable(), update->num_effect_nodes);
  const bool scroll_size_changed = ResizePropertyTree(
      property_trees.scroll_tree_mutable(), update->num_scroll_nodes);

  // Transform tree properties need to update before its nodes are updated, as
  // the nodes may index properties on the tree itself (e.g. scroll
  // constraints). Note that these properties may also index scroll and
  // transform nodes, so they must be deserialized after the trees are resized
  // above.
  bool transform_properties_changed = false;
  if (update->transform_tree_update) {
    transform_properties_changed = true;
    RETURN_IF_ERROR(UpdateTransformTreeProperties(
        property_trees, property_trees.transform_tree_mutable(),
        *update->transform_tree_update));
  }

  ASSIGN_OR_RETURN(const bool transform_nodes_changed,
                   UpdatePropertyTree(property_trees,
                                      property_trees.transform_tree_mutable(),
                                      update->transform_nodes));
  ASSIGN_OR_RETURN(
      const bool clip_nodes_changed,
      UpdatePropertyTree(property_trees, property_trees.clip_tree_mutable(),
                         update->clip_nodes));
  ASSIGN_OR_RETURN(
      const bool effect_nodes_changed,
      UpdatePropertyTree(property_trees, property_trees.effect_tree_mutable(),
                         update->effect_nodes));
  ASSIGN_OR_RETURN(
      const bool scroll_nodes_changed,
      UpdatePropertyTree(property_trees, property_trees.scroll_tree_mutable(),
                         update->scroll_nodes));

  RETURN_IF_ERROR(
      CreateOrUpdateLayers(*this, update->layers, update->layer_order, layers));

  if (update->local_surface_id_from_parent) {
    host_impl_->SetTargetLocalSurfaceId(*update->local_surface_id_from_parent);
  }

  for (const auto& tiling : update->tilings) {
    if (cc::LayerImpl* layer = layers.LayerById(tiling->layer_id)) {
      if (layer->GetLayerType() != cc::mojom::LayerType::kTileDisplay) {
        return base::unexpected("Invalid tile update");
      }
      RETURN_IF_ERROR(DeserializeTiling(
          static_cast<cc::TileDisplayLayerImpl&>(*layer), *tiling));
    }
  }

  layers.set_background_color(update->background_color);
  layers.set_source_frame_number(update->source_frame_number);
  layers.set_trace_id(
      cc::BeginMainFrameTraceId::FromUnsafeValue(update->trace_id));
  layers.SetDeviceViewportRect(update->device_viewport);
  if (update->device_scale_factor <= 0) {
    return base::unexpected("Invalid device scale factor");
  }
  layers.SetDeviceScaleFactor(update->device_scale_factor);
  if (update->painted_device_scale_factor <= 0) {
    return base::unexpected("Invalid painted device scale factor");
  }
  layers.set_painted_device_scale_factor(update->painted_device_scale_factor);
  if (update->local_surface_id_from_parent) {
    layers.SetLocalSurfaceIdFromParent(*update->local_surface_id_from_parent);
  }

  RETURN_IF_ERROR(UpdateViewportPropertyIds(layers, property_trees, *update));

  property_trees.UpdateChangeTracking();
  property_trees.transform_tree_mutable().set_needs_update(
      transform_size_changed || transform_properties_changed ||
      transform_nodes_changed ||
      property_trees.transform_tree().needs_update());
  property_trees.clip_tree_mutable().set_needs_update(
      clip_size_changed || clip_nodes_changed ||
      property_trees.clip_tree().needs_update());
  property_trees.effect_tree_mutable().set_needs_update(
      effect_size_changed || effect_nodes_changed ||
      property_trees.effect_tree().needs_update());

  const bool any_tree_changed =
      transform_size_changed || transform_nodes_changed || clip_size_changed ||
      clip_nodes_changed || effect_size_changed || effect_nodes_changed ||
      scroll_size_changed || scroll_nodes_changed;
  property_trees.set_changed(any_tree_changed);
  if (any_tree_changed) {
    property_trees.ResetCachedData();
  }

  std::vector<std::unique_ptr<cc::RenderSurfaceImpl>> old_render_surfaces;
  property_trees.effect_tree_mutable().TakeRenderSurfaces(&old_render_surfaces);
  const bool render_surfaces_changed =
      property_trees.effect_tree_mutable().CreateOrReuseRenderSurfaces(
          &old_render_surfaces, &layers);
  if (effect_size_changed || render_surfaces_changed) {
    // TODO(rockot): Forcing draw property updates here isn't strictly necessary
    // when `effect_size_changed` is true unless it's because we've removed at
    // least one EffectNode that was inducing a render surface.
    layers.set_needs_update_draw_properties();
  }

  // Safe down-cast: AnimationHost is the only subclass of MutatorHost.
  auto* animation_host = static_cast<cc::AnimationHost*>(layers.mutator_host());
  RETURN_IF_ERROR(DeserializeAnimationUpdates(*update, *animation_host));
  host_impl_->ActivateAnimations();

  compositor_sink_->SetLayerContextWantsBeginFrames(true);
  return base::ok();
}

void LayerContextImpl::UpdateDisplayTiling(mojom::TilingPtr tiling) {
  cc::LayerTreeImpl& layers = *host_impl_->active_tree();
  if (cc::LayerImpl* layer = layers.LayerById(tiling->layer_id)) {
    if (layer->GetLayerType() != cc::mojom::LayerType::kTileDisplay) {
      receiver_.ReportBadMessage("Invalid tile update");
      return;
    }

    auto result = DeserializeTiling(
        static_cast<cc::TileDisplayLayerImpl&>(*layer), *tiling);
    if (!result.has_value()) {
      receiver_.ReportBadMessage(result.error());
      return;
    }
  }
}

}  // namespace viz
