// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_

#include "base/synchronization/waitable_event.h"
#include "chromeos/ash/services/nearby/public/mojom/firewall_hole.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "net/socket/tcp_server_socket.h"
#include "third_party/nearby/src/internal/platform/exception.h"
#include "third_party/nearby/src/internal/platform/implementation/wifi_direct.h"

namespace nearby::chrome {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class WifiDirectServerSocketError {
  kSocketClosed = 0,
  kFirewallHoleDisconnected = 1,
  kSocketFailure = 2,
  kMaxValue = kSocketFailure,
};

class WifiDirectServerSocket : public api::WifiDirectServerSocket {
 public:
  explicit WifiDirectServerSocket(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      mojo::PlatformHandle handle,
      mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole,
      std::unique_ptr<net::TCPServerSocket> tcp_server_socket);
  WifiDirectServerSocket(const WifiDirectServerSocket&) = delete;
  WifiDirectServerSocket& operator=(const WifiDirectServerSocket&) = delete;
  ~WifiDirectServerSocket() override;

  // api::WifiDirectServerSocket
  std::string GetIPAddress() const override;
  int GetPort() const override;
  std::unique_ptr<api::WifiDirectSocket> Accept() override;
  Exception Close() override;

 private:
  void DoAccept(net::IPEndPoint* accepted_address,
                std::unique_ptr<net::StreamSocket>* accepted_socket,
                bool* did_succeed);
  void OnAccept(bool* did_succeed, int result);
  void CloseSocket(base::WaitableEvent* close_waitable_event);
  void OnFirewallHoleDisconnect();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::PlatformHandle handle_;
  mojo::SharedRemote<::sharing::mojom::FirewallHole> firewall_hole_;
  base::WaitableEvent pending_accept_event_;
  std::unique_ptr<net::TCPServerSocket> tcp_server_socket_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_WIFI_DIRECT_SERVER_SOCKET_H_
