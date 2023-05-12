// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/layer_tree_frame_sink_holder.h"

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "components/exo/surface_tree_host.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/returned_resource.h"

namespace exo {

BASE_FEATURE(kExoReactiveFrameSubmission,
             "ExoReactiveFrameSubmission",
             base::FEATURE_DISABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////
// LayerTreeFrameSinkHolder, public:

LayerTreeFrameSinkHolder::LayerTreeFrameSinkHolder(
    SurfaceTreeHost* surface_tree_host,
    std::unique_ptr<cc::LayerTreeFrameSink> frame_sink)
    : surface_tree_host_(surface_tree_host),
      frame_sink_(std::move(frame_sink)),
      reactive_frame_submission_(
          base::FeatureList::IsEnabled(kExoReactiveFrameSubmission)) {
  if (reactive_frame_submission_) {
    frame_timing_history_.emplace();
  }

  frame_sink_->BindToClient(this);
}

LayerTreeFrameSinkHolder::~LayerTreeFrameSinkHolder() {
  DiscardCachedFrame();

  if (frame_sink_)
    frame_sink_->DetachFromClient();

  if (lifetime_manager_)
    lifetime_manager_->RemoveObserver(this);
}

// static
void LayerTreeFrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
    std::unique_ptr<LayerTreeFrameSinkHolder> holder) {
  // Ensure that no cached frame is submitted in the future.
  holder->StopProcessingPendingFrames();

  // Delete immediately if LayerTreeFrameSink was already lost.
  if (holder->is_lost_)
    return;

  if (holder->last_frame_size_in_pixels_.IsEmpty()) {
    // Delete sink holder immediately if no frame has been submitted.
    DCHECK(holder->last_frame_resources_.empty());
    return;
  }

  // Submit an empty frame to ensure that pending release callbacks will be
  // processed in a finite amount of time. This frame is submitted directly,
  // disregarding BeginFrame request.
  viz::CompositorFrame frame;
  frame.metadata.begin_frame_ack =
      viz::BeginFrameAck::CreateManualAckWithDamage();
  frame.metadata.frame_token =
      holder->surface_tree_host_->GenerateNextFrameToken();
  frame.metadata.device_scale_factor = holder->last_frame_device_scale_factor_;
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(viz::CompositorRenderPassId{1},
               gfx::Rect(holder->last_frame_size_in_pixels_),
               gfx::Rect(holder->last_frame_size_in_pixels_), gfx::Transform());
  frame.render_pass_list.push_back(std::move(pass));
  holder->SubmitCompositorFrameToRemote(&frame);

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
  if (!reactive_frame_submission_) {
    SubmitCompositorFrameToRemote(&frame);
    return;
  }

  frame_timing_history_->MayRecordDidNotProduceToFrameArrvial(/*valid=*/true);

  DiscardCachedFrame();

  if (!ShouldSubmitFrameNow()) {
    cached_frame_ = std::move(frame);
    return;
  }

  ProcessFirstPendingBeginFrame(&frame);
  SubmitCompositorFrameToRemote(&frame);
  UpdateSubmitFrameTimer();
}

////////////////////////////////////////////////////////////////////////////////
// cc::LayerTreeFrameSinkClient overrides:

void LayerTreeFrameSinkHolder::SetBeginFrameSource(
    viz::BeginFrameSource* source) {
  if (!reactive_frame_submission_) {
    return;
  }

  if (begin_frame_source_) {
    begin_frame_source_->RemoveObserver(this);
  }

  begin_frame_source_ = source;

  if (begin_frame_source_) {
    begin_frame_source_->AddObserver(this);
  }
}

absl::optional<viz::HitTestRegionList>
LayerTreeFrameSinkHolder::BuildHitTestData() {
  return {};
}

void LayerTreeFrameSinkHolder::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  for (auto& resource : resources) {
    // Skip resources that are also in last frame. This can happen if
    // the frame sink id changed.
    if (base::Contains(last_frame_resources_, resource.id)) {
      continue;
    }
    resource_manager_.ReclaimResource(std::move(resource));
  }

  if (lifetime_manager_ && resource_manager_.HasNoCallbacks())
    ScheduleDelete();
}

void LayerTreeFrameSinkHolder::DidReceiveCompositorFrameAck() {
  pending_submit_frames_--;
  DCHECK_GE(pending_submit_frames_, 0);

  if (surface_tree_host_)
    surface_tree_host_->DidReceiveCompositorFrameAck();

  if (!reactive_frame_submission_) {
    return;
  }

  if (pending_submit_frames_ == 0) {
    while (!pending_discarded_frame_notifications_.empty()) {
      SendDiscardedFrameNotifications(
          pending_discarded_frame_notifications_.front());
      pending_discarded_frame_notifications_.pop();
    }
  }

  if (cached_frame_ && ShouldSubmitFrameNow()) {
    ProcessFirstPendingBeginFrame(&cached_frame_.value());
    SubmitCompositorFrameToRemote(&cached_frame_.value());
    cached_frame_.reset();
    UpdateSubmitFrameTimer();
  }
}

void LayerTreeFrameSinkHolder::DidPresentCompositorFrame(
    uint32_t frame_token,
    const viz::FrameTimingDetails& details) {
  if (frame_timing_history_) {
    frame_timing_history_->FrameReceivedAtRemoteSide(
        frame_token, details.received_compositor_frame_timestamp);
  }

  if (surface_tree_host_) {
    surface_tree_host_->DidPresentCompositorFrame(
        frame_token, details.presentation_feedback);
  }
}

void LayerTreeFrameSinkHolder::DidLoseLayerTreeFrameSink() {
  DCHECK(frame_sink_);
  frame_sink_->DetachFromClient();
  frame_sink_.reset();

  StopProcessingPendingFrames();

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
  base::SingleThreadTaskRunner::GetCurrentDefault()->DeleteSoon(FROM_HERE,
                                                                this);
}

void LayerTreeFrameSinkHolder::OnDestroyed() {
  lifetime_manager_->RemoveObserver(this);
  lifetime_manager_ = nullptr;

  if (frame_sink_) {
    // Make sure frame sink never outlives the shell.
    frame_sink_->DetachFromClient();
    frame_sink_.reset();
  }
  ScheduleDelete();
}

bool LayerTreeFrameSinkHolder::OnBeginFrameDerivedImpl(
    const viz::BeginFrameArgs& args) {
  DCHECK(reactive_frame_submission_);

  frame_timing_history_->MayRecordDidNotProduceToFrameArrvial(/*valid=*/false);

  pending_begin_frames_.emplace();
  pending_begin_frames_.back().begin_frame_ack =
      viz::BeginFrameAck(args, /*has_damage=*/true);
  pending_begin_frames_.back().send_deadline_estimate =
      args.deadline -
      viz::BeginFrameArgs::DefaultEstimatedDisplayDrawTime(args.interval) -
      frame_timing_history_->GetFrameTransferDurationEstimate();

  if (pending_begin_frames_.size() > 1) {
    return true;
  }

  if (cached_frame_ && ShouldSubmitFrameNow()) {
    ProcessFirstPendingBeginFrame(&cached_frame_.value());
    SubmitCompositorFrameToRemote(&cached_frame_.value());
    cached_frame_.reset();

    DCHECK(!submit_frame_timer_.IsRunning());
  } else {
    UpdateSubmitFrameTimer();
  }

  return true;
}

void LayerTreeFrameSinkHolder::OnBeginFrameSourcePausedChanged(bool paused) {}

void LayerTreeFrameSinkHolder::SubmitCompositorFrameToRemote(
    viz::CompositorFrame* frame) {
  DCHECK(!is_lost_);

  if (frame_timing_history_) {
    frame_timing_history_->FrameSubmitted(frame->metadata.frame_token,
                                          base::TimeTicks::Now());
  }

  last_frame_size_in_pixels_ = frame->size_in_pixels();
  last_frame_device_scale_factor_ = frame->metadata.device_scale_factor;
  last_frame_resources_.clear();
  for (auto& resource : frame->resource_list) {
    last_frame_resources_.push_back(resource.id);
  }
  frame_sink_->SubmitCompositorFrame(std::move(*frame),
                                     /*hit_test_data_changed=*/true);

  pending_submit_frames_++;
}

void LayerTreeFrameSinkHolder::DiscardCachedFrame() {
  if (!cached_frame_) {
    return;
  }

  DCHECK(reactive_frame_submission_);

  for (const auto& resource : cached_frame_->resource_list) {
    resource_manager_.ReclaimResource(resource.ToReturnedResource());
  }

  if (pending_submit_frames_ == 0) {
    SendDiscardedFrameNotifications(cached_frame_->metadata.frame_token);
  } else {
    // If a frame (frame_1) sent to the remote side hasn't received ack, we
    // should hold off sending back ack to `surface_tree_host_` for the
    // discarded frame (frame_2). The reason is that acks are not associated
    // with frame tokens. Sending back an ack here for frame_2 will be
    // indistinguishable from an ack for frame_1.
    pending_discarded_frame_notifications_.push(
        cached_frame_->metadata.frame_token);
  }

  cached_frame_.reset();

  frame_timing_history_->FrameDiscarded();
}

void LayerTreeFrameSinkHolder::SendDiscardedFrameNotifications(
    uint32_t frame_token) {
  if (!surface_tree_host_) {
    return;
  }

  surface_tree_host_->DidReceiveCompositorFrameAck();
  surface_tree_host_->DidPresentCompositorFrame(
      frame_token, gfx::PresentationFeedback::Failure());
}

void LayerTreeFrameSinkHolder::StopProcessingPendingFrames() {
  DiscardCachedFrame();
  pending_begin_frames_ = {};
  UpdateSubmitFrameTimer();
}

void LayerTreeFrameSinkHolder::OnSendDeadlineExpired(bool update_timer) {
  DCHECK(!is_lost_ && reactive_frame_submission_);

  if (pending_begin_frames_.empty()) {
    return;
  }

  if (cached_frame_) {
    ProcessFirstPendingBeginFrame(&cached_frame_.value());
    SubmitCompositorFrameToRemote(&cached_frame_.value());
    cached_frame_.reset();
  } else {
    auto& pending_begin_frame = pending_begin_frames_.front();
    pending_begin_frame.begin_frame_ack.has_damage = false;
    frame_sink_->DidNotProduceFrame(pending_begin_frame.begin_frame_ack,
                                    cc::FrameSkippedReason::kNoDamage);

    pending_begin_frames_.pop();

    frame_timing_history_->FrameDidNotProduce();
    if (!pending_begin_frames_.empty()) {
      frame_timing_history_->MayRecordDidNotProduceToFrameArrvial(
          /*valid=*/false);
    }
  }

  if (update_timer) {
    UpdateSubmitFrameTimer();
  }
}

void LayerTreeFrameSinkHolder::UpdateSubmitFrameTimer() {
  while (!pending_begin_frames_.empty() &&
         base::TimeTicks::Now() >=
             pending_begin_frames_.front().send_deadline_estimate) {
    OnSendDeadlineExpired(/*update_timer=*/false);
  };

  if (!pending_begin_frames_.empty()) {
    submit_frame_timer_.Start(
        FROM_HERE, pending_begin_frames_.front().send_deadline_estimate,
        base::BindOnce(&LayerTreeFrameSinkHolder::OnSendDeadlineExpired,
                       base::Unretained(this), true));
  } else {
    submit_frame_timer_.Stop();
  }
}

void LayerTreeFrameSinkHolder::ProcessFirstPendingBeginFrame(
    viz::CompositorFrame* frame) {
  DCHECK(!pending_begin_frames_.empty());

  frame->metadata.begin_frame_ack =
      pending_begin_frames_.front().begin_frame_ack;
  pending_begin_frames_.pop();
}

bool LayerTreeFrameSinkHolder::ShouldSubmitFrameNow() const {
  DCHECK(reactive_frame_submission_);

  return !pending_begin_frames_.empty() && pending_submit_frames_ == 0;
}

}  // namespace exo
