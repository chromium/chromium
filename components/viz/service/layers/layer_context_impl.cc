// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/layers/layer_context_impl.h"

#include <string>
#include <utility>

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/types/expected_macros.h"
#include "cc/debug/rendering_stats_instrumentation.h"
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

cc::LayerListSettings GetDisplayTreeSettings() {
  cc::LayerListSettings settings;
  settings.is_display_tree = true;
  return settings;
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

void LayerContextImpl::DidLoseLayerTreeFrameSinkOnImplThread() {
  NOTREACHED_NORETURN();
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
  NOTREACHED_NORETURN();
}

void LayerContextImpl::SetNeedsPrepareTilesOnImplThread() {
  NOTREACHED_NORETURN();
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

void LayerContextImpl::WillPrepareTiles() {}

void LayerContextImpl::DidPrepareTiles() {}

void LayerContextImpl::DidCompletePageScaleAnimationOnImplThread() {}

void LayerContextImpl::OnDrawForLayerTreeFrameSink(
    bool resourceless_software_draw,
    bool skip_draw) {}

void LayerContextImpl::NeedsImplSideInvalidation(
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
  // need to validate tree node references when updating layers below.
  // TODO(rockot): Update clip, effect, and scroll trees as well.
  cc::PropertyTrees& property_trees = *layers.property_trees();
  ASSIGN_OR_RETURN(const bool transform_changed,
                   UpdatePropertyTree(
                       property_trees, property_trees.transform_tree_mutable(),
                       update->transform_nodes, update->num_transform_nodes));

  layers.set_background_color(update->background_color);
  layers.set_source_frame_number(update->source_frame_number);
  layers.set_trace_id(update->trace_id);
  layers.SetDeviceViewportRect(update->device_viewport);
  layers.SetDeviceScaleFactor(update->device_scale_factor);
  if (update->local_surface_id_from_parent) {
    layers.SetLocalSurfaceIdFromParent(*update->local_surface_id_from_parent);
  }

  property_trees.UpdateChangeTracking();
  property_trees.transform_tree_mutable().set_needs_update(
      transform_changed || property_trees.transform_tree().needs_update());
  property_trees.set_changed(transform_changed);

  compositor_sink_->SetLayerContextWantsBeginFrames(true);
  return base::ok();
}

}  // namespace viz
