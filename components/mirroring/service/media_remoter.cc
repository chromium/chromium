// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/media_remoter.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/mirroring/service/message_dispatcher.h"
#include "components/mirroring/service/remoting_sender.h"
#include "media/cast/net/cast_transport.h"

using media::cast::Codec;
using media::cast::FrameSenderConfig;

namespace mirroring {

MediaRemoter::MediaRemoter(
    Client* client,
    const media::mojom::RemotingSinkMetadata& sink_metadata,
    MessageDispatcher* message_dispatcher)
    : client_(client),
      sink_metadata_(sink_metadata),
      message_dispatcher_(message_dispatcher),
      cast_environment_(nullptr),
      transport_(nullptr),
      state_(MIRRORING) {
  DCHECK(client_);
  DCHECK(message_dispatcher_);

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

void MediaRemoter::OnMessageFromSink(const ReceiverResponse& response) {
  DCHECK_EQ(ResponseType::RPC, response.type);
  remoting_source_->OnMessageFromSink(
      std::vector<uint8_t>(response.rpc.begin(), response.rpc.end()));
}

void MediaRemoter::StartRpcMessaging(
    scoped_refptr<media::cast::CastEnvironment> cast_environment,
    media::cast::CastTransport* transport,
    const FrameSenderConfig& audio_config,
    const FrameSenderConfig& video_config) {
  DCHECK(!cast_environment_);
  DCHECK(!transport_);
  DCHECK_EQ(Codec::CODEC_UNKNOWN, audio_config_.codec);
  DCHECK_EQ(Codec::CODEC_UNKNOWN, video_config_.codec);
  DCHECK(audio_config.codec == Codec::CODEC_AUDIO_REMOTE ||
         video_config.codec == Codec::CODEC_VIDEO_REMOTE);

  if (state_ != STARTING_REMOTING)
    return;  // Start operation was canceled.
  // A remoting streaming session started. Start RPC message transport and
  // notify the remoting source to start data streaming.
  cast_environment_ = std::move(cast_environment);
  transport_ = transport;
  audio_config_ = audio_config;
  video_config_ = video_config;
  message_dispatcher_->Subscribe(
      ResponseType::RPC, base::BindRepeating(&MediaRemoter::OnMessageFromSink,
                                             weak_factory_.GetWeakPtr()));
  state_ = REMOTING_STARTED;
  remoting_source_->OnStarted();
}

void MediaRemoter::OnMirroringResumed() {
  if (state_ == REMOTING_DISABLED)
    return;
  DCHECK_EQ(STOPPING_REMOTING, state_);
  state_ = MIRRORING;
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
  if (state_ != STARTING_REMOTING && state_ != REMOTING_STARTED)
    return;
  if (state_ == REMOTING_STARTED) {
    message_dispatcher_->Unsubscribe(ResponseType::RPC);
    audio_sender_.reset();
    video_sender_.reset();
    cast_environment_ = nullptr;
    transport_ = nullptr;
    audio_config_ = FrameSenderConfig();
    video_config_ = FrameSenderConfig();
  }
  state_ = STOPPING_REMOTING;
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

void MediaRemoter::StartDataStreams(
    mojo::ScopedDataPipeConsumerHandle audio_pipe,
    mojo::ScopedDataPipeConsumerHandle video_pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        audio_sender_receiver,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
        video_sender_receiver) {
  if (state_ != REMOTING_STARTED)
    return;  // Stop() was called before.
  DCHECK(cast_environment_);
  DCHECK(transport_);
  if (audio_pipe.is_valid() &&
      audio_config_.codec == Codec::CODEC_AUDIO_REMOTE) {
    audio_sender_ = std::make_unique<RemotingSender>(
        cast_environment_, transport_, audio_config_, std::move(audio_pipe),
        std::move(audio_sender_receiver),
        base::BindOnce(&MediaRemoter::OnRemotingDataStreamError,
                       base::Unretained(this)));
  }
  if (video_pipe.is_valid() &&
      video_config_.codec == Codec::CODEC_VIDEO_REMOTE) {
    video_sender_ = std::make_unique<RemotingSender>(
        cast_environment_, transport_, video_config_, std::move(video_pipe),
        std::move(video_sender_receiver),
        base::BindOnce(&MediaRemoter::OnRemotingDataStreamError,
                       base::Unretained(this)));
  }
}

void MediaRemoter::SendMessageToSink(const std::vector<uint8_t>& message) {
  if (state_ != REMOTING_STARTED)
    return;
  std::string encoded_rpc;
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(message.data()),
                        message.size()),
      &encoded_rpc);
  base::Value rpc(base::Value::Type::DICTIONARY);
  rpc.SetKey("type", base::Value("RPC"));
  rpc.SetKey("rpc", base::Value(std::move(encoded_rpc)));
  mojom::CastMessagePtr rpc_message = mojom::CastMessage::New();
  rpc_message->message_namespace = mojom::kRemotingNamespace;
  const bool did_serialize_rpc =
      base::JSONWriter::Write(rpc, &rpc_message->json_format_data);
  DCHECK(did_serialize_rpc);
  message_dispatcher_->SendOutboundMessage(std::move(rpc_message));
}

void MediaRemoter::EstimateTransmissionCapacity(
    media::mojom::Remoter::EstimateTransmissionCapacityCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(0);
}

void MediaRemoter::OnRemotingDataStreamError() {
  if (state_ != REMOTING_STARTED)
    return;
  state_ = REMOTING_DISABLED;
  Stop(media::mojom::RemotingStopReason::DATA_SEND_FAILED);
}

}  // namespace mirroring
