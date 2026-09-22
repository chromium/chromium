// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/redirection/redirection_rpc_dispatcher.h"

#include <wrl/implements.h>

#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "third_party/openscreen/src/cast/streaming/remoting.pb.h"

namespace redirection {

namespace {

// Implements IMMRResponseHandler on top of the subscribed ResponseCallback.
class ResponseHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IMMRResponseHandler,
          Microsoft::WRL::FtmBase> {
 public:
  explicit ResponseHandler(mirroring::RpcDispatcher::ResponseCallback callback)
      : callback_(std::move(callback)) {}

  // IMMRResponseHandler implementation.
  IFACEMETHODIMP OnResponse(BYTE* message, UINT32 messageSize) override {
    // SAFETY: IMMRResponseHandler::OnResponse() documents `messageSize` as the
    // number of valid bytes in `message`.
    callback_.Run(base::ToVector(
        UNSAFE_BUFFERS(base::span(message, size_t{messageSize}))));
    return S_OK;
  }

 private:
  const mirroring::RpcDispatcher::ResponseCallback callback_;
};

}  // namespace

RedirectionRpcDispatcher::RedirectionRpcDispatcher(
    IMMRSessionOpenscreen* mmr_session,
    DemuxerHandlesCallback demuxer_handles_callback)
    : mmr_session_(mmr_session),
      demuxer_handles_callback_(std::move(demuxer_handles_callback)) {}

RedirectionRpcDispatcher::~RedirectionRpcDispatcher() {
  // Subscribe() handed the session a reference to our handler, which would
  // otherwise keep delivering responses to a callback that is going away.
  Unsubscribe();
}

void RedirectionRpcDispatcher::Subscribe(ResponseCallback callback,
                                         ErrorCallback error_callback) {
  response_callback_ = std::move(callback);
  error_callback_ = std::move(error_callback);
  response_handler_ =
      Microsoft::WRL::Make<ResponseHandler>(base::BindPostTaskToCurrentDefault(
          base::BindRepeating(&RedirectionRpcDispatcher::RunResponseCallback,
                              weak_factory_.GetWeakPtr())));
  const HRESULT hr = mmr_session_->SetResponseHandler(response_handler_.Get());
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMRSessionOpenscreen::SetResponseHandler failed: 0x"
               << std::hex << hr;
    response_handler_.Reset();
    response_callback_.Reset();
    if (ErrorCallback cb = std::move(error_callback_)) {
      cb.Run();
    }
  }
}

void RedirectionRpcDispatcher::Unsubscribe() {
  response_callback_.Reset();
  error_callback_.Reset();
  weak_factory_.InvalidateWeakPtrs();

  mmr_session_->SetResponseHandler(nullptr);
  response_handler_.Reset();
}

void RedirectionRpcDispatcher::RunResponseCallback(
    const std::vector<uint8_t>& response) {
  if (response_callback_) {
    ResponseCallback cb = response_callback_;
    cb.Run(response);
  }
}

bool RedirectionRpcDispatcher::SendOutboundMessage(
    base::span<const uint8_t> message) {
  // MIDL does not emit `const` for `[in]` pointer parameters, so the buffer
  // has to be cast even though the IDL marks it read-only.
  const HRESULT hr = mmr_session_->SendRawOpenscreenMessage(
      const_cast<BYTE*>(message.data()), static_cast<UINT32>(message.size()));
  if (FAILED(hr)) {
    LOG(ERROR) << "IMMRSessionOpenscreen::SendRawOpenscreenMessage failed: 0x"
               << std::hex << hr;
    if (error_callback_) {
      ErrorCallback cb = error_callback_;
      cb.Run();
    }
    return false;
  }

  // RPC_ACQUIRE_DEMUXER names the stream ids the sink will read media from, so
  // the session opens its IMMRStreams from them. This runs only after the
  // session has processed the message, since opening those streams requires it
  // to have seen the acquire first.
  if (demuxer_handles_callback_) {
    openscreen::cast::RpcMessage rpc;
    if (rpc.ParseFromArray(message.data(), static_cast<int>(message.size())) &&
        rpc.proc() == openscreen::cast::RpcMessage::RPC_ACQUIRE_DEMUXER &&
        rpc.has_acquire_demuxer_rpc()) {
      const auto& demuxer = rpc.acquire_demuxer_rpc();
      std::move(demuxer_handles_callback_)
          .Run(demuxer.audio_demuxer_handle(), demuxer.video_demuxer_handle());
    }
  }

  return true;
}

}  // namespace redirection
