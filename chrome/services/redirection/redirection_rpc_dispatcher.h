// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_REDIRECTION_REDIRECTION_RPC_DISPATCHER_H_
#define CHROME_SERVICES_REDIRECTION_REDIRECTION_RPC_DISPATCHER_H_

#include <wrl/client.h>

#include <cstdint>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/mirroring/service/rpc_dispatcher.h"
#include "third_party/microsoft_multimedia_redirection/src/api/Mmr_h.h"

namespace redirection {

// Carries the remoting RPC messages that MediaRemoter exchanges with the sink
// over the MMR session's openscreen channel.
class RedirectionRpcDispatcher final : public mirroring::RpcDispatcher {
 public:
  // Run with the demuxer handles carried by RPC_ACQUIRE_DEMUXER, which are the
  // stream ids the session needs to open the matching IMMRStreams.
  using DemuxerHandlesCallback =
      base::OnceCallback<void(int32_t audio_demuxer_handle,
                              int32_t video_demuxer_handle)>;

  RedirectionRpcDispatcher(IMMRSessionOpenscreen* mmr_session,
                           DemuxerHandlesCallback demuxer_handles_callback);
  RedirectionRpcDispatcher(const RedirectionRpcDispatcher&) = delete;
  RedirectionRpcDispatcher& operator=(const RedirectionRpcDispatcher&) = delete;
  ~RedirectionRpcDispatcher() override;

  // mirroring::RpcDispatcher implementation.
  void Subscribe(ResponseCallback callback,
                 ErrorCallback error_callback) override;
  void Unsubscribe() override;
  bool SendOutboundMessage(base::span<const uint8_t> message) override;

 private:
  void RunResponseCallback(const std::vector<uint8_t>& response);

  Microsoft::WRL::ComPtr<IMMRSessionOpenscreen> mmr_session_;
  Microsoft::WRL::ComPtr<IMMRResponseHandler> response_handler_;
  ResponseCallback response_callback_;
  ErrorCallback error_callback_;
  DemuxerHandlesCallback demuxer_handles_callback_;

  base::WeakPtrFactory<RedirectionRpcDispatcher> weak_factory_{this};
};

}  // namespace redirection

#endif  // CHROME_SERVICES_REDIRECTION_REDIRECTION_RPC_DISPATCHER_H_
