// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill/fake_shill_third_party_vpn_driver_client.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/dbus/shill/shill_third_party_vpn_observer.h"
#include "dbus/object_proxy.h"

namespace chromeos {

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
    const base::DictionaryValue& parameters,
    const ShillClientHelper::StringCallback& callback,
    const ShillClientHelper::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(callback, std::string()));
}

void FakeShillThirdPartyVpnDriverClient::UpdateConnectionState(
    const std::string& object_path_value,
    const uint32_t connection_state,
    const base::Closure& callback,
    const ShillClientHelper::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
}

void FakeShillThirdPartyVpnDriverClient::SendPacket(
    const std::string& object_path_value,
    const std::vector<char>& ip_packet,
    const base::Closure& callback,
    const ShillClientHelper::ErrorCallback& error_callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
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

}  // namespace chromeos
