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
      state_(READY) {
  ConnectToSource();
  remoting_source_->OnSinkAvailable(sink_metadata_.Clone());
}

MediaRemoter::~MediaRemoter() {
  // The client destroys the remoter when the Cast session is torn down, which
  // can happen while remoting is active (e.g. user stops casting through the
  // Cast dialog or closes the tab).
  EndRemoting(media::mojom::RemotingStopReason::ROUTE_TERMINATED);
}

void MediaRemoter::OnRemotingStarted() {
  if (state_ != STARTING_REMOTING) {
    return;  // Start operation was canceled.
  }

  // A remoting streaming session started. Start RPC message transport and
  // notify the remoting source to start data streaming.
  base::WeakPtr self = weak_factory_.GetWeakPtr();
  rpc_dispatcher_->Subscribe(
      base::BindRepeating(&MediaRemoter::OnMessageFromSink,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&MediaRemoter::OnRpcError,
                          weak_factory_.GetWeakPtr()));
  if (!self || state_ != STARTING_REMOTING) {
    return;
  }
  state_ = REMOTING_STARTED;
  remoting_source_->OnStarted();
}

void MediaRemoter::OnRemotingStartFailed() {
  if (state_ != STARTING_REMOTING) {
    return;  // Start operation was canceled.
  }
  state_ = REMOTING_DISABLED;
  remoting_source_->OnStartFailed(
      media::mojom::RemotingStartFailReason::INVALID_ANSWER_MESSAGE);
  remoting_source_->OnSinkGone();
}

void MediaRemoter::OnRemotingStopped(media::mojom::RemotingStopReason reason) {
  EndRemoting(reason);
}

void MediaRemoter::OnReadyForRemoting() {
  if (state_ != STOPPING_REMOTING) {
    return;
  }
  state_ = READY;

  // Notify the remoting source to enable starting media remoting again.
  remoting_source_->OnSinkAvailable(sink_metadata_.Clone());
}

void MediaRemoter::OnSourceChanged() {
  if (state_ == REMOTING_DISABLED) {
    return;
  }
  receiver_.reset();
  remoting_source_.reset();
  ConnectToSource();
  if (state_ == READY) {
    remoting_source_->OnSinkAvailable(sink_metadata_.Clone());
  }
}

void MediaRemoter::Start() {
  if (state_ != READY) {
    VLOG(2) << "Warning: Ignore start request. state=" << state_;
    return;
  }
  state_ = STARTING_REMOTING;
  client_->StartRemoting();
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

void MediaRemoter::Stop(media::mojom::RemotingStopReason reason) {
  if (EndRemoting(reason)) {
    // The client may destroy `this`.
    client_->StopRemoting(reason);
  }
}

void MediaRemoter::SendMessageToSink(const std::vector<uint8_t>& message) {
  if (state_ != REMOTING_STARTED) {
    return;
  }
  rpc_dispatcher_->SendOutboundMessage(message);
}

void MediaRemoter::OnMessageFromSink(const std::vector<uint8_t>& response) {
  remoting_source_->OnMessageFromSink(response);
}

void MediaRemoter::OnRpcError() {
  FailRemoting(media::mojom::RemotingStopReason::MESSAGE_SEND_FAILED);
}

void MediaRemoter::OnRemotingDataStreamError() {
  if (state_ != REMOTING_STARTED) {
    return;
  }
  FailRemoting(media::mojom::RemotingStopReason::DATA_SEND_FAILED);
}

void MediaRemoter::ConnectToSource() {
  client_->ConnectToRemotingSource(
      receiver_.BindNewPipeAndPassRemote(),
      remoting_source_.BindNewPipeAndPassReceiver());
}

bool MediaRemoter::EndRemoting(media::mojom::RemotingStopReason reason) {
  if (state_ != STARTING_REMOTING && state_ != REMOTING_STARTED) {
    return false;
  }

  rpc_dispatcher_->Unsubscribe();
  audio_sender_.reset();
  video_sender_.reset();

  state_ = STOPPING_REMOTING;
  remoting_source_->OnStopped(reason);
  // Prevent the start of remoting until the client is ready again.
  remoting_source_->OnSinkGone();
  return true;
}

void MediaRemoter::FailRemoting(media::mojom::RemotingStopReason reason) {
  if (!EndRemoting(reason)) {
    return;
  }
  state_ = REMOTING_DISABLED;
  // The client may destroy `this`.
  client_->StopRemoting(reason);
}

}  // namespace mirroring
