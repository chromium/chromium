// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_

#include "base/macros.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display_client.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/viz_service_export.h"
#include "services/viz/privileged/mojom/compositing/display_private.mojom.h"
#include "services/viz/public/mojom/compositing/compositor_frame_sink.mojom.h"

namespace base {
class HistogramBase;
}  // namespace base

namespace viz {
class CompositorFrameSinkSupportManager;
class Display;
class FrameSinkManagerImpl;

// This class submits compositor frames to an in-process Display, with the
// client's frame being the root surface of the Display.
class VIZ_SERVICE_EXPORT DirectLayerTreeFrameSink
    : public cc::LayerTreeFrameSink,
      public mojom::CompositorFrameSinkClient,
      public ExternalBeginFrameSourceClient,
      public DisplayClient {
 public:
  // This class is used to handle the graphics pipeline related metrics
  // reporting.
  class PipelineReporting {
   public:
    PipelineReporting(BeginFrameArgs args,
                      base::TimeTicks now,
                      base::HistogramBase* submit_begin_frame_histogram);
    ~PipelineReporting();

    void Report();

    int64_t trace_id() const { return trace_id_; }

   private:
    // The trace id of a BeginFrame which is used to track its progress on the
    // client side.
    int64_t trace_id_;

    // The time stamp for the begin frame to arrive on client side.
    base::TimeTicks frame_time_;

    // Histogram metrics used to record
    // GraphicsPipeline.ClientName.SubmitCompositorFrameAfterBeginFrame
    base::HistogramBase* submit_begin_frame_histogram_;
  };

  // The underlying Display, FrameSinkManagerImpl, and LocalSurfaceIdAllocator
  // must outlive this class.
  DirectLayerTreeFrameSink(
      const FrameSinkId& frame_sink_id,
      CompositorFrameSinkSupportManager* support_manager,
      FrameSinkManagerImpl* frame_sink_manager,
      Display* display,
      mojom::DisplayClient* display_client,
      scoped_refptr<ContextProvider> context_provider,
      scoped_refptr<RasterContextProvider> worker_context_provider,
      scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      bool hit_test_data_from_surface_layer);
  ~DirectLayerTreeFrameSink() override;

  // LayerTreeFrameSink implementation.
  bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(CompositorFrame frame,
                             bool hit_test_data_changed,
                             bool show_hit_test_borders) override;
  void DidNotProduceFrame(const BeginFrameAck& ack) override;
  void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                               const SharedBitmapId& id) override;
  void DidDeleteSharedBitmap(const SharedBitmapId& id) override;

  // DisplayClient implementation.
  void DisplayOutputSurfaceLost() override;
  void DisplayWillDrawAndSwap(bool will_draw_and_swap,
                              RenderPassList* render_passes) override;
  void DisplayDidDrawAndSwap() override;
  void DisplayDidReceiveCALayerParams(
      const gfx::CALayerParams& ca_layer_params) override;
  void DisplayDidCompleteSwapWithSize(const gfx::Size& pixel_size) override;
  void SetPreferredFrameInterval(base::TimeDelta interval) override;
  base::TimeDelta GetPreferredFrameIntervalForFrameSinkId(
      const FrameSinkId& id) override;

 private:
  // mojom::CompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFrame(const BeginFrameArgs& args,
                    const FrameTimingDetailsMap& timing_details) override;
  void ReclaimResources(
      const std::vector<ReturnedResource>& resources) override;
  void OnBeginFramePausedChanged(bool paused) override;

  // ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  // ContextLostObserver implementation:
  void OnContextLost() override;

  void DidReceiveCompositorFrameAckInternal(
      const std::vector<ReturnedResource>& resources);

  // This class is only meant to be used on a single thread.
  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CompositorFrameSinkSupport> support_;

  bool needs_begin_frames_ = false;
  const FrameSinkId frame_sink_id_;
  CompositorFrameSinkSupportManager* const support_manager_;
  FrameSinkManagerImpl* frame_sink_manager_;
  ParentLocalSurfaceIdAllocator parent_local_surface_id_allocator_;
  Display* display_;
  // |display_client_| may be nullptr on platforms that do not use it.
  mojom::DisplayClient* display_client_ = nullptr;
  gfx::Size last_swap_frame_size_;
  float device_scale_factor_ = 1.f;
  bool is_lost_ = false;
  std::unique_ptr<ExternalBeginFrameSource> begin_frame_source_;

  HitTestRegionList last_hit_test_data_;

  // Use this map to record the time when client received the BeginFrameArgs.
  base::flat_map<int64_t, PipelineReporting> pipeline_reporting_frame_times_;

  // Histogram metrics used to record
  // GraphicsPipeline.ClientName.ReceivedBeginFrame
  base::HistogramBase* const receive_begin_frame_histogram_;

  // Histogram metrics used to record
  // GraphicsPipeline.ClientName.SubmitCompositorFrameAfterBeginFrame
  base::HistogramBase* const submit_begin_frame_histogram_;

  const bool hit_test_data_from_surface_layer_;

  base::WeakPtrFactory<DirectLayerTreeFrameSink> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DirectLayerTreeFrameSink);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_DIRECT_LAYER_TREE_FRAME_SINK_H_
