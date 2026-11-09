// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_session_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/services/redirection/redirection_remoting_sender.h"
#include "chrome/services/redirection/redirection_rpc_dispatcher.h"
#include "third_party/openscreen/src/cast/streaming/public/rpc_messenger.h"

namespace redirection {

namespace {

bool OpenStreamFor(IMMRSessionOpenscreen* mmr_session,
                   int32_t demuxer_handle,
                   base::WeakPtr<RedirectionRemotingSender> sender) {
  // A source that remotes only video names no audio demuxer, and offers no
  // audio pipe to send one over.
  // StartDataStreams() must precede RPC_ACQUIRE_DEMUXER on the same Remoter
  // pipe: the demuxer callback runs only once, so a null `sender` must mean
  // the source has no pipe for this stream, not that setup is still pending.
  if (demuxer_handle == openscreen::cast::RpcMessenger::kInvalidHandle ||
      !sender) {
    return true;
  }

  Microsoft::WRL::ComPtr<IMMRStream> stream;
  const HRESULT hr =
      mmr_session->CreateStream(static_cast<UINT32>(demuxer_handle), &stream);
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMRSessionOpenscreen::CreateStream failed: 0x" << std::hex
               << hr;
    return false;
  }
  sender->SetStream(stream.Get());
  return true;
}

}  // namespace

RedirectionSessionHost::RedirectionSessionHost(
    IMMRSessionOpenscreen* mmr_session,
    const media::mojom::RemotingSinkMetadata& sink_metadata,
    mojo::PendingReceiver<mojom::RedirectionSessionHost> session_host,
    mojo::PendingReceiver<media::mojom::Remoter> remoter,
    mojo::PendingRemote<media::mojom::RemotingSource> source,
    DisconnectedCallback disconnected_callback)
    : mmr_session_(mmr_session),
      session_host_receiver_(this, std::move(session_host)),
      pending_remoter_(std::move(remoter)),
      pending_source_(std::move(source)),
      rpc_dispatcher_(std::make_unique<RedirectionRpcDispatcher>(
          mmr_session,
          base::BindOnce(&RedirectionSessionHost::OnDemuxerHandlesReceived,
                         base::Unretained(this)))) {
  session_host_receiver_.set_disconnect_handler(
      base::BindOnce(std::move(disconnected_callback), base::Unretained(this)));

  // MediaRemoter announces the sink to the source from its constructor, so
  // this is what tells the browser the session is usable.
  media_remoter_ = std::make_unique<mirroring::MediaRemoter>(
      *this, sink_metadata, *rpc_dispatcher_);
}

RedirectionSessionHost::~RedirectionSessionHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EndSession();
}

void RedirectionSessionHost::ConnectToRemotingSource(
    mojo::PendingRemote<media::mojom::Remoter> remoter,
    mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The browser created both pipes when it asked for this session, so splice
  // MediaRemoter's ends onto the ends it is already holding.
  CHECK(mojo::FusePipes(std::move(pending_remoter_), std::move(remoter)));
  CHECK(
      mojo::FusePipes(std::move(source_receiver), std::move(pending_source_)));
}

void RedirectionSessionHost::RequestRemotingStreaming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The session already exists and needs no negotiation, and the streams it
  // carries are opened by the RPC traffic that starts here.
  media_remoter_->OnRemotingStarted();
}

void RedirectionSessionHost::RestartMirroringStreaming() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Redirection has no mirroring mode to fall back to, so a remoter that has
  // finished remoting has nothing left to do.
  EndSession();
}

std::unique_ptr<media::mojom::RemotingDataStreamSender>
RedirectionSessionHost::CreateRemotingDataStreamSender(
    bool is_audio,
    mojo::ScopedDataPipeConsumerHandle pipe,
    mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> receiver,
    base::OnceClosure error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto sender = std::make_unique<RedirectionRemotingSender>(
      std::move(pipe), std::move(receiver), std::move(error_callback));

  // The stream this sender writes to is opened once the sink names its
  // demuxer handles over RPC, so keep track of the sender until then.
  (is_audio ? audio_sender_ : video_sender_) = sender->GetWeakPtr();
  return sender;
}

void RedirectionSessionHost::EndSession() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!media_remoter_) {
    return;
  }

  // Release in dependency order.
  media_remoter_.reset();
  rpc_dispatcher_.reset();
  mmr_session_.Reset();
}

void RedirectionSessionHost::OnDemuxerHandlesReceived(
    int32_t audio_demuxer_handle,
    int32_t video_demuxer_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!OpenStreamFor(mmr_session_.Get(), audio_demuxer_handle, audio_sender_) ||
      !OpenStreamFor(mmr_session_.Get(), video_demuxer_handle, video_sender_)) {
    // A sender with no stream leaves its source blocked on a frame that can
    // never be acknowledged.
    media_remoter_->Stop(media::mojom::RemotingStopReason::DATA_SEND_FAILED);
    return;
  }
}

}  // namespace redirection
