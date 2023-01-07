// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo.h"

#include <utility>

#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace viz {

ExternalBeginFrameSourceMojo::ExternalBeginFrameSourceMojo(
    FrameSinkManagerImpl* frame_sink_manager,
    mojo::PendingAssociatedReceiver<mojom::ExternalBeginFrameController>
        controller_receiver,
    uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      frame_sink_manager_(frame_sink_manager),
      receiver_(this, std::move(controller_receiver)) {
  frame_sink_manager_->AddObserver(this);
}

ExternalBeginFrameSourceMojo::~ExternalBeginFrameSourceMojo() {
  frame_sink_manager_->RemoveObserver(this);
  DCHECK(!display_);
}

void ExternalBeginFrameSourceMojo::IssueExternalBeginFrame(
    const BeginFrameArgs& args,
    bool force,
    base::OnceCallback<void(const BeginFrameAck&)> callback) {
  DCHECK(!pending_frame_callback_) << "Got overlapping IssueExternalBeginFrame";
  DCHECK(pending_frame_sinks_.empty());
  original_source_id_ = args.frame_id.source_id;

  OnBeginFrame(args);

  pending_frame_callback_ = std::move(callback);

  // When not forcing a frame, wait for it to occur when sinks needs a frame.
  if (!force)
    return;
  // Ensure that Display will receive the BeginFrame (as a missed one), even
  // if it doesn't currently need it. This way, we ensure that
  // OnDisplayDidFinishFrame will be called for this BeginFrame.
  DCHECK(display_);
  display_->SetNeedsOneBeginFrame();
  MaybeProduceFrameCallback();
}

void ExternalBeginFrameSourceMojo::OnDestroyedCompositorFrameSink(
    const FrameSinkId& sink_id) {
  pending_frame_sinks_.erase(sink_id);
  MaybeProduceFrameCallback();
}

void ExternalBeginFrameSourceMojo::OnFrameSinkDidBeginFrame(
    const FrameSinkId& sink_id,
    const BeginFrameArgs& args) {
  if (args.frame_id.source_id != original_source_id_)
    return;
  pending_frame_sinks_.insert(sink_id);
}

void ExternalBeginFrameSourceMojo::OnFrameSinkDidFinishFrame(
    const FrameSinkId& sink_id,
    const BeginFrameArgs& args) {
  if (args.frame_id.source_id != original_source_id_)
    return;
  pending_frame_sinks_.erase(sink_id);
  MaybeProduceFrameCallback();
}

void ExternalBeginFrameSourceMojo::MaybeProduceFrameCallback() {
  if (!pending_frame_sinks_.empty())
    return;
  if (!pending_frame_callback_)
    return;

  if (pending_ack_) {
    DispatchFrameCallback(*pending_ack_);
    pending_ack_.reset();
    return;
  }
  // If there aren't pending surfaces and the root frame is not missing,
  // the display scheduler is likely to produce proper frame, so let it do
  // its work. Otherwise, fire the pending frame callback early.
  if (!display_->IsRootFrameMissing() &&
      !display_->HasPendingSurfaces(last_begin_frame_args_)) {
    return;
  }

  // All frame sinks are done with frame, yet the root frame is still missing,
  // the display won't draw, so resolve callback now.
  BeginFrameAck nak(last_begin_frame_args_.frame_id.source_id,
                    last_begin_frame_args_.frame_id.sequence_number,
                    /*has_damage=*/false);
  DispatchFrameCallback(nak);
}

void ExternalBeginFrameSourceMojo::DispatchFrameCallback(
    const BeginFrameAck& ack) {
  // If there are pending copy output requests that have not been fulfilled,
  // cancel them, as they won't be served till the next frame. This prevents
  // the client for waiting for them indefinitely.
  frame_sink_manager_->DiscardPendingCopyOfOutputRequests(this);
  // Prevent missing begin frames from being sent to sinks that came late,
  // as this may result in two overlapping frames being sent, which is not
  // supported with full pipeline mode.
  last_begin_frame_args_ = BeginFrameArgs();
  std::move(pending_frame_callback_).Run(ack);
}

void ExternalBeginFrameSourceMojo::OnDisplayDidFinishFrame(
    const BeginFrameAck& ack) {
  if (!pending_frame_callback_)
    return;
  if (!pending_frame_sinks_.empty()) {
    DCHECK(!pending_ack_);
    pending_ack_ = ack;
    return;
  }
  DispatchFrameCallback(ack);
}

void ExternalBeginFrameSourceMojo::OnDisplayDestroyed() {
  // As part of destruction, we are automatically removed as a display
  // observer. No need to call RemoveObserver.
  display_ = nullptr;
}

void ExternalBeginFrameSourceMojo::SetDisplay(Display* display) {
  if (display_)
    display_->RemoveObserver(this);
  display_ = display;
  if (display_)
    display_->AddObserver(this);
}

}  // namespace viz
