// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/expected_macros.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/debug/layer_tree_debug_state.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/input/browser_controls_offset_manager.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/mirror_layer_impl.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/tile_display_layer_impl.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/layer_tree_settings.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/task_runner_provider.h"
#include "components/viz/client/client_resource_provider.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

namespace viz {

namespace {

#define RETURN_IF_FALSE(expr, error)  \
  do {                                \
    if (!(expr)) {                    \
      return base::unexpected(error); \
    }                                 \
  } while (false)

int GenerateNextDisplayTreeId() {
  static int next_id = 1;
  return next_id++;
}

cc::LayerTreeSettings GetDisplayTreeSettings(
    mojom::LayerContextSettingsPtr remote_settings) {
  cc::LayerTreeSettings settings;
  settings.use_layer_lists = true;
  settings.trees_in_viz_in_viz_process = true;
  settings.display_tree_draw_mode_is_gpu = remote_settings->draw_mode_is_gpu;
  settings.enable_early_damage_check =
      remote_settings->enable_early_damage_check;
  settings.damaged_frame_limit = remote_settings->damaged_frame_limit;
  settings.scrollbar_animator = remote_settings->scrollbar_animator;
  settings.scrollbar_fade_delay = remote_settings->scrollbar_fade_delay;
  settings.scrollbar_fade_duration = remote_settings->scrollbar_fade_duration;
  settings.scrollbar_thinning_duration =
      remote_settings->scrollbar_thinning_duration;
  settings.idle_thickness_scale = remote_settings->idle_thickness_scale;
  settings.top_controls_show_threshold =
      remote_settings->top_controls_show_threshold;
  settings.top_controls_hide_threshold =
      remote_settings->top_controls_hide_threshold;
  settings.minimum_occlusion_tracking_size =
      remote_settings->minimum_occlusion_tracking_size;
  settings.enable_edge_anti_aliasing =
      remote_settings->enable_edge_anti_aliasing;
  settings.enable_backface_visibility_interop =
      remote_settings->enable_backface_visibility_interop;
  settings.enable_fluent_scrollbar = remote_settings->enable_fluent_scrollbar;
  settings.enable_fluent_overlay_scrollbar =
      remote_settings->enable_fluent_overlay_scrollbar;
  return settings;
}

base::expected<void, std::string> CreateLayer(
    cc::LayerTreeHostImpl& host_impl,
    cc::LayerTreeImpl& tree,
    const mojom::Layer& wire,
    std::unique_ptr<cc::LayerImpl>& layer) {
  cc::mojom::LayerType type = wire.type;
  int id = wire.id;
  switch (type) {
    case cc::mojom::LayerType::kLayer:
      layer = cc::LayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kMirror:
      layer = cc::MirrorLayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kNinePatch:
      layer = cc::NinePatchLayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kNinePatchThumbScrollbar: {
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_nine_patch_thumb_scrollbar_layer_extra(),
          "Invalid layer_extra type for NinePatchThumbScrollbarLayerImpl");
      auto& extra =
          wire.layer_extra->get_nine_patch_thumb_scrollbar_layer_extra();
      cc::ScrollbarOrientation orientation =
          extra->scrollbar_base_extra->is_horizontal_orientation
              ? cc::ScrollbarOrientation::kHorizontal
              : cc::ScrollbarOrientation::kVertical;
      layer = cc::NinePatchThumbScrollbarLayerImpl::Create(
          &tree, id, orientation,
          extra->scrollbar_base_extra->is_left_side_vertical_scrollbar);
      break;
    }

    case cc::mojom::LayerType::kPaintedScrollbar: {
      RETURN_IF_FALSE(wire.layer_extra &&
                          wire.layer_extra->is_painted_scrollbar_layer_extra(),
                      "Invalid layer_extra type for PaintedScrollbarLayerImpl");
      auto& extra = wire.layer_extra->get_painted_scrollbar_layer_extra();
      cc::ScrollbarOrientation orientation =
          extra->scrollbar_base_extra->is_horizontal_orientation
              ? cc::ScrollbarOrientation::kHorizontal
              : cc::ScrollbarOrientation::kVertical;
      layer = cc::PaintedScrollbarLayerImpl::Create(
          &tree, id, orientation,
          extra->scrollbar_base_extra->is_left_side_vertical_scrollbar,
          extra->scrollbar_base_extra->is_overlay_scrollbar);
      break;
    }

    case cc::mojom::LayerType::kTileDisplay:
      layer = std::make_unique<cc::TileDisplayLayerImpl>(tree, id);
      break;

    case cc::mojom::LayerType::kSolidColorScrollbar: {
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_solid_color_scrollbar_layer_extra(),
          "Invalid layer_extra type for SolidColorScrollbarLayerImpl");
      auto& extra = wire.layer_extra->get_solid_color_scrollbar_layer_extra();
      cc::ScrollbarOrientation orientation =
          extra->scrollbar_base_extra->is_horizontal_orientation
              ? cc::ScrollbarOrientation::kHorizontal
              : cc::ScrollbarOrientation::kVertical;
      layer = cc::SolidColorScrollbarLayerImpl::Create(
          &tree, id, orientation, extra->thumb_thickness, extra->track_start,
          extra->scrollbar_base_extra->is_left_side_vertical_scrollbar);
      break;
    }

    case cc::mojom::LayerType::kSurface:
      // The callback is triggered in the renderer side during WillDraw(),
      // and there is no need to do it in viz.
      layer = cc::SurfaceLayerImpl::Create(&tree, id, base::NullCallback());
      break;

    case cc::mojom::LayerType::kSolidColor:
      layer = cc::SolidColorLayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kTexture:
      layer = cc::TextureLayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kUIResource:
      layer = cc::UIResourceLayerImpl::Create(&tree, id);
      break;

    case cc::mojom::LayerType::kViewTransitionContent: {
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_view_transition_content_layer_extra(),
          "Invalid layer_extra type for ViewTransitionContentLayerImpl");
      auto& extra = wire.layer_extra->get_view_transition_content_layer_extra();
      layer = cc::ViewTransitionContentLayerImpl::Create(
          &tree, id, extra->resource_id, extra->is_live_content_layer,
          extra->max_extents_rect);
      break;
    }

    default:
      // TODO(rockot): Support other layer types.
      layer = cc::SolidColorLayerImpl::Create(&tree, id);
      break;
  }
  return base::ok();
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
      node.scroll_offset() != wire.scroll_offset) {
    node.needs_local_transform_update = true;
  }
  node.local = wire.local;
  node.origin = wire.origin;
  node.post_translation = wire.post_translation;
  node.set_to_parent(wire.to_parent);
  node.SetScrollOffset(wire.scroll_offset, cc::DamageReason::kUntracked);
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
    node.anchor_position_scroll_data_id = -1;
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
  node.maximum_animation_scale = wire.maximum_animation_scale;
  node.node_and_ancestors_are_animated_or_invertible =
      wire.node_and_ancestors_are_animated_or_invertible;
  node.is_invertible = wire.is_invertible;
  node.ancestors_are_invertible = wire.ancestors_are_invertible;
  node.node_and_ancestors_are_flat = wire.node_and_ancestors_are_flat;
  node.node_or_ancestors_will_change_transform =
      wire.node_or_ancestors_will_change_transform;

  node.visible_frame_element_id = wire.visible_frame_element_id;

  // Note that we only set |transform_changed| to true and never to false since
  // Viz might not have got a change to run and use it via
  // CalculateRenderProperties(). It should only be cleared in
  // ResetAllChangeTracking() which happens at the end of every frame.
  if (wire.transform_changed) {
    node.SetTransformChanged(cc::DamageReason::kUntracked);
  }
  if (!node.SetDamageReasonsForDeserialization(
          cc::DamageReasonSet::FromEnumBitmask(wire.damage_reasons_bit_mask))) {
    // This error case shouldn't be reachable, since
    // DamageReasonSet::FromEnumBitmask should already ignore any bits outside
    // of the set's range.
    return base::unexpected("Invalid damage_reasons_bit_mask");
  }
  node.moved_by_safe_area_bottom = wire.moved_by_safe_area_bottom;
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
  if (!IsOptionalPropertyTreeIndexValid(
          trees.effect_tree(),
          wire.closest_ancestor_with_cached_render_surface_id)) {
    return base::unexpected(
        "Invalid closest_ancestor_with_cached_render_surface_id for effect "
        "node");
  }
  if (!IsOptionalPropertyTreeIndexValid(
          trees.effect_tree(), wire.closest_ancestor_with_copy_request_id)) {
    return base::unexpected(
        "Invalid closest_ancestor_with_copy_request_id for effect node");
  }
  if (!IsOptionalPropertyTreeIndexValid(
          trees.effect_tree(), wire.closest_ancestor_being_captured_id)) {
    return base::unexpected(
        "Invalid closest_ancestor_being_captured_id for effect node");
  }
  if (!IsOptionalPropertyTreeIndexValid(
          trees.effect_tree(), wire.closest_ancestor_with_shared_element_id)) {
    return base::unexpected(
        "Invalid closest_ancestor_with_shared_element_id for effect node");
  }
  node.transform_id = wire.transform_id;
  node.clip_id = wire.clip_id;
  node.element_id = wire.element_id;
  if (node.element_id) {
    trees.effect_tree_mutable().SetElementIdForNodeId(node.id, node.element_id);
  }
  node.opacity = wire.opacity;

  // Note that we only set |effect_changed| to true and never to false since Viz
  // might not have got a change to run and use it via
  // CalculateRenderProperties(). It should only be cleared in
  // ResetAllChangeTracking() which happens at the end of every frame.
  if (wire.effect_changed) {
    node.effect_changed = true;
  }
  node.render_surface_reason = wire.render_surface_reason;
  node.surface_contents_scale = wire.surface_contents_scale;
  node.subtree_capture_id = wire.subtree_capture_id;
  node.subtree_size = wire.subtree_size;

  if (wire.blend_mode > static_cast<uint32_t>(SkBlendMode::kLastMode)) {
    return base::unexpected("Invalid blend_mode for effect node");
  }
  node.blend_mode = static_cast<SkBlendMode>(wire.blend_mode);
  node.target_id = wire.target_id;
  node.view_transition_target_id = wire.view_transition_target_id;
  node.closest_ancestor_with_cached_render_surface_id =
      wire.closest_ancestor_with_cached_render_surface_id;
  node.closest_ancestor_with_copy_request_id =
      wire.closest_ancestor_with_copy_request_id;
  node.closest_ancestor_being_captured_id =
      wire.closest_ancestor_being_captured_id;
  node.closest_ancestor_with_shared_element_id =
      wire.closest_ancestor_with_shared_element_id;
  node.view_transition_element_resource_id =
      wire.view_transition_element_resource_id;
  node.filters = wire.filters;
  node.backdrop_filters = wire.backdrop_filters;
  node.backdrop_filter_bounds = wire.backdrop_filter_bounds;
  if (wire.backdrop_filter_quality <= 0.0f ||
      wire.backdrop_filter_quality > 1.0f ||
      !std::isfinite(wire.backdrop_filter_quality)) {
    return base::unexpected("Invalid backdrop_filter_quality");
  }
  node.backdrop_filter_quality = wire.backdrop_filter_quality;
  node.backdrop_mask_element_id = wire.backdrop_mask_element_id;
  node.mask_filter_info = wire.mask_filter_info;

  node.cache_render_surface = wire.cache_render_surface;
  node.double_sided = wire.double_sided;
  node.trilinear_filtering = wire.trilinear_filtering;
  node.subtree_hidden = wire.subtree_hidden;
  node.has_potential_filter_animation = wire.has_potential_filter_animation;
  node.has_potential_backdrop_filter_animation =
      wire.has_potential_backdrop_filter_animation;
  node.has_potential_opacity_animation = wire.has_potential_opacity_animation;
  node.subtree_has_copy_request = wire.subtree_has_copy_request;
  node.is_fast_rounded_corner = wire.is_fast_rounded_corner;
  node.may_have_backdrop_effect = wire.may_have_backdrop_effect;
  node.needs_effect_for_2d_scale_transform =
      wire.needs_effect_for_2d_scale_transform;

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
    data.constraints.pixel_snap_offset = wire->pixel_snap_offset;
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
  if (update.page_scale_factor <= 0 ||
      !std::isfinite(update.page_scale_factor)) {
    return base::unexpected("Invalid page_scale_factor");
  }
  if (update.device_scale_factor <= 0 ||
      !std::isfinite(update.device_scale_factor)) {
    return base::unexpected("Invalid device_scale_factor");
  }
  if (update.device_transform_scale_factor <= 0 ||
      !std::isfinite(update.device_transform_scale_factor)) {
    return base::unexpected("Invalid device_transform_scale_factor");
  }
  tree.set_page_scale_factor(update.page_scale_factor);
  tree.set_device_scale_factor(update.device_scale_factor);
  tree.set_device_transform_scale_factor(update.device_transform_scale_factor);
  tree.set_nodes_affected_by_outer_viewport_bounds_delta(
      std::move(update.nodes_affected_by_outer_viewport_bounds_delta));
  tree.set_nodes_affected_by_safe_area_bottom(
      std::move(update.nodes_affected_by_safe_area_bottom));
  ASSIGN_OR_RETURN(
      tree.sticky_position_data(),
      DeserializeStickyPositionData(trees, update.sticky_position_data));
  ASSIGN_OR_RETURN(
      tree.anchor_position_scroll_data(),
      DeserializeAnchorPositionScrollData(update.anchor_position_scroll_data));
  return base::ok();
}

base::expected<bool, std::string> UpdateScrollTreeProperties(
    cc::PropertyTrees& trees,
    cc::ScrollTree& tree,
    const mojom::ScrollTreeUpdate& update) {
  for (auto const& [element_id, overscroll] : update.elastic_overscroll) {
    if (!std::isfinite(overscroll.x()) || !std::isfinite(overscroll.y())) {
      return base::unexpected("Invalid elastic_overscroll");
    }
  }
  tree.synced_scroll_offset_map() = update.synced_scroll_offsets;
  tree.scrolling_contents_cull_rects() = update.scrolling_contents_cull_rects;
  bool elastic_overscroll_changed =
      tree.elastic_overscroll() != update.elastic_overscroll;
  tree.elastic_overscroll() = update.elastic_overscroll;
  return elastic_overscroll_changed;
}

void UpdateMirrorLayerExtra(const mojom::MirrorLayerExtraPtr& extra,
                            cc::MirrorLayerImpl& layer) {
  layer.SetMirroredLayerId(extra->mirrored_layer_id);
}

base::expected<void, std::string> UpdateNinePatchLayerExtra(
    const mojom::NinePatchLayerExtraPtr& extra,
    cc::NinePatchLayerImpl& layer) {
  if (!extra->ui_resource_id) {
    return base::unexpected("Invalid ui_resource_id for NinePatchLayerImpl");
  }
  layer.SetUIResourceId(extra->ui_resource_id);
  layer.SetImageBounds(extra->image_bounds);
  layer.SetLayout(extra->image_aperture, extra->border, extra->layer_occlusion,
                  extra->fill_center);
  layer.SetUV(extra->uv_top_left, extra->uv_bottom_right);
  return base::ok();
}

void UpdateTextureLayerExtra(const mojom::TextureLayerExtraPtr& extra,
                             cc::TextureLayerImpl& layer) {
  layer.SetBlendBackgroundColor(extra->blend_background_color);
  layer.SetForceTextureToOpaque(extra->force_texture_to_opaque);
  layer.SetUVTopLeft(extra->uv_top_left);
  layer.SetUVBottomRight(extra->uv_bottom_right);

  if (extra->transferable_resource) {
    ReleaseCallback release_callback;
    if (!extra->transferable_resource->is_empty()) {
      release_callback = base::BindOnce(
          [](cc::LayerTreeHostImpl* host_impl, ResourceId id,
             const gpu::SyncToken& sync_token, bool is_lost) {
            host_impl->ReturnResource({id, sync_token,
                                       /*release_fence=*/gfx::GpuFenceHandle(),
                                       /*count=*/1, is_lost});
          },
          layer.layer_tree_impl()->host_impl(),
          extra->transferable_resource->id);
    }
    layer.SetTransferableResource(extra->transferable_resource.value(),
                                  std::move(release_callback));
  }
}

base::expected<void, std::string> UpdateUIResourceLayerExtra(
    const mojom::UIResourceLayerExtraPtr& extra,
    cc::UIResourceLayerImpl& layer) {
  if (!extra->ui_resource_id) {
    return base::unexpected("Invalid ui_resource_id for UIResourceLayerImpl");
  }
  layer.SetUIResourceId(extra->ui_resource_id);
  layer.SetImageBounds(extra->image_bounds);
  layer.SetUV(extra->uv_top_left, extra->uv_bottom_right);
  return base::ok();
}

void UpdateScrollbarLayerBaseExtra(
    const mojom::ScrollbarLayerBaseExtraPtr& extra,
    cc::ScrollbarLayerImplBase& layer) {
  // ScrollbarLayerImplBase properties
  layer.SetScrollElementId(extra->scroll_element_id);
  layer.set_is_overlay_scrollbar(extra->is_overlay_scrollbar);
  layer.set_is_web_test(extra->is_web_test);
  layer.SetThumbThicknessScaleFactor(extra->thumb_thickness_scale_factor);
  layer.SetCurrentPos(extra->current_pos);
  layer.SetClipLayerLength(extra->clip_layer_length);
  layer.SetScrollLayerLength(extra->scroll_layer_length);
  layer.SetVerticalAdjust(extra->vertical_adjust);
  layer.SetHasFindInPageTickmarks(extra->has_find_in_page_tickmarks);
}

void UpdateNinePatchThumbScrollbarLayerExtra(
    const mojom::NinePatchThumbScrollbarLayerExtraPtr& extra,
    cc::NinePatchThumbScrollbarLayerImpl& layer) {
  UpdateScrollbarLayerBaseExtra(
      extra->scrollbar_base_extra,
      static_cast<cc::ScrollbarLayerImplBase&>(layer));

  layer.SetThumbThickness(extra->thumb_thickness);
  layer.SetThumbLength(extra->thumb_length);
  layer.SetTrackStart(extra->track_start);
  layer.SetTrackLength(extra->track_length);
  layer.SetImageBounds(extra->image_bounds);
  layer.SetAperture(extra->aperture);
  layer.set_thumb_ui_resource_id(extra->thumb_ui_resource_id);
  layer.set_track_and_buttons_ui_resource_id(
      extra->track_and_buttons_ui_resource_id);
}

void UpdatePaintedScrollbarLayerExtra(
    const mojom::PaintedScrollbarLayerExtraPtr& extra,
    cc::PaintedScrollbarLayerImpl& layer) {
  UpdateScrollbarLayerBaseExtra(
      extra->scrollbar_base_extra,
      static_cast<cc::ScrollbarLayerImplBase&>(layer));

  layer.set_internal_contents_scale_and_bounds(extra->internal_contents_scale,
                                               extra->internal_content_bounds);

  layer.SetJumpOnTrackClick(extra->jump_on_track_click);
  layer.SetSupportsDragSnapBack(extra->supports_drag_snap_back);
  layer.SetThumbThickness(extra->thumb_thickness);
  layer.SetThumbLength(extra->thumb_length);
  layer.SetBackButtonRect(extra->back_button_rect);
  layer.SetForwardButtonRect(extra->forward_button_rect);
  layer.SetTrackRect(extra->track_rect);

  layer.set_track_and_buttons_ui_resource_id(
      extra->track_and_buttons_ui_resource_id);
  layer.set_thumb_ui_resource_id(extra->thumb_ui_resource_id);
  layer.set_uses_nine_patch_track_and_buttons(
      extra->uses_nine_patch_track_and_buttons);

  layer.SetScrollbarPaintedOpacity(extra->painted_opacity);
  if (extra->thumb_color) {
    layer.SetThumbColor(extra->thumb_color.value());
  }
  layer.SetTrackAndButtonsImageBounds(extra->track_and_buttons_image_bounds);
  layer.SetTrackAndButtonsAperture(extra->track_and_buttons_aperture);
}

void UpdateSolidColorScrollbarLayerExtra(
    const mojom::SolidColorScrollbarLayerExtraPtr& extra,
    cc::SolidColorScrollbarLayerImpl& layer) {
  UpdateScrollbarLayerBaseExtra(
      extra->scrollbar_base_extra,
      static_cast<cc::ScrollbarLayerImplBase&>(layer));
  layer.set_color(extra->color);
  // thumb_thickness has no update method in SolidColorScrollbarLayerImpl
  // so it is intentionally ignored here.
}

void UpdateSurfaceLayerExtra(const mojom::SurfaceLayerExtraPtr& extra,
                             cc::SurfaceLayerImpl& layer) {
  layer.SetRange(extra->surface_range, extra->deadline_in_frames);
  layer.SetStretchContentToFillBounds(extra->stretch_content_to_fill_bounds);
  layer.SetSurfaceHitTestable(extra->surface_hit_testable);
  layer.SetHasPointerEventsNone(extra->has_pointer_events_none);
  layer.SetIsReflection(extra->is_reflection);
  if (extra->will_draw_needs_reset) {
    layer.ResetStateForUpdateSubmissionStateCallback();
  }
  layer.SetOverrideChildPaintFlags(extra->override_child_paint_flags);
}

void UpdateViewTransitionContentLayerExtra(
    const mojom::ViewTransitionContentLayerExtraPtr& extra,
    cc::ViewTransitionContentLayerImpl& layer) {
  layer.SetMaxExtentsRect(extra->max_extents_rect);
}

void UpdateTileDisplayLayerExtra(const mojom::TileDisplayLayerExtraPtr& extra,
                                 cc::TileDisplayLayerImpl& layer) {
  layer.SetSolidColor(extra->solid_color);
  layer.SetIsBackdropFilterMask(extra->is_backdrop_filter_mask);
  layer.SetIsDirectlyCompositedImage(extra->is_directly_composited_image);
  layer.SetNearestNeighbor(extra->nearest_neighbor);
  layer.SetContentColorUsage(extra->content_color_usage);
  layer.SetRecordedBounds(extra->recorded_bounds);
  layer.SetProposedTilingScalesForDeletion(
      extra->proposed_tiling_scales_for_deletion);
}

base::expected<void, std::string> UpdateLayer(const mojom::Layer& wire,
                                              cc::LayerImpl& layer) {
  if (wire.type != layer.GetLayerType()) {
    return base::unexpected("Incorrect layer type used in Layer update.");
  }
  if (wire.contents_opaque && !wire.contents_opaque_for_text) {
    return base::unexpected(
        "Invalid contents_opaque_for_text: cannot be false if contents_opaque "
        "is true.");
  }
  if (wire.safe_opaque_background_color.isOpaque() != wire.contents_opaque) {
    return base::unexpected(
        "Invalid safe_opaque_background_color: opaqueness must agree with "
        "contents_opaque");
  }

  layer.SetBounds(wire.bounds);
  layer.SetContentsOpaque(wire.contents_opaque);
  layer.SetContentsOpaqueForText(wire.contents_opaque_for_text);
  layer.SetDrawsContent(wire.is_drawable);
  if (wire.layer_property_changed_not_from_property_trees) {
    layer.NoteLayerPropertyChanged();
  }
  if (wire.layer_property_changed_from_property_trees) {
    layer.NoteLayerPropertyChangedFromPropertyTrees();
  }
  layer.SetBackgroundColor(wire.background_color);
  layer.SetSafeOpaqueBackgroundColor(wire.safe_opaque_background_color);
  layer.SetHitTestOpaqueness(wire.hit_test_opaqueness);
  layer.SetElementId(wire.element_id);
  layer.UnionUpdateRect(wire.update_rect);
  layer.SetOffsetToTransformParent(wire.offset_to_transform_parent);
  layer.SetShouldCheckBackfaceVisibility(wire.should_check_backface_visibility);
  if (wire.rare_properties) {
    layer.SetFilterQuality(wire.rare_properties->filter_quality);
    layer.SetDynamicRangeLimit(wire.rare_properties->dynamic_range_limit);
    layer.SetCaptureBounds(wire.rare_properties->capture_bounds);
  }

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

  switch (wire.type) {
    case cc::mojom::LayerType::kMirror:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_mirror_layer_extra(),
          "Invalid layer_extra type for MirrorLayerImpl");
      UpdateMirrorLayerExtra(wire.layer_extra->get_mirror_layer_extra(),
                             static_cast<cc::MirrorLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kNinePatch:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_nine_patch_layer_extra(),
          "Invalid layer_extra type for NinePatchLayerImpl");
      RETURN_IF_ERROR(UpdateNinePatchLayerExtra(
          wire.layer_extra->get_nine_patch_layer_extra(),
          static_cast<cc::NinePatchLayerImpl&>(layer)));
      break;
    case cc::mojom::LayerType::kNinePatchThumbScrollbar:
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_nine_patch_thumb_scrollbar_layer_extra(),
          "Invalid layer_extra type for NinePatchThumbScrollbarLayerImpl");
      UpdateNinePatchThumbScrollbarLayerExtra(
          wire.layer_extra->get_nine_patch_thumb_scrollbar_layer_extra(),
          static_cast<cc::NinePatchThumbScrollbarLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kPaintedScrollbar:
      RETURN_IF_FALSE(wire.layer_extra &&
                          wire.layer_extra->is_painted_scrollbar_layer_extra(),
                      "Invalid layer_extra type for PaintedScrollbarLayerImpl");
      UpdatePaintedScrollbarLayerExtra(
          wire.layer_extra->get_painted_scrollbar_layer_extra(),
          static_cast<cc::PaintedScrollbarLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kSolidColorScrollbar:
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_solid_color_scrollbar_layer_extra(),
          "Invalid layer_extra type for SolidColorScrollbarLayerImpl");
      UpdateSolidColorScrollbarLayerExtra(
          wire.layer_extra->get_solid_color_scrollbar_layer_extra(),
          static_cast<cc::SolidColorScrollbarLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kSurface:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_surface_layer_extra(),
          "Invalid layer_extra type for SurfaceLayerImpl");
      UpdateSurfaceLayerExtra(wire.layer_extra->get_surface_layer_extra(),
                              static_cast<cc::SurfaceLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kTexture:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_texture_layer_extra(),
          "Invalid layer_extra type for TextureLayerImpl");
      UpdateTextureLayerExtra(wire.layer_extra->get_texture_layer_extra(),
                              static_cast<cc::TextureLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kTileDisplay:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_tile_display_layer_extra(),
          "Invalid layer_extra type for TileDisplayLayerImpl");
      UpdateTileDisplayLayerExtra(
          wire.layer_extra->get_tile_display_layer_extra(),
          static_cast<cc::TileDisplayLayerImpl&>(layer));
      break;
    case cc::mojom::LayerType::kUIResource:
      RETURN_IF_FALSE(
          wire.layer_extra && wire.layer_extra->is_ui_resource_layer_extra(),
          "Invalid layer_extra type for UIResourceLayerImpl");
      RETURN_IF_ERROR(UpdateUIResourceLayerExtra(
          wire.layer_extra->get_ui_resource_layer_extra(),
          static_cast<cc::UIResourceLayerImpl&>(layer)));
      break;
    case cc::mojom::LayerType::kViewTransitionContent:
      RETURN_IF_FALSE(
          wire.layer_extra &&
              wire.layer_extra->is_view_transition_content_layer_extra(),
          "Invalid layer_extra type for ViewTransitionContentLayerImpl");
      UpdateViewTransitionContentLayerExtra(
          wire.layer_extra->get_view_transition_content_layer_extra(),
          static_cast<cc::ViewTransitionContentLayerImpl&>(layer));
      break;
    default:
      // TODO(zmo): handle other types of LayerImpl.
      break;
  }
  return base::ok();
}

base::expected<void, std::string> CreateOrUpdateLayers(
    cc::LayerTreeHostImpl& host_impl,
    const std::vector<mojom::LayerPtr>& updates,
    std::optional<std::vector<int32_t>>& layer_order,
    cc::LayerTreeImpl& layers) {
  TRACE_EVENT1("viz", "CreateOrUpdateLayers", "LayerCount", updates.size());
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
      RETURN_IF_ERROR(CreateLayer(host_impl, layers, *wire, layer));
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
DeserializeTileResource(cc::LayerTreeHostImpl* host_impl,
                        mojom::TileResource& wire) {
  if (wire.resource.id == kInvalidResourceId) {
    return base::unexpected("Invalid tile resource");
  }

  ReleaseCallback release_callback = base::BindOnce(
      [](cc::LayerTreeHostImpl* host_impl, ResourceId id,
         const gpu::SyncToken& sync_token, bool is_lost) {
        host_impl->ReturnResource({id, sync_token,
                                   /*release_fence=*/gfx::GpuFenceHandle(),
                                   /*count=*/1, is_lost});
      },
      host_impl, wire.resource.id);

  auto resource_id = host_impl->resource_provider()->ImportResource(
      wire.resource,
      /*impl_release_callback=*/std::move(release_callback),
      /*main_thread_release_callback=*/base::NullCallback(),
      /*evicted_callback=*/base::NullCallback());

  return cc::TileDisplayLayerImpl::TileResource(resource_id,
                                                wire.resource.GetSize());
}

base::expected<cc::TileDisplayLayerImpl::TileContents, std::string>
DeserializeTileContents(cc::LayerTreeHostImpl* host_impl,
                        mojom::TileContents& wire) {
  switch (wire.which()) {
    case mojom::TileContents::Tag::kMissingReason:
      return cc::TileDisplayLayerImpl::TileContents(
          cc::TileDisplayLayerImpl::NoContents(wire.get_missing_reason()));

    case mojom::TileContents::Tag::kResource:
      return DeserializeTileResource(host_impl, *wire.get_resource());

    case mojom::TileContents::Tag::kSolidColor:
      return cc::TileDisplayLayerImpl::TileContents(wire.get_solid_color());
  }
}

base::expected<void, std::string> DeserializeTiling(
    cc::LayerTreeHostImpl* host_impl,
    cc::TileDisplayLayerImpl& layer,
    mojom::Tiling& wire,
    bool update_damage) {
  if (wire.is_deleted) {
    layer.RemoveTiling(wire.scale_key);
    return base::ok();
  }
  if (wire.tile_size.width() <= 0 || wire.tile_size.height() <= 0) {
    return base::unexpected("Invalid tile_size dimensions in Tiling");
  }

  const float scale_key =
      std::max(wire.raster_scale.x(), wire.raster_scale.y());
  auto& tiling = layer.GetOrCreateTilingFromScaleKey(scale_key);
  tiling.SetRasterTransform(gfx::AxisTransform2d::FromScaleAndTranslation(
      wire.raster_scale, wire.raster_translation));
  tiling.SetTileSize(wire.tile_size);
  tiling.SetTilingRect(wire.tiling_rect);
  for (auto& wire_tile : wire.tiles) {
    ASSIGN_OR_RETURN(auto contents,
                     DeserializeTileContents(host_impl, *wire_tile->contents));
    tiling.SetTileContents(
        cc::TileIndex{base::saturated_cast<int>(wire_tile->column_index),
                      base::saturated_cast<int>(wire_tile->row_index)},
        std::move(contents), update_damage);
  }
  if (tiling.tiles().empty()) {
    layer.RemoveTiling(tiling.contents_scale_key());
  }
  return base::ok();
}

void DeserializeViewTransitionRequests(
    cc::LayerTreeImpl& layers,
    std::vector<mojom::ViewTransitionRequestPtr>& wire_data) {
  for (auto& wire : wire_data) {
    std::unique_ptr<cc::ViewTransitionRequest> request;
    switch (wire->type) {
      case mojom::CompositorFrameTransitionDirectiveType::kSave:
        // Callback is not used at all in
        // ViewTransitionRequest::ConstructDirective, therefore it's not
        // wired in mojom::ViewTransitionRequest to viz, and here we just use
        // a placeholder.
        request = cc::ViewTransitionRequest::CreateCapture(
            wire->transition_token, wire->maybe_cross_frame_sink,
            wire->capture_resource_ids,
            cc::ViewTransitionRequest::ViewTransitionCaptureCallback());
        break;
      case mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer:
        request = cc::ViewTransitionRequest::CreateAnimateRenderer(
            wire->transition_token, wire->maybe_cross_frame_sink);
        break;
      case mojom::CompositorFrameTransitionDirectiveType::kRelease:
        request = cc::ViewTransitionRequest::CreateRelease(
            wire->transition_token, wire->maybe_cross_frame_sink);
        break;
    }
    request->set_sequence_id(wire->sequence_id);
    layers.AddViewTransitionRequest(std::move(request));
  }
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
    } else {
      return base::unexpected("Invalid keyframe type");
    }
  } else if constexpr (std::is_same_v<ValueType, SkColor>) {
    if (value.is_color()) {
      keyframe = gfx::ColorKeyframe::Create(start_time, value.get_color(),
                                            std::move(timing_function));
    } else {
      return base::unexpected("Invalid keyframe type");
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::SizeF>) {
    if (value.is_size()) {
      keyframe = gfx::SizeKeyframe::Create(start_time, value.get_size(),
                                           std::move(timing_function));
    } else {
      return base::unexpected("Invalid keyframe type");
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::Rect>) {
    if (value.is_rect()) {
      keyframe = gfx::RectKeyframe::Create(start_time, value.get_rect(),
                                           std::move(timing_function));
    } else {
      return base::unexpected("Invalid keyframe type");
    }
  } else if constexpr (std::is_same_v<ValueType, gfx::TransformOperations>) {
    if (value.is_transform()) {
      keyframe = gfx::TransformKeyframe::Create(
          start_time, DeserializeTransformOperations(value.get_transform()),
          std::move(timing_function));
    } else {
      return base::unexpected("Invalid keyframe type");
    }
  } else {
    static_assert(false, "Unsupported curve type");
  }

  if (!keyframe) {
    // This case handles failures from `gfx::Keyframe::Create` calls above
    // if the value was of the correct type but otherwise invalid, or if a
    // new `ValueType` is added to the system without a corresponding
    // `if constexpr` block and `Create` method here.
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
  if (wire.playback_rate == 0.0) {
    return base::unexpected("Invalid playback_rate: cannot be 0");
  }

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
      return base::unexpected("Unexpected animation with no keyframes");
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
  bool add_new_timeline = false;
  if (!timeline) {
    timeline = cc::AnimationTimeline::Create(wire.id);
    add_new_timeline = true;
  }
  for (int32_t id : wire.removed_animations) {
    if (auto* animation = timeline->GetAnimationById(id)) {
      timeline->DetachAnimation(animation);
    }
  }
  for (const auto& wire_animation : wire.new_animations) {
    RETURN_IF_ERROR(DeserializeAnimation(*wire_animation, *timeline));
  }
  if (add_new_timeline) {
    host.AddAnimationTimeline(timeline);
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
                                   mojom::PendingLayerContext& context,
                                   mojom::LayerContextSettingsPtr settings)
    : LayerContextImpl(compositor_sink,
                       std::move(settings),
                       std::move(context.receiver),
                       std::move(context.client)) {
  // Always expect valid context receiver & client to be passed to the
  // public constructor.
  CHECK(receiver_);
  CHECK(client_);
}

// static
std::unique_ptr<LayerContextImpl> LayerContextImpl::CreateForTesting(
    CompositorFrameSinkSupport* compositor_sink,
    mojom::LayerContextSettingsPtr settings) {
  return base::WrapUnique<LayerContextImpl>(new LayerContextImpl(
      compositor_sink, std::move(settings),
      mojo::PendingAssociatedReceiver<mojom::LayerContext>(),
      mojo::PendingAssociatedRemote<mojom::LayerContextClient>()));
}

LayerContextImpl::LayerContextImpl(
    CompositorFrameSinkSupport* compositor_sink,
    mojom::LayerContextSettingsPtr settings,
    mojo::PendingAssociatedReceiver<mojom::LayerContext> receiver_pipe,
    mojo::PendingAssociatedRemote<mojom::LayerContextClient> client_pipe)
    : compositor_sink_(compositor_sink),
      task_runner_provider_(cc::TaskRunnerProvider::CreateForDisplayTree(
          base::SingleThreadTaskRunner::GetCurrentDefault())),
      rendering_stats_(cc::RenderingStatsInstrumentation::Create()),
      host_impl_(cc::LayerTreeHostImpl::Create(
          GetDisplayTreeSettings(std::move(settings)),
          this,
          task_runner_provider_.get(),
          rendering_stats_.get(),
          /*task_graph_runner=*/nullptr,
          animation_host_->CreateImplInstance(),
          /*dark_mode_filter=*/nullptr,
          GenerateNextDisplayTreeId(),
          /*image_worker_task_runner=*/nullptr,
          /*scheduling_client=*/nullptr)) {
  if (receiver_pipe.is_valid() && client_pipe.is_valid()) {
    receiver_ = std::make_unique<mojo::AssociatedReceiver<mojom::LayerContext>>(
        this, std::move(receiver_pipe));
    client_ =
        std::make_unique<mojo::AssociatedRemote<mojom::LayerContextClient>>(
            std::move(client_pipe));
  }
  CHECK(host_impl_->InitializeFrameSink(this));
}

LayerContextImpl::~LayerContextImpl() {
  DoReturnResources();
  host_impl_->ReleaseLayerTreeFrameSink();
}

void LayerContextImpl::BeginFrame(const BeginFrameArgs& args) {
  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    compositor_sink_->SetLayerContextWantsBeginFrames(false);
    // TODO(zmo): The stage breakdown var is |start_update_display_tree|.
    // Consider using a difference name, so it works for TreeAnimationsInViz
    // mode as well.
    base::TimeTicks start_begin_frame = base::TimeTicks::Now();
    DoDrawInternal(args, start_begin_frame);
  }
}

void LayerContextImpl::ReceiveReturnsFromParent(
    std::vector<ReturnedResource> resources) {
  // Impl and Main thread task runners are the same. They bind to the viz
  // thread.
  auto* task_runner = task_runner_provider_->MainThreadTaskRunner();
  if (!task_runner->BelongsToCurrentThread()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&LayerContextImpl::ReceiveReturnsFromParent,
                       weak_factory_.GetWeakPtr(), std::move(resources)));
    return;
  }
  host_impl_->resource_provider()->ReceiveReturnsFromParent(
      std::move(resources));
  DoReturnResources();
}

void LayerContextImpl::DoReturnResources() {
  if (!resources_to_return_.empty()) {
    compositor_sink_->DoReturnResources(std::move(resources_to_return_));
  }
}

void LayerContextImpl::HandleBadMojoMessage(const std::string& function,
                                            const std::string& error) {
  receiver_->ReportBadMessage(function + "() : " + error);
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
  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    compositor_sink_->SetLayerContextWantsBeginFrames(true);
  }
}

void LayerContextImpl::SetNeedsOneBeginImplFrameOnImplThread() {
  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    compositor_sink_->SetLayerContextWantsBeginFrames(true);
  }
}

void LayerContextImpl::SetNeedsPrepareTilesOnImplThread() {
  NOTREACHED();
}

void LayerContextImpl::SetNeedsCommitOnImplThread(bool urgent) {
  NOTIMPLEMENTED();
}

void LayerContextImpl::SetVideoNeedsBeginFrames(bool needs_begin_frames) {}
void LayerContextImpl::DidChangeBeginFrameSourcePaused(bool paused) {}

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
                                                        bool speculative,
                                                        bool decode_succeeded) {
}

void LayerContextImpl::NotifyTransitionRequestFinished(
    uint32_t sequence_id,
    const ViewTransitionElementResourceRects&) {}

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

void LayerContextImpl::NotifyCompositorMetricsTrackerResults(
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

void LayerContextImpl::ReturnResource(ReturnedResource returned_resource) {
  resources_to_return_.emplace_back(std::move(returned_resource));
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
  // There are a few places that calls this. One is from LayerTreeHostImpl in
  // TreesInViz mode in viz process, and it's unnecessary to call it. The
  // others are from ui/aura/window.cc, and their frame_sink_ should not be
  // LayerContextImpl.
  NOTREACHED();
}

void LayerContextImpl::SubmitCompositorFrame(CompositorFrame frame,
                                             bool hit_test_data_changed) {
  if (!host_impl_->GetCurrentLocalSurfaceId().is_valid()) {
    return;
  }

  std::optional<HitTestRegionList> hit_test_region_list =
      host_impl_->BuildHitTestData();

  // TODO(vmiura): Implement other functionality from
  // AsyncLayerTreeFrameSink::SubmitCompositorFrame()

  auto result = compositor_sink_->MaybeSubmitCompositorFrame(
      host_impl_->GetCurrentLocalSurfaceId(), std::move(frame),
      std::move(hit_test_region_list), 0);
  if (result != SubmitResult::ACCEPTED) {
    client_->ResetWithReason(
        static_cast<uint32_t>(result),
        CompositorFrameSinkSupport::GetSubmitResultAsString(result));
    return;
  }

  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    constexpr bool start_ready_animations = true;
    host_impl_->UpdateAnimationState(start_ready_animations);
  }
}

void LayerContextImpl::DidNotProduceFrame(const BeginFrameAck& ack,
                                          cc::FrameSkippedReason reason) {
  compositor_sink_->DidNotProduceFrame(ack);
}

void LayerContextImpl::NotifyNewLocalSurfaceIdExpectedWhilePaused() {
  compositor_sink_->NotifyNewLocalSurfaceIdExpectedWhilePaused();
}

void LayerContextImpl::SetVisible(bool visible) {
  host_impl_->SetVisible(visible);
}

void LayerContextImpl::UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) {
  CHECK(receiver_);

  const BeginFrameArgs begin_frame_args = update->begin_frame_args;
  auto start_update_display_tree = base::TimeTicks::Now();
  const bool frame_has_damage = update->frame_has_damage;
  auto result = DoUpdateDisplayTree(std::move(update));
  if (!result.has_value()) {
    HandleBadMojoMessage("UpdateDisplayTree", result.error());
    return;
  }

  // After a tree update, either Draw or schedule animations.
  DoDraw(begin_frame_args, start_update_display_tree, frame_has_damage);

  // We may have resources to return after a tree update and draw.
  DoReturnResources();
}

base::expected<void, std::string> LayerContextImpl::DoUpdateDisplayTree(
    mojom::LayerTreeUpdatePtr update) {
  TRACE_EVENT0("viz", "LayerContextImpl::DoUpdateDisplayTree");
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

  bool scroll_properties_changed = false;
  if (update->scroll_tree_update) {
    ASSIGN_OR_RETURN(scroll_properties_changed,
                     UpdateScrollTreeProperties(
                         property_trees, property_trees.scroll_tree_mutable(),
                         *update->scroll_tree_update));
    if (scroll_properties_changed) {
      layers.set_needs_update_draw_properties();
    }
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

  // Pull any copy output requests that came in over the wire.
  for (const auto& wire : update->effect_nodes) {
    for (auto&& copy_request : wire->copy_output_requests) {
      property_trees.effect_tree_mutable().AddCopyRequest(
          wire->id, std::move(copy_request));
    }
  }

  if (update->surface_ranges) {
    base::flat_set<SurfaceRange> surface_ranges;
    for (auto& surface_range : *(update->surface_ranges)) {
      surface_ranges.insert(surface_range);
    }
    layers.ClearSurfaceRanges();
    layers.SetSurfaceRanges(surface_ranges);
  }

  if (update->view_transition_requests) {
    DeserializeViewTransitionRequests(layers,
                                      *(update->view_transition_requests));
  }

  RETURN_IF_ERROR(CreateOrUpdateLayers(
      *(this->host_impl_.get()), update->layers, update->layer_order, layers));

  // After layers are updated, validate backdrop_mask_element_id.
  for (const auto& wire : update->effect_nodes) {
    if (wire->backdrop_mask_element_id) {
      if (auto* layer =
              layers.LayerByElementId(wire->backdrop_mask_element_id)) {
        if (layer->GetLayerType() != cc::mojom::LayerType::kTileDisplay) {
          return base::unexpected(
              "Invalid backdrop_mask_element_id: layer is not a "
              "TileDisplayLayer");
        }
      } else {
        return base::unexpected(
            "Invalid backdrop_mask_element_id: layer not found");
      }
    }
  }

  if (update->local_surface_id_from_parent) {
    layers.SetLocalSurfaceIdFromParent(*update->local_surface_id_from_parent);
  }
  host_impl_->set_current_local_surface_id_from_client(
      update->current_local_surface_id);
  if (update->target_local_surface_id) {
    host_impl_->SetTargetLocalSurfaceId(*update->target_local_surface_id);
  }

  RETURN_IF_FALSE(update->next_frame_token > 0, "invalid frame token");
  host_impl_->set_next_frame_token_from_client(update->next_frame_token);

  host_impl_->set_send_frame_token_to_embedder(
      update->send_frame_token_to_embedder);

  {
    TRACE_EVENT1("viz", "DeserializeTilings", "TilingCount",
                 update->tilings.size());
    for (const auto& tiling : update->tilings) {
      if (cc::LayerImpl* layer = layers.LayerById(tiling->layer_id)) {
        if (layer->GetLayerType() != cc::mojom::LayerType::kTileDisplay) {
          return base::unexpected("Invalid tile update");
        }
        RETURN_IF_ERROR(DeserializeTiling(
            host_impl_.get(), static_cast<cc::TileDisplayLayerImpl&>(*layer),
            *tiling, /*update_damage=*/false));
      }
    }
  }

  // This needs to happen before SetPageSvaleFactorAndLimitsForDisplayTree().
  RETURN_IF_ERROR(UpdateViewportPropertyIds(layers, property_trees, *update));

  layers.set_background_color(update->background_color);
  layers.set_source_frame_number(update->source_frame_number);
  layers.set_trace_id(
      cc::BeginMainFrameTraceId::FromUnsafeValue(update->trace_id));
  layers.set_primary_main_frame_item_sequence_number(
      update->primary_main_frame_item_sequence_number);
  layers.SetDeviceViewportRect(update->device_viewport);

  layers.RegisterSelection(update->selection);

  if (update->page_scale_factor <= 0 ||
      !std::isfinite(update->page_scale_factor) ||
      update->min_page_scale_factor <= 0 ||
      !std::isfinite(update->min_page_scale_factor) ||
      update->max_page_scale_factor <= 0 ||
      !std::isfinite(update->max_page_scale_factor) ||
      update->min_page_scale_factor > update->max_page_scale_factor) {
    return base::unexpected("Invalid page scale factors");
  }
  layers.SetPageScaleFactorAndLimitsForDisplayTree(
      update->page_scale_factor, update->min_page_scale_factor,
      update->max_page_scale_factor);

  if (update->external_page_scale_factor <= 0 ||
      !std::isfinite(update->external_page_scale_factor)) {
    return base::unexpected("Invalid external page scale factor");
  }
  layers.SetExternalPageScaleFactor(update->external_page_scale_factor);

  if (update->device_scale_factor <= 0 ||
      !std::isfinite(update->device_scale_factor)) {
    return base::unexpected("Invalid device scale factor");
  }
  layers.SetDeviceScaleFactor(update->device_scale_factor);
  if (update->painted_device_scale_factor <= 0 ||
      !std::isfinite(update->painted_device_scale_factor)) {
    return base::unexpected("Invalid painted device scale factor");
  }
  if (update->max_safe_area_inset_bottom < 0 ||
      !std::isfinite(update->max_safe_area_inset_bottom)) {
    return base::unexpected("Invalid max safe area inset bottom");
  }
  if (update->browser_controls_params.top_controls_height < 0 ||
      !std::isfinite(update->browser_controls_params.top_controls_height) ||
      update->browser_controls_params.top_controls_min_height < 0 ||
      !std::isfinite(update->browser_controls_params.top_controls_min_height) ||
      update->browser_controls_params.bottom_controls_height < 0 ||
      !std::isfinite(update->browser_controls_params.bottom_controls_height) ||
      update->browser_controls_params.bottom_controls_min_height < 0 ||
      !std::isfinite(
          update->browser_controls_params.bottom_controls_min_height) ||
      update->browser_controls_params.top_controls_min_height >
          update->browser_controls_params.top_controls_height ||
      update->browser_controls_params.bottom_controls_min_height >
          update->browser_controls_params.bottom_controls_height) {
    return base::unexpected("Invalid browser controls params");
  }
  layers.SetBrowserControlsParams(update->browser_controls_params);
  host_impl_->browser_controls_manager()->SetOffsetTagModifications(
      update->browser_controls_offset_tag_modifications);

  layers.set_display_transform_hint(update->display_transform_hint);
  layers.SetMaxSafeAreaInsetBottom(update->max_safe_area_inset_bottom);
  layers.set_painted_device_scale_factor(update->painted_device_scale_factor);
  layers.SetDisplayColorSpaces(update->display_color_spaces);

  host_impl_->SetCurrentBrowserControlsShownRatio(
      update->top_controls_shown_ratio, update->bottom_controls_shown_ratio);

  host_impl_->SetViewportDamage(update->viewport_damage_rect);
  host_impl_->SetDebugState(update->debug_state);

  for (auto& ui_resource_request : update->ui_resource_requests) {
    if (ui_resource_request->type ==
        mojom::TransferableUIResourceRequest::Type::kCreate) {
      if (!ui_resource_request->transferable_resource ||
          ui_resource_request->transferable_resource->is_empty()) {
        return base::unexpected(
            "Invalid transferable resource in UI resource creation");
      }
      if (ui_resource_request->transferable_resource->GetSize().width() <= 0 ||
          ui_resource_request->transferable_resource->GetSize().height() <= 0) {
        return base::unexpected(
            "Invalid dimensions for transferable UI resource.");
      }
      ReleaseCallback release_callback = base::BindOnce(
          [](cc::LayerTreeHostImpl* host_impl, ResourceId id,
             const gpu::SyncToken& sync_token, bool is_lost) {
            host_impl->ReturnResource({id, sync_token,
                                       /*release_fence=*/gfx::GpuFenceHandle(),
                                       /*count=*/1, is_lost});
          },
          host_impl_.get(), ui_resource_request->transferable_resource->id);

      auto resource_id = host_impl_->resource_provider()->ImportResource(
          ui_resource_request->transferable_resource.value(),
          /*impl_release_callback=*/std::move(release_callback),
          /*main_thread_release_callback=*/base::NullCallback(),
          /*evicted_callback=*/base::NullCallback());

      host_impl_->CreateUIResourceFromImportedResource(
          ui_resource_request->uid, resource_id, ui_resource_request->opaque);
    } else {
      host_impl_->DeleteUIResource(ui_resource_request->uid);
    }
  }

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
    layers.set_needs_update_draw_properties();
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
  // Set this last, making sure renderer side state isn't overwritten by other
  // updates. As this is a transient property, we should set but not clear it.
  if (update->full_tree_damaged) {
    property_trees.set_full_tree_damaged(true);
  }

  // Safe down-cast: AnimationHost is the only subclass of MutatorHost.
  auto* animation_host = static_cast<cc::AnimationHost*>(layers.mutator_host());
  RETURN_IF_ERROR(DeserializeAnimationUpdates(*update, *animation_host));
  host_impl_->ActivateAnimations();

  // Only propagate property tree changes to layers if the property trees
  // actually changed. This avoids redundant work and prevents incorrectly
  // flagging draw properties as needing an update when no relevant properties
  // have changed.
  if (any_tree_changed || scroll_properties_changed) {
    layers.MoveChangeTrackingToLayers();
  }

  return base::ok();
}

void LayerContextImpl::DoDraw(const BeginFrameArgs& begin_frame_args,
                              base::TimeTicks start_update_display_tree,
                              bool frame_has_damage) {
  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    compositor_sink_->SetLayerContextWantsBeginFrames(true);
  } else {
    DoDrawInternal(begin_frame_args, start_update_display_tree,
                   frame_has_damage);
  }
}

void LayerContextImpl::DoDrawInternal(const BeginFrameArgs& begin_frame_args,
                                      base::TimeTicks start_update_display_tree,
                                      std::optional<bool> frame_has_damage) {
  TRACE_EVENT0("viz", "LayerContextImpl::DoDrawInternal");

  // If Renderer marks the frame as NOT damaged, then Viz should skip drawing.
  if (frame_has_damage && !frame_has_damage.value()) {
    return;
  }

  // Client/Renderer will never call UpdateDisplayTree if CanDraw() is false.
  // (crbug.com/454680865): Using DUMP_WILL_BE_CHECK allows all non official
  // builds to fail the check whereas official builds dumps without crashing.
  DUMP_WILL_BE_CHECK(host_impl_->CanDraw());

  host_impl_->WillBeginImplFrame(begin_frame_args);

  cc::FrameData frame;
  TreesInVizTiming stage_breakdown;
  stage_breakdown.start_update_display_tree = start_update_display_tree;
  // TODO(vmiura): Manage these flags properly.
  const bool has_damage = true;
  frame.begin_frame_ack = BeginFrameAck(begin_frame_args, has_damage);
  frame.origin_begin_main_frame_args = begin_frame_args;
  stage_breakdown.start_prepare_to_draw = base::TimeTicks::Now();

  auto expects_to_draw = frame_has_damage.value_or(false);
  auto draw_result = host_impl_->PrepareToDraw(&frame, expects_to_draw);

  // If a frame is expected to be drawn, then draw should succeed.
  if (expects_to_draw) {
    DUMP_WILL_BE_CHECK_EQ(draw_result, cc::DrawResult::kSuccess);
  }

  // Notifies the client which of the tilings it nominated for deletion are
  // actually safe to delete. This is done after PrepareToDraw() so that we have
  // the most up-to-date information on which tilings were used for the current
  // frame.
  SendTilingsCleanupNotificationToClient();

  stage_breakdown.start_draw_layers = base::TimeTicks::Now();
  frame.set_trees_in_viz_timestamps(std::move(stage_breakdown));

  // |DrawLayers| is expected to succeed. Adding a check to ensure that
  // is happening.
  std::optional<cc::SubmitInfo> submit_info = host_impl_->DrawLayers(&frame);
  DUMP_WILL_BE_CHECK(submit_info.has_value());

  host_impl_->DidDrawAllLayers(frame);
  host_impl_->DidFinishImplFrame(begin_frame_args);
}

void LayerContextImpl::SendTilingsCleanupNotificationToClient() {
  for (cc::LayerImpl* layer : *host_impl_->active_tree()) {
    if (layer->GetLayerType() == cc::mojom::LayerType::kTileDisplay) {
      auto* tile_layer = static_cast<cc::TileDisplayLayerImpl*>(layer);
      std::vector<float> scales_to_remove =
          tile_layer->GetSafeToDeleteTilings();
      if (!scales_to_remove.empty()) {
        client_->get()->OnTilingsReadyForCleanup(tile_layer->id(),
                                                 scales_to_remove);
      }
    }
  }
}

void LayerContextImpl::UpdateDisplayTiling(mojom::TilingPtr tiling,
                                           bool update_damage) {
  CHECK(receiver_);
  auto result = DoUpdateDisplayTiling(std::move(tiling), update_damage);
  if (!result.has_value()) {
    HandleBadMojoMessage("UpdateDisplayTiling", result.error());
  }
}

base::expected<void, std::string> LayerContextImpl::DoUpdateDisplayTiling(
    mojom::TilingPtr tiling,
    bool update_damage) {
  cc::LayerTreeImpl& layers = *host_impl_->active_tree();
  if (cc::LayerImpl* layer = layers.LayerById(tiling->layer_id)) {
    if (layer->GetLayerType() != cc::mojom::LayerType::kTileDisplay) {
      return base::unexpected("Invalid tile update");
    }

    return DeserializeTiling(host_impl_.get(),
                             static_cast<cc::TileDisplayLayerImpl&>(*layer),
                             *tiling, update_damage);
  }
  return base::ok();
}

}  // namespace viz
