// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h"

#include <memory>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/histograms.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/hit_test/hit_test_data_builder.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/service/display/display.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support_manager.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"

namespace viz {
namespace {

base::HistogramBase* GetHistogramNamed(const char* histogram_name_format,
                                       const char* client_name) {
  if (!client_name)
    return nullptr;

  return base::LinearHistogram::FactoryMicrosecondsTimeGet(
      base::StringPrintf(histogram_name_format, client_name),
      base::TimeDelta::FromMicroseconds(1),
      base::TimeDelta::FromMilliseconds(200), 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
}
}  // namespace

DirectLayerTreeFrameSink::PipelineReporting::PipelineReporting(
    const BeginFrameArgs args,
    base::TimeTicks now,
    base::HistogramBase* submit_begin_frame_histogram)
    : trace_id_(args.trace_id),
      frame_time_(now),
      submit_begin_frame_histogram_(submit_begin_frame_histogram) {}

DirectLayerTreeFrameSink::PipelineReporting::~PipelineReporting() = default;

void DirectLayerTreeFrameSink::PipelineReporting::Report() {
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(trace_id_),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "SubmitCompositorFrame");
  auto report_time = base::TimeTicks::Now() - frame_time_;

  if (submit_begin_frame_histogram_)
    submit_begin_frame_histogram_->AddTimeMicrosecondsGranularity(report_time);
}

DirectLayerTreeFrameSink::DirectLayerTreeFrameSink(
    const FrameSinkId& frame_sink_id,
    CompositorFrameSinkSupportManager* support_manager,
    FrameSinkManagerImpl* frame_sink_manager,
    Display* display,
    mojom::DisplayClient* display_client,
    scoped_refptr<ContextProvider> context_provider,
    scoped_refptr<RasterContextProvider> worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    bool hit_test_data_from_surface_layer)
    : LayerTreeFrameSink(std::move(context_provider),
                         std::move(worker_context_provider),
                         std::move(compositor_task_runner),
                         gpu_memory_buffer_manager),
      frame_sink_id_(frame_sink_id),
      support_manager_(support_manager),
      frame_sink_manager_(frame_sink_manager),
      display_(display),
      display_client_(display_client),
      receive_begin_frame_histogram_(
          GetHistogramNamed("GraphicsPipeline.%s.ReceivedBeginFrame",
                            cc::GetClientNameForMetrics())),
      submit_begin_frame_histogram_(GetHistogramNamed(
          "GraphicsPipeline.%s.SubmitCompositorFrameAfterBeginFrame",
          cc::GetClientNameForMetrics())),
      hit_test_data_from_surface_layer_(hit_test_data_from_surface_layer) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

DirectLayerTreeFrameSink::~DirectLayerTreeFrameSink() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool DirectLayerTreeFrameSink::BindToClient(
    cc::LayerTreeFrameSinkClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!cc::LayerTreeFrameSink::BindToClient(client))
    return false;

  support_ = support_manager_->CreateCompositorFrameSinkSupport(
      this, frame_sink_id_, /*is_root=*/true,
      /*return_sync_tokens_required=*/false);
  begin_frame_source_ = std::make_unique<ExternalBeginFrameSource>(this);
  client_->SetBeginFrameSource(begin_frame_source_.get());

  // Avoid initializing GL context here, as this should be sharing the
  // Display's context.
  display_->Initialize(this, frame_sink_manager_->surface_manager());

  support_->SetUpHitTest(display_);

  return true;
}

void DirectLayerTreeFrameSink::DetachFromClient() {
  client_->SetBeginFrameSource(nullptr);
  begin_frame_source_.reset();

  // Unregister the SurfaceFactoryClient here instead of the dtor so that only
  // one client is alive for this namespace at any given time.
  support_.reset();

  cc::LayerTreeFrameSink::DetachFromClient();
}

void DirectLayerTreeFrameSink::SubmitCompositorFrame(
    CompositorFrame frame,
    bool hit_test_data_changed,
    bool show_hit_test_borders) {
  DCHECK(frame.metadata.begin_frame_ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber,
            frame.metadata.begin_frame_ack.sequence_number);

  // It's possible to request an immediate composite from cc which will bypass
  // BeginFrame. In that case, we cannot collect full graphics pipeline data.
  auto it = pipeline_reporting_frame_times_.find(
      frame.metadata.begin_frame_ack.trace_id);
  if (it != pipeline_reporting_frame_times_.end()) {
    it->second.Report();
    pipeline_reporting_frame_times_.erase(it);
  }

  if (frame.size_in_pixels() != last_swap_frame_size_ ||
      frame.device_scale_factor() != device_scale_factor_ ||
      !parent_local_surface_id_allocator_.HasValidLocalSurfaceIdAllocation()) {
    parent_local_surface_id_allocator_.GenerateId();
    last_swap_frame_size_ = frame.size_in_pixels();
    device_scale_factor_ = frame.device_scale_factor();
    display_->SetLocalSurfaceId(
        parent_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()
            .local_surface_id(),
        device_scale_factor_);
  }

  const int64_t trace_id = ~frame.metadata.begin_frame_ack.trace_id;
  TRACE_EVENT_WITH_FLOW1(TRACE_DISABLED_BY_DEFAULT("viz.hit_testing_flow"),
                         "Event.Pipeline", TRACE_ID_GLOBAL(trace_id),
                         TRACE_EVENT_FLAG_FLOW_OUT, "step",
                         "SubmitHitTestData");

  base::Optional<HitTestRegionList> hit_test_region_list;
  if (!hit_test_data_from_surface_layer_) {
    hit_test_region_list = HitTestDataBuilder::CreateHitTestData(
        frame, /*root_accepts_events=*/true,
        /*should_ask_for_child_region=*/false);
  } else {
    hit_test_region_list = client_->BuildHitTestData();
  }

  if (!hit_test_region_list) {
    last_hit_test_data_ = HitTestRegionList();
  } else if (!hit_test_data_changed) {
    // Do not send duplicate hit-test data.
    if (HitTestRegionList::IsEqual(*hit_test_region_list,
                                   last_hit_test_data_)) {
      DCHECK(!HitTestRegionList::IsEqual(*hit_test_region_list,
                                         HitTestRegionList()));
      hit_test_region_list = base::nullopt;
    } else {
      last_hit_test_data_ = *hit_test_region_list;
    }
  } else {
    last_hit_test_data_ = *hit_test_region_list;
  }

  support_->SubmitCompositorFrame(
      parent_local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation()
          .local_surface_id(),
      std::move(frame), std::move(hit_test_region_list));
}

void DirectLayerTreeFrameSink::DidNotProduceFrame(const BeginFrameAck& ack) {
  DCHECK(!ack.has_damage);
  DCHECK_LE(BeginFrameArgs::kStartingFrameNumber, ack.sequence_number);

  // TODO(yiyix): Remove duplicated calls of DidNotProduceFrame from the same
  // BeginFrames. https://crbug.com/881949
  auto it = pipeline_reporting_frame_times_.find(ack.trace_id);
  if (it != pipeline_reporting_frame_times_.end()) {
    support_->DidNotProduceFrame(ack);
    pipeline_reporting_frame_times_.erase(it);
  }
}

void DirectLayerTreeFrameSink::DidAllocateSharedBitmap(
    base::ReadOnlySharedMemoryRegion region,
    const SharedBitmapId& id) {
  bool ok = support_->DidAllocateSharedBitmap(std::move(region), id);
  DCHECK(ok);
}

void DirectLayerTreeFrameSink::DidDeleteSharedBitmap(const SharedBitmapId& id) {
  support_->DidDeleteSharedBitmap(id);
}

void DirectLayerTreeFrameSink::DisplayOutputSurfaceLost() {
  is_lost_ = true;
  client_->DidLoseLayerTreeFrameSink();
}

void DirectLayerTreeFrameSink::DisplayWillDrawAndSwap(
    bool will_draw_and_swap,
    RenderPassList* render_passes) {
  if (support_->GetHitTestAggregator()) {
    support_->GetHitTestAggregator()->Aggregate(display_->CurrentSurfaceId(),
                                                render_passes);
  }
}

void DirectLayerTreeFrameSink::DisplayDidDrawAndSwap() {
  // This notification is not relevant to our client outside of tests. We
  // unblock the client from DidDrawCallback() when the surface is going to
  // be drawn.
}

void DirectLayerTreeFrameSink::DisplayDidReceiveCALayerParams(
    const gfx::CALayerParams& ca_layer_params) {
#if defined(OS_MACOSX)
  // If |ca_layer_params| should have content only when there exists a client
  // to send it to.
  DCHECK(ca_layer_params.is_empty || display_client_);
  if (display_client_)
    display_client_->OnDisplayReceivedCALayerParams(ca_layer_params);
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(display_client_);
#endif
}

void DirectLayerTreeFrameSink::DisplayDidCompleteSwapWithSize(
    const gfx::Size& pixel_size) {
  // Not needed in non-OOP-D mode.
}

void DirectLayerTreeFrameSink::SetPreferredFrameInterval(
    base::TimeDelta interval) {
  // Not supported in non-OOP-D mode.
  NOTREACHED() << "Can not specify preferred interval, "
                  "no supported intervals were provided";
}

base::TimeDelta
DirectLayerTreeFrameSink::GetPreferredFrameIntervalForFrameSinkId(
    const FrameSinkId& id) {
  return frame_sink_manager_->GetPreferredFrameIntervalForFrameSinkId(id);
}

void DirectLayerTreeFrameSink::DidReceiveCompositorFrameAck(
    const std::vector<ReturnedResource>& resources) {
  // Submitting a CompositorFrame can synchronously draw and dispatch a frame
  // ack. PostTask to ensure the client is notified on a new stack frame.
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DirectLayerTreeFrameSink::DidReceiveCompositorFrameAckInternal,
          weak_factory_.GetWeakPtr(), resources));
}

void DirectLayerTreeFrameSink::DidReceiveCompositorFrameAckInternal(
    const std::vector<ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
  client_->DidReceiveCompositorFrameAck();
}

void DirectLayerTreeFrameSink::OnBeginFrame(
    const BeginFrameArgs& args,
    const FrameTimingDetailsMap& timing_details) {
  for (const auto& pair : timing_details)
    client_->DidPresentCompositorFrame(pair.first, pair.second);

  DCHECK_LE(pipeline_reporting_frame_times_.size(), 25u);
  if (args.trace_id != -1) {
    base::TimeTicks current_time = base::TimeTicks::Now();
    PipelineReporting report(args, current_time, submit_begin_frame_histogram_);
    pipeline_reporting_frame_times_.emplace(args.trace_id, report);
    // Missed BeginFrames use the frame time of the last received BeginFrame
    // which is bogus from a reporting perspective if nothing has been updating
    // on screen for a while.
    if (args.type != BeginFrameArgs::MISSED) {
      base::TimeDelta frame_difference = current_time - args.frame_time;

      if (receive_begin_frame_histogram_) {
        receive_begin_frame_histogram_->AddTimeMicrosecondsGranularity(
            frame_difference);
      }
    }
  }
  if (!needs_begin_frames_) {
    TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                           TRACE_ID_GLOBAL(args.trace_id),
                           TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                           "step", "ReceiveBeginFrameDiscard");
    // OnBeginFrame() can be called just to deliver presentation feedback, so
    // report that we didn't use this BeginFrame.
    DidNotProduceFrame(BeginFrameAck(args, false));
    return;
  }
  TRACE_EVENT_WITH_FLOW1("viz,benchmark", "Graphics.Pipeline",
                         TRACE_ID_GLOBAL(args.trace_id),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "step", "ReceiveBeginFrame");

  begin_frame_source_->OnBeginFrame(args);
}

void DirectLayerTreeFrameSink::ReclaimResources(
    const std::vector<ReturnedResource>& resources) {
  client_->ReclaimResources(resources);
}

void DirectLayerTreeFrameSink::OnBeginFramePausedChanged(bool paused) {
  begin_frame_source_->OnSetBeginFrameSourcePaused(paused);
}

void DirectLayerTreeFrameSink::OnNeedsBeginFrames(bool needs_begin_frames) {
  needs_begin_frames_ = needs_begin_frames;
  support_->SetNeedsBeginFrame(needs_begin_frames);
}

void DirectLayerTreeFrameSink::OnContextLost() {
  // The display will be listening for OnContextLost(). Do nothing here.
}

}  // namespace viz
