// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SERVER_SOCKET_H_

#include <memory>
#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/services/sharing/nearby/platform/wifi_lan_socket.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/ip_endpoint.h"
#include "services/network/public/mojom/tcp_socket.mojom.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_lan.h"

namespace base {
class SequencedTaskRunner;
class WaitableEvent;
}  // namespace base

namespace nearby::chrome {

// An implementation of Nearby Connections's abstract class
// api::WifiLanServerSocket. This implementation wraps a TCPServerSocket and
// FirewallHole which live until Close() is called or the WifiLanServerSocket
// instance is destroyed.
//
// The methods Accept() and Close() are thread safe as is the destructor, which
// closes the socket if not closed already. Accept() and Close() are blocking.
// All pending Accept() calls are guaranteed to return after Close() is called
// or before destruction. All pending Close() calls are guaranteed to return
// before destruction.
class WifiLanServerSocket : public api::WifiLanServerSocket {
 public:
  // Parameters needed to construct a WifiLanServerSocket.
  struct ServerSocketParameters {
    ServerSocketParameters(
        const net::IPEndPoint& local_end_point,
        mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
        mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole);
    ~ServerSocketParameters();
    ServerSocketParameters(ServerSocketParameters&&);
    ServerSocketParameters& operator=(ServerSocketParameters&&);

    net::IPEndPoint local_end_point;
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket;
    mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole;
  };

  explicit WifiLanServerSocket(ServerSocketParameters server_socket_parameters);
  WifiLanServerSocket(const WifiLanServerSocket&) = delete;
  WifiLanServerSocket& operator=(const WifiLanServerSocket&) = delete;
  ~WifiLanServerSocket() override;

  // api::WifiLanServerSocket:
  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::WifiLanSocket> Accept() override;
  Exception Close() override;

 private:
  /*==========================================================================*/
  // Accept() helpers: Accept connection from remote TCP socket.
  /*==========================================================================*/
  void DoAccept(std::optional<WifiLanSocket::ConnectedSocketParameters>*
                    connected_socket_parameters,
                base::WaitableEvent* accept_waitable_event);
  void OnAccepted(
      std::optional<WifiLanSocket::ConnectedSocketParameters>*
          connected_socket_parameters,
      base::WaitableEvent* accept_waitable_event,
      int32_t net_result,
      const std::optional<net::IPEndPoint>& remote_addr,
      mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
      mojo::ScopedDataPipeConsumerHandle receive_stream,
      mojo::ScopedDataPipeProducerHandle send_stream);
  /*==========================================================================*/

  /*==========================================================================*/
  // Close() helpers: Close TCP server socket and cancel pending Accept() calls.
  /*==========================================================================*/
  void DoClose(base::WaitableEvent* close_waitable_event);
  /*==========================================================================*/

  // Returns true if |tcp_server_socket_| is already reset.
  bool IsClosed() const;

  // Removes |event| from |pending_accept_waitable_events_| and signals |event|.
  // Calls to this method are sequenced on |task_runner_| and thus thread safe.
  void FinishAcceptAttempt(base::WaitableEvent* event);

  // Close if the TCP server socket or firewall hole disconnect.
  void OnTcpServerSocketDisconnected();
  void OnFirewallHoleDisconnected();

  const net::IPEndPoint local_end_point_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Uniquely owned by the WifiLanServerSocket instance. The underlying
  // TCPServerSocket/FirewallHole implementation is destroyed when the
  // corresponding remote endpoint, |tcp_server_socket_|/|firewall_hole_|, is
  // destroyed.
  mojo::SharedRemote<network::mojom::TCPServerSocket> tcp_server_socket_;
  mojo::SharedRemote<::sharing::mojom::FirewallHole> firewall_hole_;

  // Track all pending accept tasks in case Close() is called while waiting.
  base::flat_set<raw_ptr<base::WaitableEvent, CtnExperimental>>
      pending_accept_waitable_events_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_LAN_SERVER_SOCKET_H_
