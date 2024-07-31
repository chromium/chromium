// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/openscreen_platform/tls_connection_factory.h"

#include <openssl/pool.h>
#include <utility>

#include "base/notreached.h"
#include "components/openscreen_platform/network_context.h"
#include "components/openscreen_platform/network_util.h"
#include "components/openscreen_platform/tls_client_connection.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_errors.h"
#include "net/ssl/ssl_info.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/openscreen/src/platform/api/tls_connection.h"
#include "third_party/openscreen/src/platform/base/tls_connect_options.h"
#include "third_party/openscreen/src/platform/base/tls_credentials.h"
#include "third_party/openscreen/src/platform/base/tls_listen_options.h"

namespace openscreen {

class TaskRunner;

std::unique_ptr<TlsConnectionFactory> TlsConnectionFactory::CreateFactory(
    Client& client,
    TaskRunner& task_runner) {
  return std::make_unique<openscreen_platform::TlsConnectionFactory>(client);
}

}  // namespace openscreen

namespace openscreen_platform {

namespace {

using openscreen::IPEndpoint;
using openscreen::TlsConnectOptions;
using openscreen::TlsCredentials;
using openscreen::TlsListenOptions;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("openscreen_tls_message", R"(
        semantics {
          sender: "Open Screen"
          description:
            "Open Screen TLS messages are used by the third_party Open Screen "
            "library, primarily for the libcast CastSocket implementation."
          trigger:
            "Any TLS encrypted message that needs to be sent or received by "
            "the Open Screen library."
          data:
            "Messages defined by the Open Screen Protocol specification."
          destination: OTHER
          destination_other:
            "The connection is made to an Open Screen endpoint on the LAN "
            "selected by the user, i.e. via a dialog."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request cannot be disabled, but it would not be sent if user "
            "does not connect to a Open Screen endpoint on the local network."
          policy_exception_justification: "Not implemented."
        })");

}  // namespace

TlsConnectionFactory::~TlsConnectionFactory() = default;

void TlsConnectionFactory::Connect(const IPEndpoint& remote_address,
                                   const TlsConnectOptions& options) {
  network::mojom::NetworkContext* network_context =
      openscreen_platform::GetNetworkContext();
  if (!network_context) {
    client_->OnError(this, openscreen::Error::Code::kItemNotFound);
    return;
  }

  TcpConnectRequest request(options, remote_address,
                            mojo::Remote<network::mojom::TCPConnectedSocket>{});

  const net::AddressList address_list(
      openscreen_platform::ToNetEndPoint(remote_address));

  mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver =
      request.tcp_socket.BindNewPipeAndPassReceiver();

  network_context->CreateTCPConnectedSocket(
      std::nullopt /* local_addr */, address_list,
      nullptr /* tcp_connected_socket_options */,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(receiver), mojo::NullRemote(), /* observer */
      base::BindOnce(&TlsConnectionFactory::OnTcpConnect,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void TlsConnectionFactory::SetListenCredentials(
    const TlsCredentials& credentials) {
  NOTIMPLEMENTED();
}

void TlsConnectionFactory::Listen(const IPEndpoint& local_address,
                                  const TlsListenOptions& options) {
  NOTIMPLEMENTED();
}

TlsConnectionFactory::TlsConnectionFactory(
    openscreen::TlsConnectionFactory::Client& client)
    : client_(client) {}

TlsConnectionFactory::TcpConnectRequest::TcpConnectRequest(
    openscreen::TlsConnectOptions options_in,
    openscreen::IPEndpoint remote_address_in,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in)
    : options(std::move(options_in)),
      remote_address(std::move(remote_address_in)),
      tcp_socket(std::move(tcp_socket_in)) {}
TlsConnectionFactory::TcpConnectRequest::TcpConnectRequest(
    TcpConnectRequest&&) = default;
TlsConnectionFactory::TcpConnectRequest&
TlsConnectionFactory::TcpConnectRequest::operator=(TcpConnectRequest&&) =
    default;

TlsConnectionFactory::TcpConnectRequest::~TcpConnectRequest() = default;

TlsConnectionFactory::TlsUpgradeRequest::TlsUpgradeRequest(
    openscreen::IPEndpoint local_address_in,
    openscreen::IPEndpoint remote_address_in,
    mojo::Remote<network::mojom::TCPConnectedSocket> tcp_socket_in,
    mojo::Remote<network::mojom::TLSClientSocket> tls_socket_in)
    : local_address(std::move(local_address_in)),
      remote_address(std::move(remote_address_in)),
      tcp_socket(std::move(tcp_socket_in)),
      tls_socket(std::move(tls_socket_in)) {}
TlsConnectionFactory::TlsUpgradeRequest::TlsUpgradeRequest(
    TlsUpgradeRequest&&) = default;
TlsConnectionFactory::TlsUpgradeRequest&
TlsConnectionFactory::TlsUpgradeRequest::operator=(TlsUpgradeRequest&&) =
    default;
TlsConnectionFactory::TlsUpgradeRequest::~TlsUpgradeRequest() = default;

void TlsConnectionFactory::OnTcpConnect(
    TcpConnectRequest request,
    int32_t net_result,
    const std::optional<net::IPEndPoint>& local_address,
    const std::optional<net::IPEndPoint>& remote_address,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  // We only care about net_result, since local_address doesn't matter,
  // remote_address should be 1 out of 1 addresses provided in the connect
  // call, and the streams must be closed before upgrading is allowed.
  if (net_result != net::OK) {
    client_->OnConnectionFailed(this, request.remote_address);
    return;
  }

  net::HostPortPair host_port_pair = net::HostPortPair::FromIPEndPoint(
      openscreen_platform::ToNetEndPoint(request.remote_address));
  network::mojom::TLSClientSocketOptionsPtr options =
      network::mojom::TLSClientSocketOptions::New();
  options->unsafely_skip_cert_verification =
      request.options.unsafely_skip_certificate_validation;

  openscreen::IPEndpoint local_endpoint{};
  if (local_address) {
    local_endpoint =
        openscreen_platform::ToOpenScreenEndPoint(local_address.value());
  }

  TlsUpgradeRequest upgrade_request(
      std::move(local_endpoint), std::move(request.remote_address),
      std::move(request.tcp_socket),
      mojo::Remote<network::mojom::TLSClientSocket>{});

  network::mojom::TCPConnectedSocket* tcp_socket =
      upgrade_request.tcp_socket.get();
  mojo::PendingReceiver<network::mojom::TLSClientSocket> tls_receiver =
      upgrade_request.tls_socket.BindNewPipeAndPassReceiver();

  tcp_socket->UpgradeToTLS(
      host_port_pair, std::move(options),
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(tls_receiver), mojo::NullRemote() /* observer */,
      base::BindOnce(&TlsConnectionFactory::OnTlsUpgrade,
                     weak_factory_.GetWeakPtr(), std::move(upgrade_request)));
}

void TlsConnectionFactory::OnTlsUpgrade(
    TlsUpgradeRequest request,
    int32_t net_result,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream,
    const std::optional<net::SSLInfo>& ssl_info) {
  if (net_result != net::OK) {
    client_->OnConnectionFailed(this, request.remote_address);
    return;
  }

  auto tls_connection = std::make_unique<TlsClientConnection>(
      request.local_address, request.remote_address, std::move(receive_stream),
      std::move(send_stream), std::move(request.tcp_socket),
      std::move(request.tls_socket));

  CRYPTO_BUFFER* der_buffer = ssl_info.value().unverified_cert->cert_buffer();
  const uint8_t* data = CRYPTO_BUFFER_data(der_buffer);
  std::vector<uint8_t> der_x509_certificate(
      data, data + CRYPTO_BUFFER_len(der_buffer));
  client_->OnConnected(this, std::move(der_x509_certificate),
                       std::move(tls_connection));
}

}  // namespace openscreen_platform
