// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/layer_tree_frame_sink_holder.h"

#include "base/containers/contains.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/typed_macros.h"
#include "cc/mojo_embedder/async_layer_tree_frame_sink.h"
#include "components/exo/surface_tree_host.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/returned_resource.h"

namespace exo {
namespace {

// If in ReactiveFrameSubmission and AutoNeedsBeginFrame mode, notifies the
// remote side to pause BeginFrame requests after the client hasn't produced
// frames for kPauseBeginFrameThreshold frames. Using a number so that the
// feature kicks in relatively quickly, but it is also not overly sensitive when
// the system occasionally drops frames.
constexpr int32_t kPauseBeginFrameThreshold = 5;

}  // namespace

BASE_FEATURE(kExoReactiveFrameSubmission,
             "ExoReactiveFrameSubmission",
             base::FEATURE_ENABLED_BY_DEFAULT);

////////////////////////////////////////////////////////////////////////////////
// LayerTreeFrameSinkHolder, public:

LayerTreeFrameSinkHolder::LayerTreeFrameSinkHolder(
    SurfaceTreeHost* surface_tree_host,
    std::unique_ptr<cc::mojo_embedder::AsyncLayerTreeFrameSink> frame_sink)
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
  DiscardCachedFrame(nullptr);

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

  if (holder->frame_sink_->last_submitted_size_in_pixels().IsEmpty()) {
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
  frame.metadata.device_scale_factor =
      holder->frame_sink_->last_submitted_device_scale_factor();
  auto pass = viz::CompositorRenderPass::Create();
  pass->SetNew(viz::CompositorRenderPassId{1},
               gfx::Rect(holder->frame_sink_->last_submitted_size_in_pixels()),
               gfx::Rect(holder->frame_sink_->last_submitted_size_in_pixels()),
               gfx::Transform());
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

void LayerTreeFrameSinkHolder::SubmitCompositorFrame(viz::CompositorFrame frame,
                                                     bool submit_now) {
  if (!reactive_frame_submission_) {
    SubmitCompositorFrameToRemote(&frame);
    return;
  }

  DiscardCachedFrame(&frame);

  // Needs to be after DiscardCachedFrame(), because discarding a frame will
  // reset the frame arrival information in `frame_timing_history_`.
  frame_timing_history_->FrameArrived();

  frame_timing_history_->MayRecordDidNotProduceToFrameArrvial(/*valid=*/true);

  ObserveBeginFrameSource(true);

  if (!ShouldSubmitFrameNow() && !submit_now) {
    cached_frame_ = std::move(frame);
    return;
  }

  ProcessFirstPendingBeginFrame(&frame);
  SubmitCompositorFrameToRemote(&frame);
  UpdateSubmitFrameTimer();
}

void LayerTreeFrameSinkHolder::SetLocalSurfaceId(
    const viz::LocalSurfaceId& local_surface_id) {
  frame_sink_->SetLocalSurfaceId(local_surface_id);
}

float LayerTreeFrameSinkHolder::LastDeviceScaleFactor() const {
  return cached_frame_ ? cached_frame_->device_scale_factor()
                       : frame_sink_->last_submitted_device_scale_factor();
}

const gfx::Size& LayerTreeFrameSinkHolder::LastSizeInPixels() const {
  return cached_frame_ ? cached_frame_->size_in_pixels()
                       : frame_sink_->last_submitted_size_in_pixels();
}

////////////////////////////////////////////////////////////////////////////////
// cc::LayerTreeFrameSinkClient overrides:

void LayerTreeFrameSinkHolder::SetBeginFrameSource(
    viz::BeginFrameSource* source) {
  if (!reactive_frame_submission_) {
    return;
  }

  ObserveBeginFrameSource(false);

  begin_frame_source_ = source;

  if (!frame_sink_->auto_needs_begin_frame()) {
    ObserveBeginFrameSource(true);
  } else {
    // Rely on SubmitCompositorFrame() to start observing begin frame source.

    // SetBeginFrameSource() with a non-null `source` is supposed to be called
    // during initialization. That must happen before any frame is submitted,
    // and therefore there must be no `cached_frame_` at this point.
    DCHECK(!cached_frame_ || source == nullptr);
  }
}

std::optional<viz::HitTestRegionList>
LayerTreeFrameSinkHolder::BuildHitTestData() {
  return {};
}

void LayerTreeFrameSinkHolder::ReclaimResources(
    std::vector<viz::ReturnedResource> resources) {
  for (auto& resource : resources) {
    // Skip resources that are also in last frame. This can happen if
    // the frame sink id changed.
    // TODO(crbug.com/40269434): if viz reclaims the resources b/c the
    // viz::Surface never gets embedded, this prevents clients from receiving
    // release callbacks. This needs to be addressed.
    if (base::Contains(last_frame_resources_, resource.id)) {
      continue;
    }
    in_use_resources_.erase(resource.id);

    // Skip resources that are also in the cached frame.
    if (cached_frame_ &&
        base::Contains(cached_frame_->resource_list, resource.id,
                       [](const viz::TransferableResource& resource) {
                         return resource.id;
                       })) {
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
  in_use_resources_.clear();
  resource_manager_.ClearAllCallbacks();
  is_lost_ = true;

  if (surface_tree_host_) {
    CHECK(!lifetime_manager_);
    surface_tree_host_->OnFrameSinkLost();
  }
  if (lifetime_manager_) {
    CHECK(!surface_tree_host_);
    ScheduleDelete();
  }
}

void LayerTreeFrameSinkHolder::ClearPendingBeginFramesForTesting() {
  while (!pending_begin_frames_.empty()) {
    OnSendDeadlineExpired(/*update_timer=*/false);
  };
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

  frame_timing_history_->BeginFrameArrived(args.frame_id);
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
    frame_timing_history_->FrameSubmitted(
        frame->metadata.begin_frame_ack.frame_id, frame->metadata.frame_token);
  }

  last_frame_resources_.clear();
  for (auto& resource : frame->resource_list) {
    last_frame_resources_.push_back(resource.id);
    in_use_resources_.insert(resource.id);
  }
  frame_sink_->SubmitCompositorFrame(std::move(*frame),
                                     /*hit_test_data_changed=*/true);

  // TODO(crbug.com/40278992): Push an object to
  // `pending_discarded_frame_notifications_` instead of using the counter here,
  // s.t. we don't have to wait until this counter drop to zero before
  // `SendDiscardedFrameNotifications()`, and frame_acks are properly ordered.
  pending_submit_frames_++;
}

void LayerTreeFrameSinkHolder::DiscardCachedFrame(
    const viz::CompositorFrame* new_frame) {
  if (!cached_frame_) {
    return;
  }

  DCHECK(reactive_frame_submission_);

  for (const auto& resource : cached_frame_->resource_list) {
    // Skip if the resource is still in use by the remote side.
    if (in_use_resources_.contains(resource.id)) {
      continue;
    }

    // Skip if the resource is also in `new_frame`.
    if (new_frame &&
        base::Contains(new_frame->resource_list, resource.id,
                       [](const viz::TransferableResource& resource) {
                         return resource.id;
                       })) {
      continue;
    }
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

  const int64_t client_frame_trace_id =
      cached_frame_->metadata.begin_frame_ack.trace_id;
  if (client_frame_trace_id != -1) {
    TRACE_EVENT_INSTANT(
        "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
        perfetto::Flow::Global(client_frame_trace_id),
        [client_frame_trace_id](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_chrome_graphics_pipeline();
          data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                             StepName::STEP_EXO_DISCARD_COMPOSITOR_FRAME);
          data->set_display_trace_id(client_frame_trace_id);
        });
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
  DiscardCachedFrame(nullptr);
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

    frame_timing_history_->FrameDidNotProduce(
        pending_begin_frame.begin_frame_ack.frame_id);

    pending_begin_frames_.pop();

    bool should_pause_begin_frame =
        frame_sink_->auto_needs_begin_frame() &&
        frame_timing_history_->consecutive_did_not_produce_count() >=
            kPauseBeginFrameThreshold;

    if (!pending_begin_frames_.empty() || should_pause_begin_frame) {
      frame_timing_history_->MayRecordDidNotProduceToFrameArrvial(
          /*valid=*/false);
    }

    if (should_pause_begin_frame) {
      ObserveBeginFrameSource(false);
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
                       base::Unretained(this), true),
        base::subtle::DelayPolicy::kPrecise);
  } else {
    submit_frame_timer_.Stop();
  }
}

void LayerTreeFrameSinkHolder::ProcessFirstPendingBeginFrame(
    viz::CompositorFrame* frame) {
  // The client-side frame trace ID, if available, is temporarily stored in
  // the frame's BeginFrameAck struct. Extract it before populating
  // BeginFrameAck.
  const int64_t client_frame_trace_id =
      frame->metadata.begin_frame_ack.trace_id;

  // If there are not-yet-handled BeginFrames requests from the remote side,
  // use `frame` as response to the earliest one.
  if (!pending_begin_frames_.empty()) {
    frame->metadata.begin_frame_ack =
        pending_begin_frames_.front().begin_frame_ack;
    pending_begin_frames_.pop();
  } else {
    // Submit an unsolicited frame.
    frame->metadata.begin_frame_ack =
        viz::BeginFrameAck::CreateManualAckWithDamage();
  }

  if (client_frame_trace_id != -1) {
    // Use both the ID from the client-side frame submission and the ID from the
    // BeginFrame request to connect the two flows.
    TRACE_EVENT_INSTANT(
        "viz,benchmark,graphics.pipeline", "Graphics.Pipeline",
        perfetto::Flow::Global(client_frame_trace_id),
        perfetto::Flow::Global(frame->metadata.begin_frame_ack.trace_id),
        [client_frame_trace_id](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_chrome_graphics_pipeline();
          data->set_step(perfetto::protos::pbzero::ChromeGraphicsPipeline::
                             StepName::STEP_EXO_SUBMIT_COMPOSITOR_FRAME);
          data->set_display_trace_id(client_frame_trace_id);
        });
  }
}

bool LayerTreeFrameSinkHolder::ShouldSubmitFrameNow() const {
  DCHECK(reactive_frame_submission_);

  return (!pending_begin_frames_.empty() || UnsolicitedFrameAllowed()) &&
         pending_submit_frames_ == 0;
}

void LayerTreeFrameSinkHolder::ObserveBeginFrameSource(bool start) {
  if (observing_begin_frame_source_ == start) {
    return;
  }

  if (begin_frame_source_) {
    observing_begin_frame_source_ = start;
    if (start) {
      begin_frame_source_->AddObserver(this);
    } else {
      begin_frame_source_->RemoveObserver(this);
    }
  } else {
    // If `begin_frame_source_` is nullptr, `observing_begin_frame_source_`
    // should already be false, and should stay that way even if `start` is
    // true.
    DCHECK(!observing_begin_frame_source_);
  }
}

bool LayerTreeFrameSinkHolder::UnsolicitedFrameAllowed() const {
  DCHECK(reactive_frame_submission_);

  // `frame_sink_->needs_begin_frames()` being false means the remote side is
  // currently not configured to send us BeginFrames. In this case, an
  // unsolicited frame should be allowed.
  return frame_sink_->auto_needs_begin_frame() &&
         !frame_sink_->needs_begin_frames();
}

}  // namespace exo
