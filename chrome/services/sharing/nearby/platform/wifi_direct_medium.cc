// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include <netinet/in.h>

#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/services/sharing/nearby/platform/wifi_direct_server_socket.h"
#include "net/socket/socket_descriptor.h"

namespace nearby::chrome {

WifiDirectMedium::WifiDirectMedium(
    const mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>&
        wifi_direct_manager,
    const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
        firewall_hole_factory)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      wifi_direct_manager_(std::move(wifi_direct_manager)),
      firewall_hole_factory_(std::move(firewall_hole_factory)) {}

WifiDirectMedium::~WifiDirectMedium() = default;

bool WifiDirectMedium::IsInterfaceValid() const {
  bool is_interface_valid = false;
  base::WaitableEvent waitable_event;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectMedium::GetCapabilities, base::Unretained(this),
                     &is_interface_valid, &waitable_event));
  waitable_event.Wait();
  return is_interface_valid;
}

bool WifiDirectMedium::StartWifiDirect(WifiDirectCredentials* credentials) {
  // Wrap the async mojo call to make it sync.
  base::WaitableEvent waitable_event;
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WifiDirectMedium::CreateGroup, base::Unretained(this),
                     credentials, &waitable_event));
  waitable_event.Wait();

  // An active remote means the group has been created.
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
  NOTIMPLEMENTED();
  return false;
}

bool WifiDirectMedium::DisconnectWifiDirect() {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<api::WifiDirectSocket> WifiDirectMedium::ConnectToService(
    absl::string_view ip_address,
    int port,
    CancellationFlag* cancellation_flag) {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<api::WifiDirectServerSocket> WifiDirectMedium::ListenForService(
    int port) {
  // Ensure that there is a valid WiFi Direct connection.
  if (!connection_) {
    return nullptr;
  }

  // Create server socket.
  auto fd = base::ScopedFD(
      net::CreatePlatformSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
  if (fd.get() < 0) {
    return nullptr;
  }
  mojo::PlatformHandle handle = mojo::PlatformHandle(std::move(fd));

  // Wrap the async mojo call to make it sync.
  bool did_associate;
  {
    base::WaitableEvent waitable_event;
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WifiDirectMedium::AssociateSocket,
                                  base::Unretained(this), &did_associate,
                                  &waitable_event, handle.Clone()));
    waitable_event.Wait();
  }

  if (!did_associate) {
    // Socket not associated at the platform layer.
    return nullptr;
  }

  return std::make_unique<WifiDirectServerSocket>(std::move(handle));
}

absl::optional<std::pair<std::int32_t, std::int32_t>>
WifiDirectMedium::GetDynamicPortRange() {
  NOTIMPLEMENTED();
  return absl::nullopt;
}

void WifiDirectMedium::GetCapabilities(
    bool* is_capability_supported,
    base::WaitableEvent* waitable_event) const {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  wifi_direct_manager_->GetWifiP2PCapabilities(
      base::BindOnce(&WifiDirectMedium::OnCapabilities, base::Unretained(this),
                     is_capability_supported, waitable_event));
}

void WifiDirectMedium::OnCapabilities(
    bool* is_capability_supported,
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiP2PCapabilitiesPtr capabilities) const {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  // TODO(b/341325756): The current mojo API only has `is_client_ready` and
  // `is_owner_ready`, both of which return false. There are two options here:
  //    1. Update the mojo API to include `is_p2p_supported` and use that.
  //    2. Update the Nearby API to specify owner or client AND fix the current
  //       mojo responses.
  *is_capability_supported = true;
  waitable_event->Signal();
}

void WifiDirectMedium::CreateGroup(WifiDirectCredentials* credentials,
                                   base::WaitableEvent* waitable_event) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());

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
  CHECK(task_runner_->RunsTasksInCurrentSequence());

  if (result == ash::wifi_direct::mojom::WifiDirectOperationResult::kSuccess) {
    // Store the connection so that the group can be destroyed when the remote
    // is reset.
    connection_.Bind(std::move(connection), task_runner_);
    connection_.set_disconnect_handler(
        base::BindOnce(&WifiDirectMedium::OnDisconnect, base::Unretained(this)),
        task_runner_);

    // Fetch the IPv4 address from the connection.
    connection_->GetProperties(base::BindOnce(&WifiDirectMedium::OnProperties,
                                              base::Unretained(this),
                                              credentials, waitable_event));
    return;
  }

  // Trigger sync signal.
  waitable_event->Signal();
}

void WifiDirectMedium::OnProperties(
    WifiDirectCredentials* credentials,
    base::WaitableEvent* waitable_event,
    ash::wifi_direct::mojom::WifiDirectConnectionPropertiesPtr properties) {
  credentials->SetIPAddress(properties->ipv4_address);
  credentials->SetGateway(properties->ipv4_address);
  waitable_event->Signal();
}

void WifiDirectMedium::AssociateSocket(bool* did_associate,
                                       base::WaitableEvent* waitable_event,
                                       mojo::PlatformHandle socket_handle) {
  CHECK(task_runner_->RunsTasksInCurrentSequence());
  CHECK(connection_.is_bound());
  connection_->AssociateSocket(
      std::move(socket_handle),
      base::BindOnce(&WifiDirectMedium::OnSocketAssociated,
                     base::Unretained(this), did_associate, waitable_event));
}

void WifiDirectMedium::OnSocketAssociated(bool* did_associate,
                                          base::WaitableEvent* waitable_event,
                                          bool success) {
  *did_associate = success;
  waitable_event->Signal();
}

void WifiDirectMedium::OnDisconnect() {
  // Reset the connection, since it has been disconnected at this point.
  connection_.reset();
}

}  // namespace nearby::chrome
