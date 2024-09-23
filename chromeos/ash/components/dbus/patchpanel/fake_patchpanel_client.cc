// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"

#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"

namespace ash {

namespace {

FakePatchPanelClient* g_instance = nullptr;

}  // namespace

// static
FakePatchPanelClient* FakePatchPanelClient::Get() {
  return g_instance;
}

FakePatchPanelClient::FakePatchPanelClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

FakePatchPanelClient::~FakePatchPanelClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void FakePatchPanelClient::GetDevices(GetDevicesCallback callback) {}

void FakePatchPanelClient::NotifyAndroidInteractiveState(bool interactive) {
  notify_android_interactive_state_count_++;
}

int FakePatchPanelClient::GetAndroidInteractiveStateNotifyCount() {
  return notify_android_interactive_state_count_;
}

void FakePatchPanelClient::NotifyAndroidWifiMulticastLockChange(bool is_held) {
  notify_android_wifi_multicast_lock_change_count_++;
}

int FakePatchPanelClient::GetAndroidWifiMulticastLockChangeNotifyCount() {
  return notify_android_wifi_multicast_lock_change_count_;
}

void FakePatchPanelClient::NotifySocketConnectionEvent(
    const patchpanel::SocketConnectionEvent& msg) {}

void FakePatchPanelClient::NotifyARCVPNSocketConnectionEvent(
    const patchpanel::SocketConnectionEvent& msg) {}

void FakePatchPanelClient::TagSocket(int socket_fd,
                                     std::optional<int> network_id,
                                     std::optional<VpnRoutingPolicy> vpn_policy,
                                     TagSocketCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tag_socket_success_));
}

void FakePatchPanelClient::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void FakePatchPanelClient::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void FakePatchPanelClient::NotifyNetworkConfigurationChanged() {
  for (auto& observer : observer_list_) {
    observer.NetworkConfigurationChanged();
  }
}

void FakePatchPanelClient::SetFeatureFlag(
    patchpanel::SetFeatureFlagRequest::FeatureFlag flag,
    bool enabled) {}

void FakePatchPanelClient::WaitForServiceToBeAvailable(
    dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) {
  std::move(callback).Run(true);
}

}  // namespace ash
