// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/frame_sinks/external_begin_frame_source_mojo_mac.h"

#include <utility>

#include "components/viz/common/features.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace viz {

ExternalBeginFrameSourceMojoMac::ExternalBeginFrameSourceMojoMac(
    mojo::PendingReceiver<mojom::ExternalBeginFrameController>
        controller_receiver,
    mojo::PendingRemote<mojom::ExternalBeginFrameControllerClient>
        controller_remote_client)
    : receiver_(std::in_place_type<Receiver>, this),
      remote_client_(std::move(controller_remote_client)) {
  CHECK(remote_client_);

  if (mojo::IsDirectReceiverSupported() &&
      features::IsVizDirectCompositorThreadIpcFrameSinkManagerEnabled()) {
    receiver_.emplace<DirectReceiver>(mojo::DirectReceiverKey{}, this);
  }

  std::visit(
      [&](auto& receiver) { receiver.Bind(std::move(controller_receiver)); },
      receiver_);
}

ExternalBeginFrameSourceMojoMac::~ExternalBeginFrameSourceMojoMac() {}

void ExternalBeginFrameSourceMojoMac::IssueExternalBeginFrame(
    const BeginFrameArgs& args,
    bool force,
    IssueExternalBeginFrameCallback callback) {
  // IssueExternalBeginFrame on Mac is for headless only.
  NOTREACHED();
}

}  // namespace viz
