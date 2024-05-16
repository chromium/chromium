// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/platform/wifi_direct_medium.h"

namespace nearby::chrome {

WifiDirectMedium::WifiDirectMedium(
    const mojo::SharedRemote<ash::wifi_direct::mojom::WifiDirectManager>&
        manager,
    const mojo::SharedRemote<::sharing::mojom::FirewallHoleFactory>&
        firewall_hole_factory)
    : manager_(std::move(manager)),
      firewall_hole_factory_(std::move(firewall_hole_factory)) {}

WifiDirectMedium::~WifiDirectMedium() = default;

bool WifiDirectMedium::IsInterfaceValid() const {
  NOTIMPLEMENTED();
  return false;
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

}  // namespace nearby::chrome
