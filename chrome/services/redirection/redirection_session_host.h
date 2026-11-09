// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REDIRECTION_REDIRECTION_SESSION_HOST_H_
#define CHROME_SERVICES_REDIRECTION_REDIRECTION_SESSION_HOST_H_

#include <wrl/client.h>

#include <cstdint>
#include <memory>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/services/redirection/public/mojom/redirection_service.mojom.h"
#include "components/mirroring/service/media_remoter.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "media/mojo/mojom/remoting_common.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "third_party/microsoft_multimedia_redirection/src/api/Mmr_h.h"

namespace redirection {

class RedirectionRemotingSender;
class RedirectionRpcDispatcher;

// Serves one remoting source out of an MMR session. It owns the MediaRemoter
// the source drives, and carries that remoter's traffic over the session. RPC
// travels on the session's openscreen channel, and each remoting data stream
// on an IMMRStream.
class RedirectionSessionHost final : public mirroring::MediaRemoter::Client,
                                     public mojom::RedirectionSessionHost {
 public:
  // Run when the browser closes the session host pipe. It carries the host's
  // own identity, since the owner keeps its hosts in a container keyed on it.
  using DisconnectedCallback =
      base::OnceCallback<void(RedirectionSessionHost*)>;

  RedirectionSessionHost(
      IMMRSessionOpenscreen* mmr_session,
      const media::mojom::RemotingSinkMetadata& sink_metadata,
      mojo::PendingReceiver<mojom::RedirectionSessionHost> session_host,
      mojo::PendingReceiver<media::mojom::Remoter> remoter,
      mojo::PendingRemote<media::mojom::RemotingSource> source,
      DisconnectedCallback disconnected_callback);
  RedirectionSessionHost(const RedirectionSessionHost&) = delete;
  RedirectionSessionHost& operator=(const RedirectionSessionHost&) = delete;
  ~RedirectionSessionHost() override;

 private:
  // mirroring::MediaRemoter::Client implementation.
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> source_receiver)
      override;
  void RequestRemotingStreaming() override;
  void RestartMirroringStreaming() override;
  std::unique_ptr<media::mojom::RemotingDataStreamSender>
  CreateRemotingDataStreamSender(
      bool is_audio,
      mojo::ScopedDataPipeConsumerHandle pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender> receiver,
      base::OnceClosure error_callback) override;

  // Releases everything this session runs on. The browser learns of it when
  // the pipes MediaRemoter owns close.
  void EndSession();

  // Opens an IMMRStream for each handle the sink named, and hands it to the
  // sender that will write to it.
  void OnDemuxerHandlesReceived(int32_t audio_demuxer_handle,
                                int32_t video_demuxer_handle);

  Microsoft::WRL::ComPtr<IMMRSessionOpenscreen> mmr_session_;

  mojo::Receiver<mojom::RedirectionSessionHost> session_host_receiver_;

  // The browser's ends of the Remoter and RemotingSource pipes, held until
  // MediaRemoter hands over its own ends to splice them onto.
  mojo::PendingReceiver<media::mojom::Remoter> pending_remoter_;
  mojo::PendingRemote<media::mojom::RemotingSource> pending_source_;

  std::unique_ptr<RedirectionRpcDispatcher> rpc_dispatcher_;
  std::unique_ptr<mirroring::MediaRemoter> media_remoter_;

  // Owned by `media_remoter_`.
  base::WeakPtr<RedirectionRemotingSender> audio_sender_;
  base::WeakPtr<RedirectionRemotingSender> video_sender_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace redirection

#endif  // CHROME_SERVICES_REDIRECTION_REDIRECTION_SESSION_HOST_H_
