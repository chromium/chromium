// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/openscreen_platform/tls_client_connection.h"

#include <algorithm>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"

namespace openscreen_platform {

using openscreen::Error;

TlsClientConnection::TlsClientConnection(
    openscreen::IPEndpoint local_address,
    openscreen::IPEndpoint remote_address,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket,
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket)
    : local_address_(std::move(local_address)),
      remote_address_(std::move(remote_address)),
      receive_stream_(std::move(receive_stream)),
      send_stream_(std::move(send_stream)),
      tcp_socket_(std::move(tcp_socket)),
      tls_socket_(std::move(tls_socket)),
      receive_stream_watcher_(FROM_HERE,
                              mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
  if (receive_stream_.is_valid()) {
    receive_stream_watcher_.Watch(
        receive_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED |
            MOJO_HANDLE_SIGNAL_NEW_DATA_READABLE,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&TlsClientConnection::ReceiveMore,
                            base::Unretained(this)));
    receive_stream_watcher_.ArmOrNotify();
  }
}

TlsClientConnection::~TlsClientConnection() = default;

void TlsClientConnection::SetClient(Client* client) {
  client_ = client;
}

bool TlsClientConnection::Send(openscreen::ByteView data) {
  if (!send_stream_.is_valid()) {
    if (client_) {
      client_->OnError(this, Error(Error::Code::kSocketClosedFailure,
                                   "Send stream was closed."));
    }
    return false;
  }

  const MojoResult result = send_stream_->WriteAllData(data);
  mojo::HandleSignalsState state = send_stream_->QuerySignalsState();
  return ProcessMojoResult(result, state.peer_closed()
                                       ? Error::Code::kSocketClosedFailure
                                       : Error::Code::kSocketSendFailure) ==
         Error::Code::kNone;
}

openscreen::IPEndpoint TlsClientConnection::GetRemoteEndpoint() const {
  return remote_address_;
}

void TlsClientConnection::ReceiveMore(MojoResult result,
                                      const mojo::HandleSignalsState& state) {
  if (!receive_stream_.is_valid()) {
    if (client_) {
      client_->OnError(this, Error(Error::Code::kSocketClosedFailure,
                                   "Receive stream was closed."));
    }
    return;
  }

  if (result == MOJO_RESULT_OK) {
    size_t num_bytes = 0;
    result = receive_stream_->ReadData(MOJO_READ_DATA_FLAG_QUERY,
                                       base::span<uint8_t>(), num_bytes);
    if (result == MOJO_RESULT_OK) {
      num_bytes = std::min(num_bytes, kMaxBytesPerRead);
      std::vector<uint8_t> buffer(num_bytes);
      result = receive_stream_->ReadData(MOJO_READ_DATA_FLAG_NONE, buffer,
                                         num_bytes);
      if (result == MOJO_RESULT_OK && client_) {
        buffer.resize(num_bytes);
        client_->OnRead(this, std::move(buffer));
      }
    }
  }

  const Error::Code interpretation = ProcessMojoResult(
      result, state.peer_closed() ? Error::Code::kSocketClosedFailure
                                  : Error::Code::kSocketReadFailure);
  if (interpretation == Error::Code::kNone ||
      interpretation == Error::Code::kAgain) {
    receive_stream_watcher_.ArmOrNotify();
  }
}

Error::Code TlsClientConnection::ProcessMojoResult(
    MojoResult result,
    Error::Code error_code_if_fatal) {
  switch (result) {
    case MOJO_RESULT_OK:
      return Error::Code::kNone;

    case MOJO_RESULT_UNAVAILABLE:
    case MOJO_RESULT_OUT_OF_RANGE:  // Cannot write all data (all-or-none mode).
    case MOJO_RESULT_BUSY:
    case MOJO_RESULT_SHOULD_WAIT:
      // Transient (i.e., "try again") errors.
      return Error::Code::kAgain;

    default:
      // Fatal errors.
      if (client_) {
        std::ostringstream error_message;
        error_message << "MojoResult: " << result;
        client_->OnError(this, Error(error_code_if_fatal, error_message.str()));
      }
      return error_code_if_fatal;
  }
}

// static
constexpr size_t TlsClientConnection::kMaxBytesPerRead;

}  // namespace openscreen_platform
