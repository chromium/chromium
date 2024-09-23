// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include <netinet/in.h>

#include "base/files/scoped_file.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_socket.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_descriptor.h"

namespace nearby::chrome {

WifiDirectMedium::WifiDirectMedium(
    const mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>&
        wifi_direct_manager,
    const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
        firewall_hole_factory)
    : io_thread_(std::make_unique<base::Thread>("wifi-direct-medium")),
      wifi_direct_manager_(std::move(wifi_direct_manager)),
      firewall_hole_factory_(std::move(firewall_hole_factory)) {
  io_thread_->StartWithOptions(
      base::Thread::Options(base::MessagePumpType::IO, 0));
}

WifiDirectMedium::~WifiDirectMedium() = default;

bool WifiDirectMedium::IsInterfaceValid() const {
  bool is_interface_valid = false;
  base::WaitableEvent waitable_event;
  io_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectMedium::GetCapabilities, base::Unretained(this),
                     &is_interface_valid, &waitable_event));
  waitable_event.Wait();
  return is_interface_valid;
}

bool WifiDirectMedium::StartWifiDirect(WifiDirectCredentials* credentials) {
  // Wrap the async mojo call to make it sync.
  base::WaitableEvent waitable_event;
  io_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectMedium::CreateGroup, base::Unretained(this),
                     credentials, &waitable_event));
  waitable_event.Wait();

  // An active remote means the group has been created.
  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.CreateWifiDirectGroup.Result",
      !!connection_);
  return !!connection_;
}

bool WifiDirectMedium::StopWifiDirect() {
  if (connection_) {
    connection_.reset();
    return true;
  }
  return false;
}

bool WifiDirectMedium::ConnectWifiDirect(WifiDirectCredentials* credentials) {
  // Wrap the async mojo call to make it sync.
  base::WaitableEvent waitable_event;
  io_thread_->task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectMedium::ConnectGroup, base::Unretained(this),
                     credentials, &waitable_event));
  waitable_event.Wait();

  // An active remote means the group has been connected to.
  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.ConnectToWifiDirectGroup.Result",
      !!connection_);
  return !!connection_;
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  if (connection_) {
    connection_.reset();
    return true;
  }
  return false;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectMedium::ConnectToService(
    std::string_view ip_address,
    int port,
    CancellationFlag* cancellation_flag) {
  // Ensure that there is a valid WiFi Direct connection.
  if (!connection_) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kNoConnection);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  // Ensure the connection attempt hasn't been cancelled.
  if (cancellation_flag && cancellation_flag->Cancelled()) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kCancelled);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  mojo::PlatformHandle handle = mojo::PlatformHandle(base::ScopedFD(
      net::CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP)));
  if (!handle.is_valid()) {
    // Failed to get a socket file descriptor.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kFailedToCreatePlatformSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  bool did_associate = false;
  {
    base::WaitableEvent waitable_event;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&WifiDirectMedium::AssociateSocket,
                                  base::Unretained(this), &did_associate,
                                  &waitable_event, handle.Clone()));
    waitable_event.Wait();
  }

  if (!did_associate) {
    // Socket not associated at the platform layer.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kFailedToAssociateSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  // TODO(b/345572726): Unittest this specific cancellation scenario.
  if (cancellation_flag && cancellation_flag->Cancelled()) {
    // Cancelled during connection attempt.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kCancelled);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  // Listening on the socket needs to happen on an IO thread.
  std::unique_ptr<net::TCPClientSocket> socket = nullptr;
  {
    base::WaitableEvent waitable_event;
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&WifiDirectMedium::CreateAndConnectSocket,
                       base::Unretained(this), ip_address, port,
                       handle.GetFD().get(), &socket, &waitable_event));
    waitable_event.Wait();
  }
  if (!socket) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kFailedToConnectSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  // TODO(b/345572726): Unittest this specific cancellation scenario.
  if (cancellation_flag && cancellation_flag->Cancelled()) {
    // Cancelled during connection attempt.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToService.Error",
        WifiDirectServiceError::kCancelled);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ConnectToService.Result", false);
    return nullptr;
  }

  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.ConnectToService.Result", true);
  return std::make_unique<WifiDirectSocket>(
      std::move(handle), io_thread_->task_runner(), std::move(socket));
}

std::unique_ptr<api::WifiDirectServerSocket> WifiDirectMedium::ListenForService(
    int port) {
  // Ensure that there is a valid WiFi Direct connection.
  if (!connection_) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ListenForService.Error",
        WifiDirectServiceError::kNoConnection);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ListenForService.Result", false);
    return nullptr;
  }

  // Create server socket.
  auto fd = base::ScopedFD(
      net::CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (fd.get() < 0) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ListenForService.Error",
        WifiDirectServiceError::kFailedToCreatePlatformSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ListenForService.Result", false);
    return nullptr;
  }
  mojo::PlatformHandle handle = mojo::PlatformHandle(std::move(fd));

  // Wrap the async mojo call to make it sync.
  bool did_associate;
  {
    base::WaitableEvent waitable_event;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&WifiDirectMedium::AssociateSocket,
                                  base::Unretained(this), &did_associate,
                                  &waitable_event, handle.Clone()));
    waitable_event.Wait();
  }

  if (!did_associate) {
    // Socket not associated at the platform layer.
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ListenForService.Error",
        WifiDirectServiceError::kFailedToAssociateSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ListenForService.Result", false);
    return nullptr;
  }

  std::optional<ash::nearby::TcpServerSocketPort> tcp_port =
      ash::nearby::TcpServerSocketPort::FromInt(port);
  if (!tcp_port) {
    tcp_port = ash::nearby::TcpServerSocketPort::Random();
  }

  // Open a firewall hole.
  mojo::PendingRemote<sharing::mojom::FirewallHole> firewall_hole;
  {
    base::WaitableEvent waitable_event;
    io_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&WifiDirectMedium::OpenFirewallHole,
                                  base::Unretained(this), *tcp_port,
                                  &firewall_hole, &waitable_event));
    waitable_event.Wait();
  }

  if (!firewall_hole) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ListenForService.Error",
        WifiDirectServiceError::kFailedToOpenFirewallHole);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ListenForService.Result", false);
    return nullptr;
  }

  // Listening on the socket needs to happen on an IO thread.
  // Do this last so that the socket is immediately assigned; socket cleanup
  // must happen on the same sequence the it was created on.
  std::unique_ptr<net::TCPServerSocket> socket;
  {
    base::WaitableEvent waitable_event;
    io_thread_->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&WifiDirectMedium::CreateAndListenToSocket,
                       base::Unretained(this), tcp_port->port(),
                       handle.GetFD().get(), &socket, &waitable_event));
    waitable_event.Wait();
  }
  if (!socket) {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ListenForService.Error",
        WifiDirectServiceError::kFailedToListenToSocket);
    base::UmaHistogramBoolean(
        "Nearby.Connections.WifiDirect.ListenForService.Result", false);
    return nullptr;
  }

  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.ListenForService.Result", true);
  return std::make_unique<WifiDirectServerSocket>(
      io_thread_->task_runner(), std::move(handle), std::move(firewall_hole),
      std::move(socket));
}

std::optional<std::pair<std::int32_t, std::int32_t>>
WifiDirectMedium::GetDynamicPortRange() {
  NOTIMPLEMENTED();
  return std::nullopt;
}

void WifiDirectMedium::GetCapabilities(
    bool* is_capability_supported,
    base::WaitableEvent* waitable_event) const {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());
  wifi_direct_manager_->GetWifiP2PCapabilities(
      base::BindOnce(&WifiDirectMedium::OnCapabilities, base::Unretained(this),
                     is_capability_supported, waitable_event));
}

void WifiDirectMedium::OnCapabilities(
    bool* is_capability_supported,
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiP2PCapabilitiesPtr capabilities) const {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());
  CHECK(capabilities);
  *is_capability_supported = capabilities->is_p2p_supported;
  base::UmaHistogramBoolean("Nearby.Connections.WifiDirect.IsP2pSupported",
                            capabilities->is_p2p_supported);
  waitable_event->Signal();
}

void WifiDirectMedium::CreateGroup(WifiDirectCredentials* credentials,
                                   base::WaitableEvent* waitable_event) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());

  // This is currently validated in the Chrome connectivity layer, but totally
  // ignored at the platform level. Both SSID and password need to be valid for
  // this call to succeed.
  credentials->SetSSID("DIRECT-00");
  credentials->SetPassword("SecretPassword");

  auto credentials_ptr = ash::wifi_direct::mojom::WifiCredentials::New();
  credentials_ptr->ssid = credentials->GetSSID();
  credentials_ptr->passphrase = credentials->GetPassword();
  wifi_direct_manager_->CreateWifiDirectGroup(
      std::move(credentials_ptr),
      base::BindOnce(&WifiDirectMedium::OnGroupCreated, base::Unretained(this),
                     credentials, waitable_event));
}

void WifiDirectMedium::OnGroupCreated(
    WifiDirectCredentials* credentials,
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiDirectOperationResult result,
    mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectConnection>
        connection) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());

  if (result == ash::wifi_direct::mojom::WifiDirectOperationResult::kSuccess) {
    // Store the connection so that the group can be destroyed when the remote
    // is reset.
    connection_.Bind(std::move(connection), io_thread_->task_runner());
    connection_.set_disconnect_handler(
        base::BindOnce(&WifiDirectMedium::OnDisconnect, base::Unretained(this)),
        io_thread_->task_runner());

    // Fetch the IPv4 address from the connection.
    connection_->GetProperties(base::BindOnce(&WifiDirectMedium::OnProperties,
                                              base::Unretained(this),
                                              credentials, waitable_event));
    return;
  }

  base::UmaHistogramEnumeration(
      "Nearby.Connections.WifiDirect.CreateWifiDirectGroup.Error", result);
  // Trigger sync signal.
  waitable_event->Signal();
}

void WifiDirectMedium::OnProperties(
    WifiDirectCredentials* credentials,
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiDirectConnectionPropertiesPtr properties) {
  credentials->SetSSID(properties->credentials->ssid);
  credentials->SetPassword(properties->credentials->passphrase);
  credentials->SetIPAddress(properties->ipv4_address);
  credentials->SetGateway(properties->ipv4_address);
  ipv4_address_ = properties->ipv4_address;
  credentials->SetFrequency(properties->frequency);
  waitable_event->Signal();
}

void WifiDirectMedium::ConnectGroup(WifiDirectCredentials* credentials,
                                    base::WaitableEvent* waitable_event) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());

  auto credentials_ptr = ash::wifi_direct::mojom::WifiCredentials::New();
  credentials_ptr->ssid = credentials->GetSSID();
  credentials_ptr->passphrase = credentials->GetPassword();
  int frequency = credentials->GetFrequency();
  wifi_direct_manager_->ConnectToWifiDirectGroup(
      std::move(credentials_ptr),
      frequency > 0 ? std::optional(frequency) : std::nullopt,
      base::BindOnce(&WifiDirectMedium::OnGroupConnected,
                     base::Unretained(this), waitable_event));
}

void WifiDirectMedium::OnGroupConnected(
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiDirectOperationResult result,
    mojo::PendingRemote<ash::wifi_direct::mojom::WifiDirectConnection>
        connection) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());

  if (result == ash::wifi_direct::mojom::WifiDirectOperationResult::kSuccess) {
    // Store the connection so that the group can be destroyed when the remote
    // is reset.
    connection_.Bind(std::move(connection), io_thread_->task_runner());
    connection_.set_disconnect_handler(
        base::BindOnce(&WifiDirectMedium::OnDisconnect, base::Unretained(this)),
        io_thread_->task_runner());
  } else {
    base::UmaHistogramEnumeration(
        "Nearby.Connections.WifiDirect.ConnectToWifiDirectGroup.Error", result);
  }

  // Trigger sync signal.
  waitable_event->Signal();
}

void WifiDirectMedium::AssociateSocket(bool* did_associate,
                                       base::WaitableEvent* waitable_event,
                                       mojo::PlatformHandle socket_handle) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());
  CHECK(connection_.is_bound());
  connection_->AssociateSocket(
      std::move(socket_handle),
      base::BindOnce(&WifiDirectMedium::OnSocketAssociated,
                     base::Unretained(this), did_associate, waitable_event));
}

void WifiDirectMedium::OnSocketAssociated(bool* did_associate,
                                          base::WaitableEvent* waitable_event,
                                          bool success) {
  base::UmaHistogramBoolean(
      "Nearby.Connections.WifiDirect.AssociateSocket.Result", success);
  *did_associate = success;
  waitable_event->Signal();
}

void WifiDirectMedium::CreateAndListenToSocket(
    int16_t port,
    net::SocketDescriptor socket_descriptor,
    std::unique_ptr<net::TCPServerSocket>* socket,
    base::WaitableEvent* waitable_event) {
  // Build the address object.
  std::optional<net::IPAddress> address =
      net::IPAddress::FromIPLiteral(ipv4_address_);
  if (!address) {
    waitable_event->Signal();
    return;
  }

  // Convert the socket descriptor into a TCP server socket.
  auto tcp_socket =
      std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());
  int adopt_result = tcp_socket->AdoptSocket(socket_descriptor);
  if (adopt_result != net::OK) {
    waitable_event->Signal();
    return;
  }

  // Listen on the socket.
  net::IPEndPoint end_point(*address, port);
  int result = tcp_socket->Listen(end_point, 4, /*ipv6_only=*/std::nullopt);
  if (result != net::OK) {
    waitable_event->Signal();
    return;
  }

  // Return the result.
  *socket = std::move(tcp_socket);
  waitable_event->Signal();
}

void WifiDirectMedium::CreateAndConnectSocket(
    const std::string_view& ip_address,
    int port,
    int fd,
    std::unique_ptr<net::TCPClientSocket>* socket,
    base::WaitableEvent* waitable_event) {
  auto ip = net::IPAddress::FromIPLiteral(ip_address);
  if (!ip) {
    // Invalid address.
    waitable_event->Signal();
    return;
  }
  auto ip_endpoint = net::IPEndPoint(*ip, port);
  auto tcp_socket =
      net::TCPSocket::Create(nullptr, nullptr, net::NetLogSource());
  tcp_socket->AdoptUnconnectedSocket(fd);
  tcp_socket->Connect(
      ip_endpoint,
      base::BindOnce(&WifiDirectMedium::OnSocketConnected,
                     base::Unretained(this), socket, waitable_event,
                     std::move(tcp_socket), ip_endpoint));
}

void WifiDirectMedium::OnSocketConnected(
    std::unique_ptr<net::TCPClientSocket>* socket,
    base::WaitableEvent* waitable_event,
    std::unique_ptr<net::TCPSocket> tcp_socket,
    net::IPEndPoint ip_endpoint,
    int result) {
  if (result < 0) {
    waitable_event->Signal();
    return;
  }

  *socket = std::make_unique<net::TCPClientSocket>(std::move(tcp_socket),
                                                   ip_endpoint);
  waitable_event->Signal();
}

void WifiDirectMedium::OpenFirewallHole(
    ash::nearby::TcpServerSocketPort port,
    mojo::PendingRemote<sharing::mojom::FirewallHole>* output,
    base::WaitableEvent* waitable_event) {
  CHECK(io_thread_->task_runner()->RunsTasksInCurrentSequence());
  firewall_hole_factory_->OpenFirewallHole(
      port, base::BindOnce(&WifiDirectMedium::OnFirewallHoleCreated,
                           base::Unretained(this), output, waitable_event));
}

void WifiDirectMedium::OnFirewallHoleCreated(
    mojo::PendingRemote<sharing::mojom::FirewallHole>* output,
    base::WaitableEvent* waitable_event,
    mojo::PendingRemote<sharing::mojom::FirewallHole> firewall_hole) {
  if (firewall_hole.is_valid()) {
    *output = std::move(firewall_hole);
  }
  waitable_event->Signal();
}

void WifiDirectMedium::OnDisconnect() {
  // Reset the connection, since it has been disconnected at this point.
  connection_.reset();
}

}  // namespace nearby::chrome
