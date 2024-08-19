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
  node.local = wire.local;
  node.origin = wire.origin;
  node.scroll_offset = wire.scroll_offset;
  node.visible_frame_element_id = wire.visible_frame_element_id;
  node.transform_changed = true;
  return base::ok();
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::ClipNode& node,
    const mojom::ClipNode& wire) {
  if (!IsPropertyTreeIndexValid(trees.transform_tree(), wire.transform_id)) {
    return base::unexpected("Invalid transform_id for clip node");
  }
  node.transform_id = wire.transform_id;
  node.clip = wire.clip;
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
  return base::ok();
}

base::expected<void, std::string> UpdatePropertyTreeNode(
    cc::PropertyTrees& trees,
    cc::ScrollNode& node,
    const mojom::ScrollNode& wire) {
  if (!IsPropertyTreeIndexValid(trees.transform_tree(), wire.transform_id)) {
    return base::unexpected("Invalid transform_id for scroll node");
  }
  node.transform_id = wire.transform_id;
  node.container_bounds = wire.container_bounds;
  node.bounds = wire.bounds;
  node.element_id = wire.element_id;
  if (node.element_id) {
    trees.scroll_tree_mutable().SetElementIdForNodeId(node.id, node.element_id);
  }
  node.scrolls_inner_viewport = wire.scrolls_inner_viewport;
  node.scrolls_outer_viewport = wire.scrolls_outer_viewport;
  node.user_scrollable_horizontal = wire.user_scrollable_horizontal;
  node.user_scrollable_vertical = wire.user_scrollable_vertical;
  return base::ok();
}

template <typename TreeType, typename WireContainerType>
base::expected<bool, std::string> UpdatePropertyTree(
    cc::PropertyTrees& trees,
    TreeType& tree,
    const WireContainerType& wire_updates,
    uint32_t num_nodes) {
  const bool changed_anything =
      !wire_updates.empty() || num_nodes < tree.nodes().size();
  if (num_nodes < tree.nodes().size()) {
    tree.RemoveNodes(tree.nodes().size() - num_nodes);
  } else {
    using NodeType = typename TreeType::NodeType;
    for (size_t i = tree.nodes().size(); i < num_nodes; ++i) {
      tree.Insert(NodeType(), cc::kRootPropertyNodeId);
    }
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
  return changed_anything;
}

base::expected<void, std::string> AddOrUpdateLayer(
    LayerContextImpl& context,
    cc::LayerTreeImpl& tree,
    mojom::Layer& wire,
    cc::LayerImpl* existing_layer) {
  cc::LayerImpl* layer;
  if (existing_layer) {
    // TODO(rockot): Also validate existing layer type here. We don't yet fully
    // honor the type given by the client, so validation doesn't make sense yet.
    if (existing_layer->id() != wire.id) {
      return base::unexpected("Layer ID mismatch");
    }
    layer = existing_layer;
  } else {
    auto new_layer = CreateLayer(context, tree, wire.type, wire.id);
    layer = new_layer.get();
    tree.AddLayer(std::move(new_layer));
  }

  DCHECK(layer);
  layer->SetBounds(wire.bounds);
  layer->SetContentsOpaque(wire.contents_opaque);
  layer->SetContentsOpaqueForText(wire.contents_opaque_for_text);
  layer->SetDrawsContent(wire.is_drawable);
  layer->SetBackgroundColor(wire.background_color);
  layer->SetSafeOpaqueBackgroundColor(wire.safe_opaque_background_color);
  layer->SetElementId(wire.element_id);
  layer->UnionUpdateRect(wire.update_rect);
  layer->SetOffsetToTransformParent(wire.offset_to_transform_parent);

  const cc::PropertyTrees& property_trees = *tree.property_trees();
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

  layer->SetTransformTreeIndex(wire.transform_tree_index);
  layer->SetClipTreeIndex(wire.clip_tree_index);
  layer->SetEffectTreeIndex(wire.effect_tree_index);
  layer->SetScrollTreeIndex(wire.scroll_tree_index);
  layer->UpdateScrollable();
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
  NOTIMPLEMENTED();
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

  // We update property trees first, as they may change dimensions here and we
  // need to validate tree node references when updating layers below. The order
  // of tree update also matters here because clip, effect, and scroll trees all
  // validate some fields against the updated transform tree, and effect trees
  // also validate fields against the updated clip tree.
  cc::PropertyTrees& property_trees = *layers.property_trees();
  ASSIGN_OR_RETURN(const bool transform_changed,
                   UpdatePropertyTree(
                       property_trees, property_trees.transform_tree_mutable(),
                       update->transform_nodes, update->num_transform_nodes));
  ASSIGN_OR_RETURN(
      const bool clip_changed,
      UpdatePropertyTree(property_trees, property_trees.clip_tree_mutable(),
                         update->clip_nodes, update->num_clip_nodes));
  ASSIGN_OR_RETURN(
      const bool effect_changed,
      UpdatePropertyTree(property_trees, property_trees.effect_tree_mutable(),
                         update->effect_nodes, update->num_effect_nodes));
  ASSIGN_OR_RETURN(
      const bool scroll_changed,
      UpdatePropertyTree(property_trees, property_trees.scroll_tree_mutable(),
                         update->scroll_nodes, update->num_scroll_nodes));

  if (layers.RemoveLayers(update->removed_layers) !=
      update->removed_layers.size()) {
    return base::unexpected("Invalid layer removal");
  }

  if (update->root_layer) {
    RETURN_IF_ERROR(AddOrUpdateLayer(*this, layers, *update->root_layer,
                                     layers.root_layer()));
  } else if (!layers.root_layer() && !update->layers.empty()) {
    return base::unexpected(
        "Initial non-empty tree update missing root layer.");
  }

  for (auto& wire : update->layers) {
    RETURN_IF_ERROR(
        AddOrUpdateLayer(*this, layers, *wire, layers.LayerById(wire->id)));
  }

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
  layers.set_trace_id(update->trace_id);
  layers.SetDeviceViewportRect(update->device_viewport);
  if (update->device_scale_factor <= 0) {
    return base::unexpected("Invalid device scale factor");
  }
  layers.SetDeviceScaleFactor(update->device_scale_factor);
  if (update->local_surface_id_from_parent) {
    layers.SetLocalSurfaceIdFromParent(*update->local_surface_id_from_parent);
  }

  RETURN_IF_ERROR(UpdateViewportPropertyIds(layers, property_trees, *update));

  property_trees.UpdateChangeTracking();
  property_trees.transform_tree_mutable().set_needs_update(
      transform_changed || property_trees.transform_tree().needs_update());
  property_trees.clip_tree_mutable().set_needs_update(
      clip_changed || property_trees.clip_tree().needs_update());
  property_trees.effect_tree_mutable().set_needs_update(
      effect_changed || property_trees.effect_tree().needs_update());
  property_trees.set_changed(transform_changed || clip_changed ||
                             effect_changed || scroll_changed);

  std::vector<std::unique_ptr<cc::RenderSurfaceImpl>> old_render_surfaces;
  property_trees.effect_tree_mutable().TakeRenderSurfaces(&old_render_surfaces);
  const bool render_surfaces_changed =
      property_trees.effect_tree_mutable().CreateOrReuseRenderSurfaces(
          &old_render_surfaces, &layers);
  if (render_surfaces_changed) {
    layers.set_needs_update_draw_properties();
  }

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
