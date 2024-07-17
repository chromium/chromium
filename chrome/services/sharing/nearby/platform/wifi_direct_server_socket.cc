// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"

#include "base/metrics/histogram_functions.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"
#include "net/socket/tcp_client_socket.h"

namespace nearby::chrome {

WifiDirectServerSocket::WifiDirectServerSocket(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    mojo::PlatformHandle handle,
    mojo::PendingRemote<sharing::mojom::FirewallHole> firewall_hole,
    std::unique_ptr<net::TCPServerSocket> tcp_server_socket)
    : task_runner_(std::move(task_runner)),
      handle_(std::move(handle)),
      firewall_hole_(std::move(firewall_hole)),
      tcp_server_socket_(std::move(tcp_server_socket)) {
  CHECK(task_runner_);
  firewall_hole_.set_disconnect_handler(
      base::BindOnce(&WifiDirectServerSocket::OnFirewallHoleDisconnect,
                     base::Unretained(this)),
      task_runner_);
}

WifiDirectServerSocket::~WifiDirectServerSocket() {
  Close();
}

// api::WifiDirectServerSocket
std::string WifiDirectServerSocket::GetIPAddress() const {
  CHECK(tcp_server_socket_);
  net::IPEndPoint address;
  tcp_server_socket_->GetLocalAddress(&address);
  return address.address().ToString();
}

int WifiDirectServerSocket::GetPort() const {
  CHECK(tcp_server_socket_);
  net::IPEndPoint address;
  tcp_server_socket_->GetLocalAddress(&address);
  return address.port();
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectServerSocket::Accept() {
  // Ensure that the socket has not been closed.
  if (!tcp_server_socket_) {
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ServerSocket.Accept.Result", false);
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ServerSocket.Accept.Error",
        WifiDirectServerSocketError::kSocketClosed);
    return nullptr;
  }

  bool success = false;
  net::IPEndPoint accepted_address;
  std::unique_ptr<net::StreamSocket> accepted_socket;
  pending_accept_event_.Reset();
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectServerSocket::DoAccept, base::Unretained(this),
                     &accepted_address, &accepted_socket, &success));
  pending_accept_event_.Wait();

  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.ServerSocket.Accept.Result", success);
  if (!success) {
    return nullptr;
  }

  return std::make_unique<WifiDirectSocket>(task_runner_,
                                            std::move(accepted_socket));
}

Exception WifiDirectServerSocket::Close() {
  handle_.reset();

  if (!tcp_server_socket_) {
    return {Exception::kFailed};
  }

  // Cancel the pending `Accept` call, if it exists.
  pending_accept_event_.Signal();

  // Directly call `CloseSocket` if the current sequence is on the appriroiate
  // task runner.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    CloseSocket(nullptr);
    return {Exception::kSuccess};
  }

  // Cleanup the socket on the IO thread.
  base::WaitableEvent waitable_event;
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WifiDirectServerSocket::CloseSocket,
                                base::Unretained(this), &waitable_event));
  waitable_event.Wait();

  return {Exception::kSuccess};
}

void WifiDirectServerSocket::DoAccept(
    net::IPEndPoint* accepted_address,
    std::unique_ptr<net::StreamSocket>* accepted_socket,
    bool* did_succeed) {
  // Ensure that the Accept call has not been cancelled.
  if (pending_accept_event_.IsSignaled()) {
    return;
  }

  // We cannot accept connections if the firewall hole has disconnected.
  if (!firewall_hole_ || !firewall_hole_.is_bound()) {
    // The firewall hole disconnect handler will close the socket, which will in
    // turn trigger this callback. It is important to check the firewall hole
    // availability before checking the socket availability, otherwise this
    // error state will never trigger.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ServerSocket.Accept.Error",
        WifiDirectServerSocketError::kFirewallHoleDisconnected);
    pending_accept_event_.Signal();
    return;
  }

  // Ensure that the socket has not been closed.
  if (!tcp_server_socket_) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ServerSocket.Accept.Error",
        WifiDirectServerSocketError::kSocketClosed);
    pending_accept_event_.Signal();
    return;
  }

  int result = tcp_server_socket_->Accept(
      accepted_socket,
      base::BindOnce(&WifiDirectServerSocket::OnAccept, base::Unretained(this),
                     did_succeed),
      accepted_address);
  // In the case where there was not a pending connection request, this call
  // will return `ERR_IO_PENDING`. This means the provided callback will be
  // called with the result, when it occurs. In all other cases, we have the
  // result immediately and should manually propagate this to the callback.
  if (result != net::ERR_IO_PENDING) {
    OnAccept(did_succeed, result);
  }
}

void WifiDirectServerSocket::OnAccept(bool* did_succeed, int result) {
  // Ensure that the Accept call has not been cancelled.
  if (pending_accept_event_.IsSignaled()) {
    return;
  }

  *did_succeed = (result == net::OK);
  if (!did_succeed) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ServerSocket.Accept.Error",
        WifiDirectServerSocketError::kSocketFailure);
  }
  pending_accept_event_.Signal();
}

void WifiDirectServerSocket::CloseSocket(
    base::WaitableEvent* close_waitable_event) {
  tcp_server_socket_.reset();
  if (close_waitable_event) {
    close_waitable_event->Signal();
  }
}

void WifiDirectServerSocket::OnFirewallHoleDisconnect() {
  // Reset the remote to indicate that it has been disconnected.
  firewall_hole_.reset();

  // Close the socket so that Nearby Connections knows that there will not be
  // any new incoming connections.
  // Note: This is a callback that is bound to the provided `task_runner`, which
  // is the appropriate place to destroy the stored socket,
  CloseSocket(nullptr);
}

}  // namespace nearby::chrome
