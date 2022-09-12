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

 protected:
  friend class PatchPanelClient;

  FakePatchPanelClient();
  ~FakePatchPanelClient() override;

  void Init(dbus::Bus* bus) override {}

  // Calls NetworkConfigurationChanged() on Observer instances.
  void NotifyNetworkConfigurationChanged();

  // List of observers.
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_FAKE_PATCHPANEL_CLIENT_H_
