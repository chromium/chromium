// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

#include "base/task/thread_pool.h"

namespace nearby::chrome {

WifiDirectMedium::WifiDirectMedium(
    const mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>&
        manager,
    const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
        firewall_hole_factory)
    : task_runner_(
          base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})),
      wifi_direct_manager_(std::move(manager)),
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
  NOTIMPLEMENTED();
  return false;
}

bool WifiDirectMedium::StopWifiDirect() {
  NOTIMPLEMENTED();
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
  NOTIMPLEMENTED();
  return nullptr;
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

}  // namespace nearby::chrome
