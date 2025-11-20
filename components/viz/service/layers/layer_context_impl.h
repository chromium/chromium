// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
#define COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "cc/animation/animation_host.h"
#include "cc/layers/tile_display_layer_impl.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "cc/trees/layer_tree_host_impl_client.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

namespace cc {
class LayerTreeHostImpl;
class RenderingStatsInstrumentation;
class TaskRunnerProvider;
}  // namespace cc

namespace viz {

struct BeginFrameArgs;
class CompositorFrameSinkSupport;

// Implements the Viz LayerContext API backed by a LayerTreeHostImpl. This
// provides the service backend for a client-side VizLayerContext.
class VIZ_SERVICE_EXPORT LayerContextImpl : public cc::LayerTreeHostImplClient,
                                            public cc::LayerTreeFrameSink,
                                            public mojom::LayerContext {
 public:
  // Constructs a new LayerContextImpl which submits frames to the local
  // `compositor_sink` with client connection details given by `context`.
  LayerContextImpl(CompositorFrameSinkSupport* compositor_sink,
                   mojom::PendingLayerContext& context,
                   mojom::LayerContextSettingsPtr settings);

  // Static factory method for testing purposes. The created object's lifetime
  // is not managed by this function.
  static std::unique_ptr<LayerContextImpl> CreateForTesting(
      CompositorFrameSinkSupport* compositor_sink,
      mojom::LayerContextSettingsPtr settings);

  ~LayerContextImpl() override;

  void BeginFrame(const BeginFrameArgs& args);

  base::expected<void, std::string> DoUpdateDisplayTree(
      mojom::LayerTreeUpdatePtr update);
  base::expected<void, std::string> DoUpdateDisplayTiling(
      mojom::TilingPtr tiling,
      bool update_damage);
  void DoDraw(const BeginFrameArgs& begin_frame_args,
              base::TimeTicks start_update_display_tree,
              bool frame_has_damage);

  // Receive exported resources returned from the frame sink.
  void ReceiveReturnsFromParent(std::vector<ReturnedResource> resources);

  cc::LayerTreeHostImpl* host_impl() const { return host_impl_.get(); }

 private:
  // Private constructor that all other constructors/factory methods delegate
  // to.
  LayerContextImpl(
      CompositorFrameSinkSupport* compositor_sink,
      mojom::LayerContextSettingsPtr settings,
      mojo::PendingAssociatedReceiver<mojom::LayerContext> receiver_pipe,
      mojo::PendingAssociatedRemote<mojom::LayerContextClient> client_pipe);

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
  void SetNeedsPrepareTilesOnImplThread() override;
  void SetNeedsCommitOnImplThread(bool urgent) override;
  void SetVideoNeedsBeginFrames(bool needs_begin_frames) override;
  void DidChangeBeginFrameSourcePaused(bool paused) override;
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
                                        bool speculative,
                                        bool decode_succeeded) override;
  void NotifyTransitionRequestFinished(
      uint32_t sequence_id,
      const ViewTransitionElementResourceRects&) override;
  void DidPresentCompositorFrameOnImplThread(
      uint32_t frame_token,
      cc::PresentationTimeCallbackBuffer::PendingCallbacks callbacks,
      const FrameTimingDetails& details) override;
  void NotifyAnimationWorkletStateChange(
      cc::AnimationWorkletMutationState state,
      cc::ElementListType element_list_type) override;
  void NotifyPaintWorkletStateChange(
      cc::Scheduler::PaintWorkletState state) override;
  void NotifyCompositorMetricsTrackerResults(
      cc::CustomTrackerResults results) override;
  bool IsInSynchronousComposite() const override;
  void FrameSinksToThrottleUpdated(
      const base::flat_set<FrameSinkId>& ids) override;
  void ClearHistory() override;
  void SetHasActiveThreadedScroll(bool is_scrolling) override;
  void SetWaitingForScrollEvent(bool waiting_for_scroll_event) override;
  void ReturnResource(ReturnedResource returned_resource) override;
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
  void NotifyNewLocalSurfaceIdExpectedWhilePaused() override;

  // mojom::LayerContext:
  void SetVisible(bool visible) override;
  void UpdateDisplayTree(mojom::LayerTreeUpdatePtr update) override;
  void UpdateDisplayTiling(mojom::TilingPtr tiling,
                           bool update_damage) override;

  // Return any resources pending in |resources_to_return_| to the LayerContext
  // client, via the frame sink.
  void DoReturnResources();

  void HandleBadMojoMessage(const std::string& function,
                            const std::string& error);

  // Draws and submits a frame if there is damage. When |expects_to_draw|
  // is set, this will force draw even if there is no computed damage, or
  // other conditions would make us abort drawing.
  void DoDrawInternal(const BeginFrameArgs& begin_frame_args,
                      base::TimeTicks start_update_display_tree,
                      std::optional<bool> frame_has_damage = std::nullopt);

  void SendTilingsCleanupNotificationToClient();

  const raw_ptr<CompositorFrameSinkSupport> compositor_sink_;
  const std::unique_ptr<cc::AnimationHost> animation_host_{
      cc::AnimationHost::CreateMainInstance()};

  std::unique_ptr<mojo::AssociatedReceiver<mojom::LayerContext>> receiver_;
  std::unique_ptr<mojo::AssociatedRemote<mojom::LayerContextClient>> client_;
  const std::unique_ptr<cc::TaskRunnerProvider> task_runner_provider_;
  const std::unique_ptr<cc::RenderingStatsInstrumentation> rendering_stats_;

  std::vector<ReturnedResource> resources_to_return_;

  raw_ptr<cc::LayerTreeFrameSinkClient> frame_sink_client_ = nullptr;
  const std::unique_ptr<cc::LayerTreeHostImpl> host_impl_;

  // Must be the last member to ensure this is destroyed first in the
  // destruction order and invalidates all weak pointers.
  base::WeakPtrFactory<LayerContextImpl> weak_factory_{this};
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_LAYERS_LAYER_CONTEXT_IMPL_H_
