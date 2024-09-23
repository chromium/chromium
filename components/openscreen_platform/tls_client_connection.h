// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_TLS_CLIENT_CONNECTION_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_TLS_CLIENT_CONNECTION_H_

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/error.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"

namespace openscreen_platform {

class TlsClientConnection final : public openscreen::TlsConnection {
 public:
  TlsClientConnection(
      openscreen::IPEndpoint local_address,
      openscreen::IPEndpoint remote_address,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream,
      mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket,
      mojo::Remote<network::mojom::TLSClientSocket> tls_socket);

  ~TlsClientConnection() final;

  // TlsConnection overrides.
  void SetClient(Client* client) final;
  bool Send(openscreen::ByteView data) final;
  openscreen::IPEndpoint GetRemoteEndpoint() const final;

  // The maximum size of the vector in any single Client::OnRead() callback.
  static constexpr size_t kMaxBytesPerRead = 64 << 10;  // 64 KB.

 private:
  // Invoked by |receive_stream_watcher_| when the |receive_stream_|'s status
  // has changed. Calls Client::OnRead() if data has become available.
  void ReceiveMore(MojoResult result, const mojo::HandleSignalsState& state);

  // Classifies MojoResult codes into one of three categories: kOk, kAgain for
  // transient errors, or |error_code_if_fatal| for fatal errors. If the result
  // is a fatal error, this also invokes Client::OnError().
  openscreen::Error::Code ProcessMojoResult(
      MojoResult result,
      openscreen::Error::Code error_code_if_fatal);

  const openscreen::IPEndpoint local_address_;
  const openscreen::IPEndpoint remote_address_;
  const mojo::ScopedDataPipeConsumerHandle receive_stream_;
  const mojo::ScopedDataPipeProducerHandle send_stream_;
  const mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_;
  const mojo::Remote<network::mojom::TLSClientSocket> tls_socket_;

  mojo::SimpleWatcher receive_stream_watcher_;

  raw_ptr<Client> client_ = nullptr;
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_TLS_CLIENT_CONNECTION_H_
