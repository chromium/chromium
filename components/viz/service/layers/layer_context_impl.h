// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/tile_display_layer_impl.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc {
class RenderingStatsInstrumentation;
}  // namespace cc

namespace viz {

struct BeginFrameArgs;
class CompositorFrameSinkSupport;

// Implements the Viz LayerContext API backed by a LayerTreeHostImpl. This
// provides the service backend for a client-side VizLayerContext.
class LayerContextImpl : public cc::LayerTreeHostImplClient,
                         public cc::LayerTreeFrameSink,
                         public cc::TileDisplayLayerImpl::Client,
                         public mojom::LayerContext {
 public:
  // Constructs a new LayerContextImpl which submits frames to the local
  // `compositor_sink` with client connection details given by `context`.
  LayerContextImpl(CompositorFrameSinkSupport* compositor_sink,
                   mojom::PendingLayerContext& context);
  ~LayerContextImpl() override;

  void BeginFrame(const BeginFrameArgs& args);

  void ReturnResources(std::vector<ReturnedResource> resources);

 private:
  // cc::LayerTreeHostImplClient:
  void DidLoseLayerTreeFrameSinkOnImplThread() override;
  void SetBeginFrameSource(BeginFrameSource* source) override;
  void DidReceiveCompositorFrameAckOnImplThread() override;
  void OnCanDrawStateChanged(bool can_draw) override;
  void NotifyReadyToActivate() override;
  bool IsReadyToActivate() override;
  void NotifyReadyToDraw() override;
  void SetNeedsRedrawOnImplThread() override;
  void SetNeedsOneBeginImplFrameOnImplThread() override;
  void SetNeedsUpdateDisplayTreeOnImplThread() override;
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread() override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void SetDeferBeginMainFrameFromImpl(bool defer_begin_main_frame) override;
  bool IsInsideDraw() override;
  void RenewTreePriority() override;
  void PostDelayedAnimationTaskOnImplThread(base::OnceClosure task,
                                            base::TimeDelta delay) override;
  void DidActivateSyncTree() override;
  void DidPrepareTiles() override;
  void DidCompletePageScaleAnimationOnImplThread() override;
  void OnDrawForLayerTreeFrameSink(bool resourceless_software_draw,
                                   bool skip_draw) override;
  void SetNeedsImplSideInvalidation(
      bool needs_first_draw_on_activation) override;
  void NotifyImageDecodeRequestFinished(int request_id,
                                        bool decode_succeeded) override;
  void NotifyTransitionRequestFinished(uint32_t sequence_id) override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      cc::PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
      const FrameTimingDetails& details) override;
  void NotifyAnimationWorkletStateChange(
      cc::AnimationWorkletMutationState state,
      cc::ElementListType element_list_type) override;
  void NotifyPaintWorkletStateChange(
      cc::Scheduler::PaintWorkletState state) override;
  void NotifyThroughputTrackerResults(
      cc::CustomTrackerResults results) override;
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<FrameSinkId>& ids) override;
  void ClearHistory() override;
  void SetHasActiveThreadedScroll(bool is_scrolling) override;
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override;
  size_t CommitDurationSampleCountForTesting() const override;
  void DidObserveFirstScrollDelay(
      int source_frame_number,
      base::TimeDelta first_scroll_delay,
      base::TimeTicks first_scroll_timestamp) override;

  // cc::LayerTreeFrameSink:
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(CompositorFrame frame,
                             bool hit_test_data_changed) override;
  void DidNotProduceFrame(const BeginFrameAck& ack,
                          cc::FrameSkippedReason reason) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const SharedBitmapId& id) override;

  // cc::TileDisplayLayerImpl::Client:
  void DidAppendQuadsWithResources(
      const std::vector<TransferableResource>& resources) override;

  // mojom::LayerContext:
  void SetVisible(bool visible) override;
  void UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) override;
  void UpdateDisplayTiling(mojom::TilingPtr tiling) override;

  base::expected<void, std::string> DoUpdateDisplayTree(
      mojom::LayerTreeUpdatePtr update);

  const raw_ptr<CompositorFrameSinkSupport> compositor_sink_;
  const std::unique_ptr<cc::AnimationHost> animation_host_{
      cc::AnimationHost::CreateMainInstance()};

  mojo::AssociatedReceiver<mojom::LayerContext> receiver_;
  mojo::AssociatedRemote<mojom::LayerContextClient> client_;
  const std::unique_ptr<cc::TaskRunnerProvider> task_runner_provider_;
  const std::unique_ptr<cc::RenderingStatsInstrumentation> rendering_stats_;
  const std::unique_ptr<cc::LayerTreeHostImpl> host_impl_;

  std::vector<TransferableResource> next_frame_resources_;

  raw_ptr<cc::LayerTreeFrameSinkClient> frame_sink_client_ = nullptr;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
