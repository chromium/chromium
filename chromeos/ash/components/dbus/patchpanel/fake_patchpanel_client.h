// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_FAKE_PATCHPANEL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_FAKE_PATCHPANEL_CLIENT_H_

#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"

#include "base/observer_list.h"

namespace ash {

// FakePatchPanelClient is a stub implementation of PatchPanelClient used for
// testing.
class COMPONENT_EXPORT(PATCHPANEL) FakePatchPanelClient
    : public PatchPanelClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakePatchPanelClient* Get();

  FakePatchPanelClient(const FakePatchPanelClient&) = delete;
  FakePatchPanelClient& operator=(const FakePatchPanelClient&) = delete;

  // PatchPanelClient:
  void GetDevices(GetDevicesCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void NotifyAndroidInteractiveState(bool interactive) override;
  void NotifyAndroidWifiMulticastLockChange(bool is_held) override;
  void NotifySocketConnectionEvent(
      const patchpanel::SocketConnectionEvent& msg) override;
  void NotifyARCVPNSocketConnectionEvent(
      const patchpanel::SocketConnectionEvent& msg) override;
  void TagSocket(int socket_fd,
                 std::optional<int> network_id,
                 std::optional<VpnRoutingPolicy> vpn_policy,
                 TagSocketCallback callback) override;
  void SetFeatureFlag(patchpanel::SetFeatureFlagRequest::FeatureFlag flag,
                      bool enabled) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;

  // Record of count of calling NotifyAndroidInteractiveState for testing
  // purpose.
  int GetAndroidInteractiveStateNotifyCount();
  // Record of count of calling NotifyAndroidWifiMulticastLockChange for
  // testing purpose.
  int GetAndroidWifiMulticastLockChangeNotifyCount();

  void set_tag_socket_success_for_testing(bool success) {
    tag_socket_success_ = success;
  }

 protected:
  friend class PatchPanelClient;

  FakePatchPanelClient();
  ~FakePatchPanelClient() override;

  void Init(dbus::Bus* bus) override {}

  // Calls NetworkConfigurationChanged() on Observer instances.
  void NotifyNetworkConfigurationChanged();

  // List of observers.
  base::ObserverList<Observer> observer_list_;

  int notify_android_interactive_state_count_ = 0;

  int notify_android_wifi_multicast_lock_change_count_ = 0;

  bool tag_socket_success_ = true;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_FAKE_PATCHPANEL_CLIENT_H_
