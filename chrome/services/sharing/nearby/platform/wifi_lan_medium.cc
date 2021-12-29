// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_medium.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

// The max size of the server socket's queue of pending connection requests from
// remote sockets. Any additional connection requests are refused.
constexpr uint32_t kBacklog = 10;

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("nearby_connections_wifi_lan", R"(
        semantics {
          sender: "Nearby Connections"
          description:
            "Nearby Connections provides device-to-device communication "
            "mediums such as WLAN, which is relevant here. Both devices open "
            "TCP sockets, connect to each other, and establish an encrypted "
            "and authenticated communication channel. Both devices must be "
            "using the same router or possibly access point."
          trigger:
            "User's Chromebook uses a feature such as Nearby Share or "
            "Phone Hub that leverages Nearby Connections to establish a "
            "communication channel between devices."
          data:
            "After the WLAN connection between devices is established, "
            "encrypted, and authenticated, feature-specific bytes are "
            "transferred. For example, Nearby Share might send/receive files "
            "and Phone Hub might receive message notification data from the "
            "phone."
          destination: OTHER
          destination_other:
            "Another device such as a Chromebook or Android phone."
        }
        policy {
          cookies_allowed: NO
          setting:
            "Features that use WLAN such as Nearby Share and Phone Hub can be "
            "enabled/disabled in Chromebook settings."
          policy_exception_justification:
            "The individual features that leverage Nearby Connections have "
            "their own policies associated with them."
        })");

}  // namespace

WifiLanMedium::WifiLanMedium(
    const mojo::SharedRemote<network::mojom::NetworkContext>& network_context)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      network_context_(network_context) {
  // NOTE: We do not set the disconnect handler for |network_context_| here. It
  // is a fundamental dependency of the Nearby Connections process, which will
  // crash if any dependency disconnects.
}

WifiLanMedium::~WifiLanMedium() {
  // For thread safety, shut down on the |task_runner_|.
  base::WaitableEvent shutdown_waitable_event;
  task_runner_->PostTask(FROM_HERE, base::BindOnce(&WifiLanMedium::Shutdown,
                                                   base::Unretained(this),
                                                   &shutdown_waitable_event));
  shutdown_waitable_event.Wait();
}

/*============================================================================*/
// Begin: ConnectToService()
/*============================================================================*/
std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const NsdServiceInfo& remote_service_info,
    CancellationFlag* cancellation_flag) {
  return ConnectToService(remote_service_info.GetIPAddress(),
                          remote_service_info.GetPort(), cancellation_flag);
}

std::unique_ptr<api::WifiLanSocket> WifiLanMedium::ConnectToService(
    const std::string& ip_address,
    int port,
    CancellationFlag* cancellation_flag) {
  // TODO(https://crbug.com/1261238): Possibly utilize cancellation_flag.

  net::IPAddress ip(reinterpret_cast<const uint8_t*>(ip_address.data()),
                    ip_address.length());
  const net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip, port);

  // To accommodate the synchronous ConnectToService() signature, block until we
  // connect to the remote TCP server socket or fail.
  base::WaitableEvent connect_waitable_event;
  absl::optional<WifiLanSocket::ConnectedSocketParameters>
      connected_socket_parameters;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiLanMedium::DoConnect, base::Unretained(this),
                     address_list, &connected_socket_parameters,
                     &connect_waitable_event));
  connect_waitable_event.Wait();

  // TODO(https://crbug.com/1261238): Log metric.
  bool success = connected_socket_parameters.has_value();
  if (!success) {
    LOG(WARNING) << "WifiLanMedium::" << __func__ << ": Failed to connect to "
                 << address_list.back().ToString();
    return nullptr;
  }

  VLOG(1) << "WifiLanMedium::" << __func__ << ": Connection established with "
          << connected_socket_parameters->remote_end_point.ToString();
  return std::make_unique<WifiLanSocket>(
      std::move(*connected_socket_parameters));
}

void WifiLanMedium::DoConnect(
    const net::AddressList& address_list,
    absl::optional<WifiLanSocket::ConnectedSocketParameters>*
        connected_socket_parameters,
    base::WaitableEvent* connect_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  pending_connect_waitable_events_.insert(connect_waitable_event);

  VLOG(1) << "WifiLanMedium::" << __func__ << ": Attempting to connect to "
          << address_list.back().ToString();
  mojo::PendingRemote<network::mojom::TCPConnectedSocket> tcp_connected_socket;
  mojo::PendingReceiver<network::mojom::TCPConnectedSocket> receiver =
      tcp_connected_socket.InitWithNewPipeAndPassReceiver();
  network_context_->CreateTCPConnectedSocket(
      /*local_addr=*/absl::nullopt, address_list,
      /*tcp_connected_socket_options=*/nullptr,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(receiver), /*observer=*/mojo::NullRemote(),
      base::BindOnce(&WifiLanMedium::OnConnect, base::Unretained(this),
                     connected_socket_parameters, connect_waitable_event,
                     std::move(tcp_connected_socket)));
}

void WifiLanMedium::OnConnect(
    absl::optional<WifiLanSocket::ConnectedSocketParameters>*
        connected_socket_parameters,
    base::WaitableEvent* connect_waitable_event,
    mojo::PendingRemote<network::mojom::TCPConnectedSocket>
        tcp_connected_socket,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr,
    const absl::optional<net::IPEndPoint>& peer_addr,
    mojo::ScopedDataPipeConsumerHandle receive_stream,
    mojo::ScopedDataPipeProducerHandle send_stream) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(https://crbug.com/1261238): Log metric.
  if (result != net::OK) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create TCP connected socket. result="
                 << net::ErrorToString(result);
    FinishConnectAttempt(connect_waitable_event);
    return;
  }

  DCHECK(tcp_connected_socket);
  DCHECK(local_addr);
  DCHECK(peer_addr);
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Created TCP connected socket. local_addr="
          << local_addr->ToString() << ", peer_addr=" << peer_addr->ToString();
  *connected_socket_parameters = {*peer_addr, std::move(tcp_connected_socket),
                                  std::move(receive_stream),
                                  std::move(send_stream)};

  FinishConnectAttempt(connect_waitable_event);
}
/*============================================================================*/
// End: ConnectToService()
/*============================================================================*/

/*============================================================================*/
// Begin: ListenForService()
/*============================================================================*/
std::unique_ptr<api::WifiLanServerSocket> WifiLanMedium::ListenForService(
    int port) {
  // To accommodate the synchronous ListenForService() signature, block until we
  // create a server socket and start listening for connections or fail.
  base::WaitableEvent listen_waitable_event;
  absl::optional<WifiLanServerSocket::ServerSocketParameters>
      server_socket_parameters;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiLanMedium::DoListenForService, base::Unretained(this),
                     &server_socket_parameters, &listen_waitable_event, port));
  listen_waitable_event.Wait();

  // TODO(https://crbug.com/1261238): Log metric.
  bool success = server_socket_parameters.has_value();
  if (!success) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create server socket on port " << port;
    return nullptr;
  }

  VLOG(1) << "WifiLanMedium::" << __func__ << ": Server socket created on "
          << server_socket_parameters->local_end_point.ToString();
  return std::make_unique<WifiLanServerSocket>(
      std::move(*server_socket_parameters));
}

void WifiLanMedium::DoListenForService(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    int port) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  pending_listen_waitable_events_.insert(listen_waitable_event);

  // TODO(https://crbug.com/1261238): Implement local IP address fetching. We
  // temporarily use this hardcoding for unit tests.
  net::IPAddress address(192, 168, 86, 75);
  OnLocalIpAddressFetched(server_socket_parameters, listen_waitable_event,
                          net::IPEndPoint(address, port));
}

void WifiLanMedium::OnLocalIpAddressFetched(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    const net::IPEndPoint& local_end_point) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(https://crbug.com/1261238): Process fetched local IP address and log
  // metric.

  mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket;
  auto receiver = tcp_server_socket.InitWithNewPipeAndPassReceiver();
  network_context_->CreateTCPServerSocket(
      local_end_point, kBacklog,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(receiver),
      base::BindOnce(&WifiLanMedium::OnTcpServerSocketCreated,
                     base::Unretained(this), server_socket_parameters,
                     listen_waitable_event, std::move(tcp_server_socket)));
}

void WifiLanMedium::OnTcpServerSocketCreated(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(https://crbug.com/1261238): Log metric.
  if (result != net::OK) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create TCP server socket. result="
                 << net::ErrorToString(result);
    FinishListenAttempt(listen_waitable_event);
    return;
  }

  // TODO(https://crbug.com/1261238): Open firewall hole.
  DCHECK(tcp_server_socket);
  DCHECK(local_addr);
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Created TCP server socket. local_addr="
          << local_addr->ToString();
  OnFirewallHoleCreated(server_socket_parameters, listen_waitable_event,
                        std::move(tcp_server_socket), local_addr);
}

void WifiLanMedium::OnFirewallHoleCreated(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
    const absl::optional<net::IPEndPoint>& local_addr) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // TODO(https://crbug.com/1261238): Process firewall hole, log metric, and add
  // firewall hole to server socket parameters.

  *server_socket_parameters = {*local_addr, std::move(tcp_server_socket)};

  FinishListenAttempt(listen_waitable_event);
}
/*============================================================================*/
// End: ListenForService()
/*============================================================================*/

/*============================================================================*/
// Begin: Not implemented
/*============================================================================*/
bool WifiLanMedium::StartAdvertising(const NsdServiceInfo& nsd_service_info) {
  NOTIMPLEMENTED();
  return false;
}
bool WifiLanMedium::StopAdvertising(const NsdServiceInfo& nsd_service_info) {
  NOTIMPLEMENTED();
  return false;
}
bool WifiLanMedium::StartDiscovery(const std::string& service_type,
                                   DiscoveredServiceCallback callback) {
  NOTIMPLEMENTED();
  return false;
}
bool WifiLanMedium::StopDiscovery(const std::string& service_type) {
  NOTIMPLEMENTED();
  return false;
}
absl::optional<std::pair<std::int32_t, std::int32_t>>
WifiLanMedium::GetDynamicPortRange() {
  NOTIMPLEMENTED();
  return absl::nullopt;
}
/*============================================================================*/
// End: Not implemented
/*============================================================================*/

void WifiLanMedium::FinishConnectAttempt(base::WaitableEvent* event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = pending_connect_waitable_events_.find(event);
  if (it == pending_connect_waitable_events_.end())
    return;

  base::WaitableEvent* event_copy = *it;
  pending_connect_waitable_events_.erase(it);
  event_copy->Signal();
}

void WifiLanMedium::FinishListenAttempt(base::WaitableEvent* event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = pending_listen_waitable_events_.find(event);
  if (it == pending_listen_waitable_events_.end())
    return;

  base::WaitableEvent* event_copy = *it;
  pending_listen_waitable_events_.erase(it);
  event_copy->Signal();
}

void WifiLanMedium::Shutdown(base::WaitableEvent* shutdown_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Note that resetting the Remote will cancel any pending callbacks, including
  // those already in the task queue.
  // TODO(https://crbug.com/1261238): Reset firewall hole factory.
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Closing NetworkContext Remote.";
  network_context_.reset();

  // Cancel all pending connect/listen calls. This is thread safe because all
  // changes to the pending-event sets are sequenced. Make a copy of the events
  // because elements will be removed from the sets during iteration.
  if (!pending_connect_waitable_events_.empty()) {
    VLOG(1) << "WifiLanMedium::" << __func__ << ": Canceling "
            << pending_connect_waitable_events_.size()
            << " pending ConnectToService() calls.";
  }
  auto pending_connect_waitable_events_copy = pending_connect_waitable_events_;
  for (base::WaitableEvent* event : pending_connect_waitable_events_copy) {
    FinishConnectAttempt(event);
  }
  if (!pending_listen_waitable_events_.empty()) {
    VLOG(1) << "WifiLanMedium::" << __func__ << ": Canceling "
            << pending_listen_waitable_events_.size()
            << " pending ListenForService() calls.";
  }
  auto pending_listen_waitable_events_copy = pending_listen_waitable_events_;
  for (base::WaitableEvent* event : pending_listen_waitable_events_copy) {
    FinishListenAttempt(event);
  }

  shutdown_waitable_event->Signal();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
