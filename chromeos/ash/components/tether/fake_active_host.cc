// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/fake_active_host.h"

#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/multidevice/remote_device_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

FakeActiveHost::FakeActiveHost()
    : ActiveHost(nullptr, nullptr),
      active_host_status_(ActiveHost::ActiveHostStatus::DISCONNECTED),
      active_host_device_id_(std::string()),
      tether_network_guid_(std::string()),
      wifi_network_guid_(std::string()) {}

FakeActiveHost::~FakeActiveHost() = default;

void FakeActiveHost::SetActiveHostDisconnected() {
  SetActiveHost(ActiveHost::ActiveHostStatus::DISCONNECTED, "", "", "");
}

void FakeActiveHost::SetActiveHostConnecting(
    const std::string& active_host_device_id,
    const std::string& tether_network_guid) {
  SetActiveHost(ActiveHost::ActiveHostStatus::CONNECTING, active_host_device_id,
                tether_network_guid, "");
}

void FakeActiveHost::SetActiveHostConnected(
    const std::string& active_host_device_id,
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  SetActiveHost(ActiveHost::ActiveHostStatus::CONNECTED, active_host_device_id,
                tether_network_guid, wifi_network_guid);
}

void FakeActiveHost::GetActiveHost(
    ActiveHost::ActiveHostCallback active_host_callback) {
  std::optional<multidevice::RemoteDeviceRef> remote_device;
  if (GetActiveHostStatus() != ActiveHost::ActiveHostStatus::DISCONNECTED) {
    // Convert the active host ID to a public key.
    std::string public_key;
    ASSERT_TRUE(base::Base64Decode(GetActiveHostDeviceId(), &public_key));

    // Create a new RemoteDevice and set its public key.
    remote_device = std::make_optional<multidevice::RemoteDeviceRef>(
        multidevice::RemoteDeviceRefBuilder().SetPublicKey(public_key).Build());
  }

  std::move(active_host_callback)
      .Run(GetActiveHostStatus(), remote_device, GetTetherNetworkGuid(),
           GetWifiNetworkGuid());
}

ActiveHost::ActiveHostStatus FakeActiveHost::GetActiveHostStatus() const {
  return active_host_status_;
}

std::string FakeActiveHost::GetActiveHostDeviceId() const {
  return active_host_device_id_;
}

std::string FakeActiveHost::GetTetherNetworkGuid() const {
  return tether_network_guid_;
}

std::string FakeActiveHost::GetWifiNetworkGuid() const {
  return wifi_network_guid_;
}

void FakeActiveHost::SetActiveHost(ActiveHostStatus active_host_status,
                                   const std::string& active_host_device_id,
                                   const std::string& tether_network_guid,
                                   const std::string& wifi_network_guid) {
  ActiveHostStatus old_status = GetActiveHostStatus();
  std::string old_device_id = GetActiveHostDeviceId();
  std::string old_tether_network_guid = GetTetherNetworkGuid();
  std::string old_wifi_network_guid = GetWifiNetworkGuid();

  if (old_status == active_host_status &&
      old_device_id == active_host_device_id &&
      old_tether_network_guid == tether_network_guid &&
      old_wifi_network_guid == wifi_network_guid) {
    // If nothing has changed, return early.
    return;
  }

  active_host_status_ = active_host_status;
  active_host_device_id_ = active_host_device_id;
  tether_network_guid_ = tether_network_guid;
  wifi_network_guid_ = wifi_network_guid;

  GetActiveHost(base::BindOnce(&FakeActiveHost::SendActiveHostChangedUpdate,
                               base::Unretained(this), old_status,
                               old_device_id, old_tether_network_guid,
                               old_wifi_network_guid));
}

}  // namespace tether

}  // namespace ash
