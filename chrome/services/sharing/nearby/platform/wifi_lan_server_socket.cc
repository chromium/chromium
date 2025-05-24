// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_server_socket.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"

namespace nearby::chrome {

WifiLanServerSocket::ServerSocketParameters::ServerSocketParameters(
    const net::IPEndPoint& local_end_point,
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
    mojo::PendingRemote<::sharing::mojom::FirewallHole> firewall_hole)
    : local_end_point(local_end_point),
      tcp_server_socket(std::move(tcp_server_socket)),
      firewall_hole(std::move(firewall_hole)) {}

WifiLanServerSocket::ServerSocketParameters::~ServerSocketParameters() =
    default;

WifiLanServerSocket::ServerSocketParameters::ServerSocketParameters(
    ServerSocketParameters&&) = default;

WifiLanServerSocket::ServerSocketParameters&
WifiLanServerSocket::ServerSocketParameters::operator=(
    ServerSocketParameters&&) = default;

WifiLanServerSocket::WifiLanServerSocket(
    ServerSocketParameters server_socket_parameters)
    : local_end_point_(server_socket_parameters.local_end_point),
      task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      tcp_server_socket_(std::move(server_socket_parameters.tcp_server_socket),
                         task_runner_),
      firewall_hole_(std::move(server_socket_parameters.firewall_hole),
                     task_runner_) {
  tcp_server_socket_.set_disconnect_handler(
      base::BindOnce(&WifiLanServerSocket::OnTcpServerSocketDisconnected,
                     base::Unretained(this)),
      task_runner_);
  firewall_hole_.set_disconnect_handler(
      base::BindOnce(&WifiLanServerSocket::OnFirewallHoleDisconnected,
                     base::Unretained(this)),
      task_runner_);
}

WifiLanServerSocket::~WifiLanServerSocket() {
  Close();
}

std::string WifiLanServerSocket::GetIPAddress() const {
  const net::IPAddressBytes& bytes = local_end_point_.address().bytes();
  return std::string(bytes.begin(), bytes.end());
}

int WifiLanServerSocket::GetPort() const {
  return local_end_point_.port();
}

/*============================================================================*/
// Begin: Accept()
/*============================================================================*/
std::unique_ptr<api::WifiLanSocket> WifiLanServerSocket::Accept() {
  // To accommodate the synchronous Accept() signature, block until we accept an
  // incoming connection request and create a connected socket.
  base::WaitableEvent accept_waitable_event;

  // Because the WifiLanSocket constructor blocks, we cannot directly create the
  // WifiLanSocket in OnAccepted() while the WaitableEvent from Accept() is
  // still waiting; this would trigger a thread-restriction assert. Instead we
  // populate ConnectedSocketParameters in OnAccepted() and construct the
  // WifiLanSocket after the WaitableEvent is signaled.
  std::optional<WifiLanSocket::ConnectedSocketParameters>
      connected_socket_parameters;

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiLanServerSocket::DoAccept, base::Unretained(this),
                     &connected_socket_parameters, &accept_waitable_event));
  accept_waitable_event.Wait();

  return connected_socket_parameters.has_value()
             ? std::make_unique<WifiLanSocket>(
                   std::move(*connected_socket_parameters))
             : nullptr;
}

void WifiLanServerSocket::DoAccept(
    std::optional<WifiLanSocket::ConnectedSocketParameters>*
        connected_socket_parameters,
    base::WaitableEvent* accept_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_accept_waitable_events_.insert(accept_waitable_event);

  if (IsClosed()) {
    LOG(WARNING) << "WifiLanServerSocket::" << __func__
                 << ": Cannot accept; server socket already closed.";
    FinishAcceptAttempt(accept_waitable_event);
    return;
  }

  VLOG(1) << "WifiLanServerSocket::" << __func__
          << ": Start accepting incoming connections.";
  tcp_server_socket_->Accept(
      /*observer=*/mojo::NullRemote(),
      base::BindOnce(&WifiLanServerSocket::OnAccepted, base::Unretained(this),
                     connected_socket_parameters, accept_waitable_event));
}

void WifiLanServerSocket::OnAccepted(
    std::optional<WifiLanSocket::ConnectedSocketParameters>*
        connected_socket_parameters,
    base::WaitableEvent* accept_waitable_event,
    int32_t net_error,
    const std::optional<net::IPEndPoint>& remote_addr,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket> connected_socket,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  base::UmaHistogramSparse("Nearby.Connections.WifiLan.Socket.AcceptResult",
                           -net_error);

  if (net_error == net::OK) {
    DCHECK(remote_addr);
    VLOG(1) << "WifiLanServerSocket::" << __func__
            << ": Connection accepted from " << remote_addr->ToString();
    *connected_socket_parameters = {std::move(connected_socket),
                                    std::move(receive_stream),
                                    std::move(send_stream)};
  } else {
    LOG(WARNING) << "WifiLanServerSocket::" << __func__
                 << ": Failed to accept incoming connection. net_error="
                 << net::ErrorToString(net_error);
  }

  FinishAcceptAttempt(accept_waitable_event);
}
/*============================================================================*/
// End: Accept()
/*============================================================================*/

/*============================================================================*/
// Begin: Close()
/*============================================================================*/
Exception WifiLanServerSocket::Close() {
  // For thread safety, close on the |task_runner_|.
  if (task_runner_->RunsTasksInCurrentSequence()) {
    // No need to post a task if this is already running on |task_runner_|.
    DoClose(/*close_waitable_event=*/nullptr);
  } else {
    base::WaitableEvent close_waitable_event;
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WifiLanServerSocket::DoClose, base::Unretained(this),
                       &close_waitable_event));
    close_waitable_event.Wait();
  }

  return {Exception::kSuccess};
}

void WifiLanServerSocket::DoClose(base::WaitableEvent* close_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!IsClosed()) {
    // Note that resetting the Remote will cancel any pending callbacks,
    // including those already in the task queue.
    VLOG(1) << "WifiLanServerSocket::" << __func__
            << ": Closing TCP server socket and firewall hole.";
    DCHECK(tcp_server_socket_);
    tcp_server_socket_.reset();
    DCHECK(firewall_hole_);
    firewall_hole_.reset();

    // Cancel all pending Accept() calls. This is thread safe because all
    // changes to |pending_accept_waitable_events_| are sequenced. Make a copy
    // of the events because elements will be removed from
    // |pending_accept_waitable_events_| during iteration.
    if (!pending_accept_waitable_events_.empty()) {
      VLOG(1) << "WifiLanServerSocket::" << __func__ << ": Canceling "
              << pending_accept_waitable_events_.size()
              << " pending Accept() calls.";
    }
    auto pending_accept_waitable_events_copy = pending_accept_waitable_events_;
    for (base::WaitableEvent* event : pending_accept_waitable_events_copy) {
      FinishAcceptAttempt(event);
    }
  }

  if (close_waitable_event)
    close_waitable_event->Signal();
}
/*============================================================================*/
// End: Close()
/*============================================================================*/

bool WifiLanServerSocket::IsClosed() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(tcp_server_socket_.is_bound(), firewall_hole_.is_bound());
  return !tcp_server_socket_.is_bound();
}

void WifiLanServerSocket::FinishAcceptAttempt(base::WaitableEvent* event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = pending_accept_waitable_events_.find(event);
  if (it == pending_accept_waitable_events_.end())
    return;

  base::WaitableEvent* event_copy = *it;
  pending_accept_waitable_events_.erase(it);
  event_copy->Signal();
}

void WifiLanServerSocket::OnTcpServerSocketDisconnected() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  LOG(WARNING) << "WifiLanServerSocket::" << __func__
               << ": TCP server socket unexpectedly disconnected. Closing "
               << "WifiLanServerSocket.";
  Close();
}

void WifiLanServerSocket::OnFirewallHoleDisconnected() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  LOG(WARNING) << "WifiLanServerSocket::" << __func__
               << ": Firewall hole unexpectedly disconnected. Closing "
               << "WifiLanServerSocket.";
  Close();
}

}  // namespace nearby::chrome
