// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo_mac.h"

#include <utility>

#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/display/mac/vsync_provider_mac.h"

namespace viz {

ExternalBeginFrameSourceMojoMac::ExternalBeginFrameSourceMojoMac(
    mojo::PendingReceiver<mojom::ExternalBeginFrameController>
        controller_receiver,
    mojo::PendingRemote<mojom::ExternalBeginFrameControllerClient>
        controller_remote_client,
    base::RepeatingClosure update_vsync_displays_cb)
    : receiver_(std::in_place_type<Receiver>, this),
      remote_client_(std::move(controller_remote_client)),
      update_vsync_displays_cb_(std::move(update_vsync_displays_cb)) {
  CHECK(remote_client_);

  if (mojo::IsDirectReceiverSupported() &&
      features::IsVizDirectCompositorThreadIpcFrameSinkManagerEnabled()) {
    receiver_.emplace<DirectReceiver>(mojo::DirectReceiverKey{}, this);
  }

  std::visit(
      [&](auto& receiver) { receiver.Bind(std::move(controller_receiver)); },
      receiver_);

  ui::NeedsBeginFrameCB callback = base::BindRepeating(
      &ExternalBeginFrameSourceMojoMac::NeedsBeginFrameWithId,
      weak_factory_.GetWeakPtr());
  ui::VSyncProviderMac::GetInstance()->SetCallbackForRemoteNeedsBeginFrame(
      std::move(callback));
}

ExternalBeginFrameSourceMojoMac::~ExternalBeginFrameSourceMojoMac() {
  remote_client_->SetNeedsBeginFrame(false);
}

// mojom::ExternalBeginFrameController implementation.
void ExternalBeginFrameSourceMojoMac::IssueExternalVSync(
    const CADisplayLinkParams& params) {
  ui::VSyncParamsMac ui_params(true, params.timestamp, params.interval, true,
                               params.target_timestamp, params.interval);
  ui::VSyncProviderMac::GetInstance()->OnVSync(ui_params, params.display_id);
}

void ExternalBeginFrameSourceMojoMac::SetSupportedDisplayLinkId(
    int64_t display_id,
    bool is_supported) {
  ui::VSyncProviderMac::GetInstance()->SetSupportedDisplayLinkId(display_id,
                                                                 is_supported);
  // Update DisplayLinkMac in every ExternalBeginFrameSourceMac if needed.
  update_vsync_displays_cb_.Run();
}

void ExternalBeginFrameSourceMojoMac::IssueExternalBeginFrame(
    const BeginFrameArgs& args,
    bool force,
    IssueExternalBeginFrameCallback callback) {
  // IssueExternalBeginFrame on Mac is for headless only.
  NOTREACHED();
}

void ExternalBeginFrameSourceMojoMac::NeedsBeginFrameWithId(
    int64_t display_id,
    bool needs_begin_frames) {
  remote_client_->NeedsBeginFrameWithId(display_id, needs_begin_frames);
}

}  // namespace viz
