// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_lan_medium.h"

#include "base/bind.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/services/nearby/public/cpp/tcp_server_socket_port.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "net/base/net_errors.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace location {
namespace nearby {
namespace chrome {

namespace {

// The max time spent trying to connect to another device's TCP socket. We
// expect connection attempts to fail in practice, for example, when two devices
// are on different networks. We want to fail quickly so another upgrade medium
// like WebRTC can be used. The default networking stack timeout is too long; it
// can take over 2 minutes.
constexpr base::TimeDelta kConnectTimeout = base::Seconds(2);

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
    const mojo::SharedRemote<sharing::mojom::TcpSocketFactory>& socket_factory,
    const mojo::SharedRemote<
        chromeos::network_config::mojom::CrosNetworkConfig>&
        cros_network_config,
    const mojo::SharedRemote<sharing::mojom::FirewallHoleFactory>&
        firewall_hole_factory)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      socket_factory_(socket_factory),
      cros_network_config_(cros_network_config),
      firewall_hole_factory_(firewall_hole_factory) {
  // NOTE: We do not set the disconnect handler for the SharedRemotes here. They
  // are fundamental dependencies of the Nearby Connections process, which will
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

bool WifiLanMedium::IsNetworkConnected() const {
  // This is not used by ChromeOS. A virtual function was created in the base
  // class to support Windows, so this implementation only exists to override
  // that function. This may be implemented correctly at a later date if
  // Wi-Fi LAN would need similar functionality.
  // Context: cl/452402734
  return true;
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

  bool success = connected_socket_parameters.has_value();
  if (!success) {
    LOG(WARNING) << "WifiLanMedium::" << __func__ << ": Failed to connect to "
                 << address_list.back().ToString();
    return nullptr;
  }

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
  socket_factory_->CreateTCPConnectedSocket(
      /*timeout=*/kConnectTimeout,
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

  if (result != net::OK) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create TCP connected socket. result="
                 << net::ErrorToString(result);
    FinishConnectAttempt(connect_waitable_event,
                         ConnectResult::kErrorFailedToCreateTcpSocket);
    return;
  }

  DCHECK(tcp_connected_socket);
  DCHECK(local_addr);
  DCHECK(peer_addr);
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Created TCP connected socket. local_addr="
          << local_addr->ToString() << ", peer_addr=" << peer_addr->ToString();
  *connected_socket_parameters = {std::move(tcp_connected_socket),
                                  std::move(receive_stream),
                                  std::move(send_stream)};

  FinishConnectAttempt(connect_waitable_event, ConnectResult::kSuccess);
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

  // TcpServerSocketPort enforces any necessary restrictions on port number
  // ranges. If |port| is 0, choose a random port from the acceptable range.
  absl::optional<ash::nearby::TcpServerSocketPort> tcp_port =
      port == 0 ? absl::make_optional<ash::nearby::TcpServerSocketPort>(
                      ash::nearby::TcpServerSocketPort::Random())
                : ash::nearby::TcpServerSocketPort::FromInt(port);
  if (!tcp_port) {
    LOG(WARNING)
        << "WifiLanMedium::" << __func__
        << ": Failed to construct a TcpServerSocketPort from port number "
        << port;
    FinishListenAttempt(listen_waitable_event, ListenResult::kErrorInvalidPort);
    return;
  }

  // Local IP fetching: Step 1) Grab the first (i.e., default) active network.
  cros_network_config_->GetNetworkStateList(
      chromeos::network_config::mojom::NetworkFilter::New(
          chromeos::network_config::mojom::FilterType::kActive,
          chromeos::network_config::mojom::NetworkType::kAll,
          /*limit=*/1),
      base::BindOnce(&WifiLanMedium::OnGetNetworkStateList,
                     base::Unretained(this), server_socket_parameters,
                     listen_waitable_event, *tcp_port));
}

void WifiLanMedium::OnGetNetworkStateList(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    const ash::nearby::TcpServerSocketPort& port,
    std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
        result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (result.empty()) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to get network state list. Features will likely "
                 << "check for an active network connection before deciding to "
                 << "use WifiLan as a medium. If that is the case, this "
                 << "failure is unexpected.";
    FinishListenAttempt(listen_waitable_event,
                        ListenResult::kErrorFetchIpFailedToGetNetworkStateList);
    return;
  }

  // Local IP fetching: Step 2) Look up the network properties by GUID.
  cros_network_config_->GetManagedProperties(
      result[0]->guid,
      base::BindOnce(&WifiLanMedium::OnGetNetworkProperties,
                     base::Unretained(this), server_socket_parameters,
                     listen_waitable_event, port));
}

void WifiLanMedium::OnGetNetworkProperties(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    const ash::nearby::TcpServerSocketPort& port,
    chromeos::network_config::mojom::ManagedPropertiesPtr properties) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!properties) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to get network properties.";
    FinishListenAttempt(
        listen_waitable_event,
        ListenResult::kErrorFetchIpFailedToGetManagedProperties);
    return;
  }

  if (!properties->ip_configs || properties->ip_configs->empty()) {
    LOG(WARNING) << "WifiLanMedium::" << __func__ << ": IP configs are empty.";
    FinishListenAttempt(listen_waitable_event,
                        ListenResult::kErrorFetchIpMissingIpConfigs);
    return;
  }

  // Local IP fetching: Step 3) Take the first valid IPv4 address.
  absl::optional<net::IPAddress> ip_address;
  for (const auto& ip_config : *properties->ip_configs) {
    if (!ip_config->ip_address)
      continue;

    net::IPAddress ip;
    if (!ip.AssignFromIPLiteral(*ip_config->ip_address))
      continue;

    if (!ip.IsValid() || !ip.IsIPv4() || ip.IsLoopback() || ip.IsZero())
      continue;

    ip_address = ip;
    break;
  }

  if (!ip_address) {
    LOG(WARNING)
        << "WifiLanMedium::" << __func__
        << ": Failed to get local IPv4 address. This is likely unexpected "
        << "unless an IPv6-only network is used, for instance.";
    FinishListenAttempt(listen_waitable_event,
                        ListenResult::kErrorFetchIpNoValidLocalIpAddress);
    return;
  }

  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Fetched local IP address. ip_address="
          << ip_address->ToString();
  mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket;
  auto receiver = tcp_server_socket.InitWithNewPipeAndPassReceiver();
  socket_factory_->CreateTCPServerSocket(
      *ip_address, port, kBacklog,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(receiver),
      base::BindOnce(&WifiLanMedium::OnTcpServerSocketCreated,
                     base::Unretained(this), server_socket_parameters,
                     listen_waitable_event, std::move(tcp_server_socket),
                     *ip_address, port));
}

void WifiLanMedium::OnTcpServerSocketCreated(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
    const net::IPAddress& ip_address,
    const ash::nearby::TcpServerSocketPort& port,
    int32_t result,
    const absl::optional<net::IPEndPoint>& local_addr) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (result != net::OK) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create TCP server socket. result="
                 << net::ErrorToString(result);
    FinishListenAttempt(listen_waitable_event,
                        ListenResult::kErrorFailedToCreateTcpServerSocket);
    return;
  }

  DCHECK(tcp_server_socket);
  DCHECK(local_addr);
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Created TCP server socket. local_addr="
          << local_addr->ToString();

  if (local_addr->address() != ip_address ||
      local_addr->port() != port.port()) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": TCP server socket's IP:port disagrees with "
                    "input values. in="
                 << ip_address.ToString() << ":" << port.port()
                 << ", out=" << local_addr->ToString();
    FinishListenAttempt(
        listen_waitable_event,
        ListenResult::kErrorUnexpectedTcpServerSocketIpEndpoint);
    return;
  }

  firewall_hole_factory_->OpenFirewallHole(
      port, base::BindOnce(&WifiLanMedium::OnFirewallHoleCreated,
                           base::Unretained(this), server_socket_parameters,
                           listen_waitable_event, std::move(tcp_server_socket),
                           *local_addr));
}

void WifiLanMedium::OnFirewallHoleCreated(
    absl::optional<WifiLanServerSocket::ServerSocketParameters>*
        server_socket_parameters,
    base::WaitableEvent* listen_waitable_event,
    mojo::PendingRemote<network::mojom::TCPServerSocket> tcp_server_socket,
    const net::IPEndPoint& local_addr,
    mojo::PendingRemote<sharing::mojom::FirewallHole> firewall_hole) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (!firewall_hole) {
    LOG(WARNING) << "WifiLanMedium::" << __func__
                 << ": Failed to create firewall hole. local_addr="
                 << local_addr.ToString();
    FinishListenAttempt(listen_waitable_event,
                        ListenResult::kErrorFailedToCreateFirewallHole);
    return;
  }

  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Created firewall hole. local_addr=" << local_addr.ToString();
  *server_socket_parameters = {local_addr, std::move(tcp_server_socket),
                               std::move(firewall_hole)};

  FinishListenAttempt(listen_waitable_event, ListenResult::kSuccess);
}
/*============================================================================*/
// End: ListenForService()
/*============================================================================*/

absl::optional<std::pair<std::int32_t, std::int32_t>>
WifiLanMedium::GetDynamicPortRange() {
  return std::pair<std::int32_t, std::int32_t>(
      ash::nearby::TcpServerSocketPort::kMin,
      ash::nearby::TcpServerSocketPort::kMax);
}

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
/*============================================================================*/
// End: Not implemented
/*============================================================================*/

void WifiLanMedium::FinishConnectAttempt(base::WaitableEvent* event,
                                         ConnectResult result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = pending_connect_waitable_events_.find(event);
  if (it == pending_connect_waitable_events_.end())
    return;

  base::UmaHistogramEnumeration("Nearby.Connections.WifiLan.ConnectResult",
                                result);

  base::WaitableEvent* event_copy = *it;
  pending_connect_waitable_events_.erase(it);
  event_copy->Signal();
}

void WifiLanMedium::FinishListenAttempt(base::WaitableEvent* event,
                                        ListenResult result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  auto it = pending_listen_waitable_events_.find(event);
  if (it == pending_listen_waitable_events_.end())
    return;

  base::UmaHistogramEnumeration("Nearby.Connections.WifiLan.ListenResult",
                                result);

  base::WaitableEvent* event_copy = *it;
  pending_listen_waitable_events_.erase(it);
  event_copy->Signal();
}

void WifiLanMedium::Shutdown(base::WaitableEvent* shutdown_waitable_event) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // Note that resetting the Remote will cancel any pending callbacks, including
  // those already in the task queue.
  VLOG(1) << "WifiLanMedium::" << __func__
          << ": Closing TcpSocketFactory, CrosNetworkConfig, and "
             "FirewallHoleFactory mojo SharedRemotes.";
  socket_factory_.reset();
  cros_network_config_.reset();
  firewall_hole_factory_.reset();

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
    FinishConnectAttempt(event, ConnectResult::kCanceled);
  }
  if (!pending_listen_waitable_events_.empty()) {
    VLOG(1) << "WifiLanMedium::" << __func__ << ": Canceling "
            << pending_listen_waitable_events_.size()
            << " pending ListenForService() calls.";
  }
  auto pending_listen_waitable_events_copy = pending_listen_waitable_events_;
  for (base::WaitableEvent* event : pending_listen_waitable_events_copy) {
    FinishListenAttempt(event, ListenResult::kCanceled);
  }

  shutdown_waitable_event->Signal();
}

}  // namespace chrome
}  // namespace nearby
}  // namespace location
