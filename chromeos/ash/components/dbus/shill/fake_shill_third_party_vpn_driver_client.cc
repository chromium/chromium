// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_shill_third_party_vpn_driver_client.h"

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/shill/shill_third_party_vpn_observer.h"
#include "dbus/object_proxy.h"

namespace ash {

FakeShillThirdPartyVpnDriverClient::FakeShillThirdPartyVpnDriverClient() =
    default;

FakeShillThirdPartyVpnDriverClient::~FakeShillThirdPartyVpnDriverClient() =
    default;

void FakeShillThirdPartyVpnDriverClient::AddShillThirdPartyVpnObserver(
    const std::string& object_path_value,
    ShillThirdPartyVpnObserver* observer) {
  if (observer_map_.find(object_path_value) != observer_map_.end()) {
    VLOG(2) << "Observer exists.";
    return;
  }
  observer_map_[object_path_value] = observer;
}

void FakeShillThirdPartyVpnDriverClient::RemoveShillThirdPartyVpnObserver(
    const std::string& object_path_value) {
  if (observer_map_.find(object_path_value) == observer_map_.end()) {
    VLOG(2) << "Observer does not exist.";
    return;
  }
  observer_map_.erase(object_path_value);
}

void FakeShillThirdPartyVpnDriverClient::SetParameters(
    const std::string& object_path_value,
    const base::Value::Dict& parameters,
    StringCallback callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::string()));
}

void FakeShillThirdPartyVpnDriverClient::UpdateConnectionState(
    const std::string& object_path_value,
    const uint32_t connection_state,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillThirdPartyVpnDriverClient::SendPacket(
    const std::string& object_path_value,
    const std::vector<char>& ip_packet,
    base::OnceClosure callback,
    ErrorCallback error_callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, std::move(callback));
}

void FakeShillThirdPartyVpnDriverClient::OnPacketReceived(
    const std::string& object_path_value,
    const std::vector<char>& packet) {
  ObserverMap::iterator it = observer_map_.find(object_path_value);
  if (it == observer_map_.end()) {
    LOG(ERROR) << "Unexpected OnPacketReceived for " << object_path_value;
    return;
  }

  it->second->OnPacketReceived(packet);
}

void FakeShillThirdPartyVpnDriverClient::OnPlatformMessage(
    const std::string& object_path_value,
    uint32_t message) {
  ObserverMap::iterator it = observer_map_.find(object_path_value);
  if (it == observer_map_.end()) {
    LOG(ERROR) << "Unexpected OnPlatformMessage for " << object_path_value;
    return;
  }

  it->second->OnPlatformMessage(message);
}

ShillThirdPartyVpnDriverClient::TestInterface*
FakeShillThirdPartyVpnDriverClient::GetTestInterface() {
  return this;
}

}  // namespace ash
