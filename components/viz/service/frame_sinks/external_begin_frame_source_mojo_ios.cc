// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo_ios.h"

#include <utility>

namespace viz {

ExternalBeginFrameSourceMojoIOS::ExternalBeginFrameSourceMojoIOS(
    mojo::PendingAssociatedReceiver<mojom::ExternalBeginFrameController>
        controller_receiver,
    mojo::PendingAssociatedRemote<mojom::ExternalBeginFrameControllerClient>
        controller_client_remote,
    uint32_t restart_id)
    : ExternalBeginFrameSource(this, restart_id),
      receiver_(this, std::move(controller_receiver)),
      remote_client_(std::move(controller_client_remote)) {}

ExternalBeginFrameSourceMojoIOS::~ExternalBeginFrameSourceMojoIOS() = default;

void ExternalBeginFrameSourceMojoIOS::IssueExternalBeginFrameNoAck(
    const BeginFrameArgs& args) {
  OnBeginFrame(args);
}

void ExternalBeginFrameSourceMojoIOS::OnNeedsBeginFrames(
    bool needs_begin_frames) {
  if (remote_client_) {
    remote_client_->SetNeedsBeginFrame(needs_begin_frames);
  }
}

void ExternalBeginFrameSourceMojoIOS::SetPreferredInterval(
    base::TimeDelta interval) {
  if (remote_client_) {
    remote_client_->SetPreferredInterval(interval);
  }
}

}  // namespace viz
