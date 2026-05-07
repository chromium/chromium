// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/media_remoter.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "components/mirroring/service/rpc_dispatcher.h"

namespace mirroring {

MediaRemoter::MediaRemoter(
    Client& client,
    const media::mojom::RemotingSinkMetadata& sink_metadata,
    RpcDispatcher& rpc_dispatcher)
    : client_(client),
      sink_metadata_(sink_metadata),
      rpc_dispatcher_(rpc_dispatcher),
      state_(MIRRORING) {
  client_->ConnectToRemotingSource(
      receiver_.BindNewPipeAndPassRemote(),
      remoting_source_.BindNewPipeAndPassReceiver());
  remoting_source_->OnSinkAvailable(sink_metadata_.Clone());
}

MediaRemoter::~MediaRemoter() {
  // Stop this remoting session if mirroring is stopped during a remoting
  // session. For example, user stops mirroring through the cast dialog or
  // closes the tab.
  Stop(media::mojom::RemotingStopReason::ROUTE_TERMINATED);
}

void MediaRemoter::OnMessageFromSink(const std::vector<uint8_t>& response) {
  remoting_source_->OnMessageFromSink(response);
}

void MediaRemoter::OnRemotingStarted() {
  if (state_ != STARTING_REMOTING) {
    return;  // Start operation was canceled.
  }

  // A remoting streaming session started. Start RPC message transport and
  // notify the remoting source to start data streaming.
  rpc_dispatcher_->Subscribe(base::BindRepeating(
      &MediaRemoter::OnMessageFromSink, weak_factory_.GetWeakPtr()));
  state_ = REMOTING_STARTED;
  remoting_source_->OnStarted();
}

void MediaRemoter::OnMirroringResumed(bool is_tab_switching) {
  if (state_ == REMOTING_DISABLED) {
    return;
  }
  DCHECK(state_ == STOPPING_REMOTING ||
         (state_ == MIRRORING && is_tab_switching));

  state_ = MIRRORING;

  if (is_tab_switching) {
    receiver_.reset();
    remoting_source_.reset();
    client_->ConnectToRemotingSource(
        receiver_.BindNewPipeAndPassRemote(),
        remoting_source_.BindNewPipeAndPassReceiver());
  }

  // Notify the remoting source to enable starting media remoting again.
  remoting_source_->OnSinkAvailable(sink_metadata_.Clone());
}

void MediaRemoter::OnRemotingFailed() {
  DCHECK(state_ == STARTING_REMOTING || state_ == REMOTING_STARTED);
  if (state_ == STARTING_REMOTING) {
    remoting_source_->OnStartFailed(
        media::mojom::RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
  }
  state_ = REMOTING_DISABLED;
  remoting_source_->OnSinkGone();
  // Fallback to mirroring.
  client_->RestartMirroringStreaming();
}

void MediaRemoter::Stop(media::mojom::RemotingStopReason reason) {
  if (state_ == STOPPING_REMOTING || state_ == MIRRORING) {
    return;
  }

  // At this point, we are currently remoting and should tear down.
  rpc_dispatcher_->Unsubscribe();
  audio_sender_.reset();
  video_sender_.reset();

  // Don't change `state_` if remoting is disabled so that it won't attempt to
  // start remoting again after mirroring resumed.
  if (state_ != REMOTING_DISABLED) {
    state_ = STOPPING_REMOTING;
  }
  remoting_source_->OnStopped(reason);
  // Prevent the start of remoting until switching completes.
  remoting_source_->OnSinkGone();
  // Switch to mirroring.
  client_->RestartMirroringStreaming();
}

void MediaRemoter::Start() {
  if (state_ != MIRRORING) {
    VLOG(2) << "Warning: Ignore start request. state=" << state_;
    return;
  }
  state_ = STARTING_REMOTING;
  client_->RequestRemotingStreaming();
}

void MediaRemoter::StartWithPermissionAlreadyGranted() {
  NOTIMPLEMENTED();
}

void MediaRemoter::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        audio_sender_receiver,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender_receiver) {
  if (state_ != REMOTING_STARTED) {
    return;  // Stop() was called before.
  }

  if (audio_pipe.is_valid()) {
    auto audio_sender = client_->CreateRemotingDataStreamSender(
        /*is_audio=*/true, std::move(audio_pipe),
        std::move(audio_sender_receiver),
        base::BindOnce(&MediaRemoter::OnRemotingDataStreamError,
                       base::Unretained(this)));
    if (audio_sender) {
      audio_sender_ = std::move(audio_sender);
    }
  }

  if (video_pipe.is_valid()) {
    auto video_sender = client_->CreateRemotingDataStreamSender(
        /*is_audio=*/false, std::move(video_pipe),
        std::move(video_sender_receiver),
        base::BindOnce(&MediaRemoter::OnRemotingDataStreamError,
                       base::Unretained(this)));
    if (video_sender) {
      video_sender_ = std::move(video_sender);
    }
  }
}

void MediaRemoter::SendMessageToSink(const std::vector<uint8_t>& message) {
  if (state_ != REMOTING_STARTED) {
    return;
  }
  rpc_dispatcher_->SendOutboundMessage(message);
}

void MediaRemoter::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(0);
}

void MediaRemoter::OnRemotingDataStreamError() {
  if (state_ != REMOTING_STARTED) {
    return;
  }
  Stop(media::mojom::RemotingStopReason::DATA_SEND_FAILED);
  state_ = REMOTING_DISABLED;
}

}  // namespace mirroring
