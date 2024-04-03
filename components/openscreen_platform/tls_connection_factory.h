// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPENSCREEN_PLATFORM_TLS_CONNECTION_FACTORY_H_
#define COMPONENTS_OPENSCREEN_PLATFORM_TLS_CONNECTION_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "services/network/public/mojom/tls_socket.mojom.h"
#include "third_party/openscreen/src/platform/api/tls_connection_factory.h"
#include "third_party/openscreen/src/platform/base/ip_address.h"
#include "third_party/openscreen/src/platform/base/tls_connect_options.h"

namespace net {
class IPEndPoint;
}

namespace openscreen {

struct TlsCredentials;
struct TlsListenOptions;

}  // namespace openscreen

namespace openscreen_platform {

class TlsConnectionFactory final : public openscreen::TlsConnectionFactory {
 public:
  explicit TlsConnectionFactory(
      openscreen::TlsConnectionFactory::Client& client);

  ~TlsConnectionFactory() final;

  // TlsConnectionFactory overrides
  void Connect(const openscreen::IPEndpoint& remote_address,
               const openscreen::TlsConnectOptions& options) final;

  // Since Chrome doesn't implement TLS server sockets, these methods are not
  // implemented.
  void SetListenCredentials(
      const openscreen::TlsCredentials& credentials) final;
  void Listen(const openscreen::IPEndpoint& local_address,
              const openscreen::TlsListenOptions& options) final;

 private:
  // Note on TcpConnectRequest and TlsUpgradeRequest:
  // These classes are used to manage connection state for creating TCP.
  // connections and upgrading them to TLS. They are movable, but not copyable,
  // due to unique ownership of the mojo::Remotes, and passed into the TCP/TLS
  // callbacks (OnTcpConnect and OnTlsUpgrade) using currying.
  struct TcpConnectRequest {
    TcpConnectRequest(
        openscreen::TlsConnectOptions options_in,
        openscreen::IPEndpoint remote_address_in,
        mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in);
    TcpConnectRequest(const TcpConnectRequest&) = delete;
    TcpConnectRequest(TcpConnectRequest&&);
    TcpConnectRequest& operator=(const TcpConnectRequest&) = delete;
    TcpConnectRequest& operator=(TcpConnectRequest&&);
    ~TcpConnectRequest();

    openscreen::TlsConnectOptions options;
    openscreen::IPEndpoint remote_address;
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket;
  };

  struct TlsUpgradeRequest {
    TlsUpgradeRequest(
        openscreen::IPEndpoint local_address_in,
        openscreen::IPEndpoint remote_address_in,
        mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in,
        mojo::Remote<network::mojom::TLSClientSocket> tls_socket_in);
    TlsUpgradeRequest(const TlsUpgradeRequest&) = delete;
    TlsUpgradeRequest(TlsUpgradeRequest&&);
    TlsUpgradeRequest& operator=(const TlsUpgradeRequest&) = delete;
    TlsUpgradeRequest& operator=(TlsUpgradeRequest&&);
    ~TlsUpgradeRequest();

    openscreen::IPEndpoint local_address;
    openscreen::IPEndpoint remote_address;
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket;
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket;
  };

  void OnTcpConnect(TcpConnectRequest request,
                    int32_t net_result,
                    const std::optional<net::IPEndPoint>& local_address,
                    const std::optional<net::IPEndPoint>& remote_address,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream);

  void OnTlsUpgrade(TlsUpgradeRequest request,
                    int32_t net_result,
                    mojo::ScopedDataPipeConsumerHandle receive_stream,
                    mojo::ScopedDataPipeProducerHandle send_stream,
                    const std::optional<net::SSLInfo>& ssl_info);

  raw_ref<openscreen::TlsConnectionFactory::Client> client_;
  base::WeakPtrFactory<TlsConnectionFactory> weak_factory_{this};
};

}  // namespace openscreen_platform

#endif  // COMPONENTS_OPENSCREEN_PLATFORM_TLS_CONNECTION_FACTORY_H_
