// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/layer_tree_frame_sink_holder.h"

#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/surface_tree_host.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/returned_resource.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// LayerTreeFrameSinkHolder, public:

LayerTreeFrameSinkHolder::LayerTreeFrameSinkHolder(
    SurfaceTreeHost* surface_tree_host,
    std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
    : surface_tree_host_(surface_tree_host),
      frame_sink_(std::move(frame_sink)) {
  frame_sink_->BindToClient(this);
}

LayerTreeFrameSinkHolder::~LayerTreeFrameSinkHolder() {
  if (frame_sink_)
    frame_sink_->DetachFromClient();

  if (lifetime_manager_)
    lifetime_manager_->RemoveObserver(this);
}

// static
void LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
    std::unique_ptr<LayerTreeFrameSinkHolder> holder) {
  // Delete immediately if LayerTreeFrameSink was already lost.
  if (holder->is_lost_)
    return;

  if (holder->last_frame_size_in_pixels_.IsEmpty()) {
    // Delete sink holder immediately if no frame has been submitted.
    DCHECK(holder->last_frame_resources_.empty());
    return;
  }

  // Submit an empty frame to ensure that pending release callbacks will be
  // processed in a finite amount of time.
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack.source_id =
      viz::BeginFrameArgs::kManualSourceId;
  frame.metadata.begin_frame_ack.sequence_number =
      viz::BeginFrameArgs::kStartingFrameNumber;
  frame.metadata.begin_frame_ack.has_damage = true;
  frame.metadata.frame_token = ++holder->next_frame_token_;
  frame.metadata.device_scale_factor = holder->last_frame_device_scale_factor_;
  frame.metadata.local_surface_id_allocation_time =
      holder->last_local_surface_id_allocation_time_;
  std::unique_ptr<viz::RenderPass> pass = viz::RenderPass::Create();
  pass->SetNew(1, gfx::Rect(holder->last_frame_size_in_pixels_),
               gfx::Rect(holder->last_frame_size_in_pixels_), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  holder->last_frame_resources_.clear();
  holder->frame_sink_->SubmitCompositorFrame(std::move(frame),
                                             /*hit_test_data_changed=*/true,
                                             /*show_hit_test_borders=*/false);

  // Delete sink holder immediately if not waiting for resources to be
  // reclaimed.
  if (holder->resource_manager_.HasNoCallbacks())
    return;

  WMHelper::LifetimeManager* lifetime_manager =
      WMHelper::GetInstance()->GetLifetimeManager();
  holder->lifetime_manager_ = lifetime_manager;
  holder->surface_tree_host_ = nullptr;

  // If we have pending release callbacks then extend the lifetime of holder
  // by adding it as a LifetimeManager observer. The holder will delete itself
  // when LifetimeManager shuts down or when all pending release callbacks have
  // been called.
  lifetime_manager->AddObserver(holder.release());
}

void LayerTreeFrameSinkHolder::SubmitCompositorFrame(
    viz::CompositorFrame frame) {
  DCHECK(!is_lost_);

  last_frame_size_in_pixels_ = frame.size_in_pixels();
  last_frame_device_scale_factor_ = frame.metadata.device_scale_factor;
  last_local_surface_id_allocation_time_ =
      frame.metadata.local_surface_id_allocation_time;
  last_frame_resources_.clear();
  for (auto& resource : frame.resource_list)
    last_frame_resources_.push_back(resource.id);
  frame_sink_->SubmitCompositorFrame(std::move(frame),
                                     /*hit_test_data_changed=*/true,
                                     /*show_hit_test_borders=*/false);
}

void LayerTreeFrameSinkHolder::DidNotProduceFrame(
    const viz::BeginFrameAck& ack) {
  DCHECK(!is_lost_);
  frame_sink_->DidNotProduceFrame(ack);
}

////////////////////////////////////////////////////////////////////////////////
// cc::LayerTreeFrameSinkClient overrides:

base::Optional<viz::HitTestRegionList>
LayerTreeFrameSinkHolder::BuildHitTestData() {
  return {};
}

void LayerTreeFrameSinkHolder::ReclaimResources(
    const std::vector<viz::ReturnedResource>& resources) {
  for (auto& resource : resources) {
    // Skip resources that are also in last frame. This can happen if
    // the frame sink id changed.
    if (base::Contains(last_frame_resources_, resource.id)) {
      continue;
    }
    resource_manager_.ReclaimResource(resource);
  }

  if (lifetime_manager_ && resource_manager_.HasNoCallbacks())
    ScheduleDelete();
}

void LayerTreeFrameSinkHolder::DidReceiveCompositorFrameAck() {
  if (surface_tree_host_)
    surface_tree_host_->DidReceiveCompositorFrameAck();
}

void LayerTreeFrameSinkHolder::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  if (surface_tree_host_) {
    surface_tree_host_->DidPresentCompositorFrame(
        frame_token, details.presentation_feedback);
  }
}

void LayerTreeFrameSinkHolder::DidLoseLayerTreeFrameSink() {
  last_frame_resources_.clear();
  resource_manager_.ClearAllCallbacks();
  is_lost_ = true;

  if (lifetime_manager_)
    ScheduleDelete();
}

////////////////////////////////////////////////////////////////////////////////
// LayerTreeFrameSinkHolder, private:

void LayerTreeFrameSinkHolder::ScheduleDelete() {
  if (delete_pending_)
    return;
  delete_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE, this);
}

void LayerTreeFrameSinkHolder::OnDestroyed() {
  lifetime_manager_->RemoveObserver(this);
  lifetime_manager_ = nullptr;

  // Make sure frame sink never outlives the shell.
  frame_sink_->DetachFromClient();
  frame_sink_.reset();
  ScheduleDelete();
}

}  // namespace exo
