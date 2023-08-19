// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_service.pb.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "dbus/object_proxy.h"

namespace ash {

// Simple wrapper around patchpanel DBus API. The method names and protobuf
// schema used by patchpanel DBus API are defined in
// third_party/cros_system_api/dbus/patchpanel.
class COMPONENT_EXPORT(PATCHPANEL) PatchPanelClient
    : public chromeos::DBusClient {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called when NetworkConfigurationChanged signal is received, when the
    // there is a network configuration change.
    virtual void NetworkConfigurationChanged() {}
  };

  using GetDevicesCallback = base::OnceCallback<void(
      const std::vector<patchpanel::NetworkDevice>& devices)>;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates the global instance with a fake implementation.
  static void InitializeFake();

  // Destroys the global instance which must have been initialized first.
  static void Shutdown();

  // Returns the global instance if initialized. May return null.
  static PatchPanelClient* Get();

  PatchPanelClient(const PatchPanelClient&) = delete;
  PatchPanelClient& operator=(const PatchPanelClient&) = delete;

  // Obtains a list of virtual network interfaces configured and managed by
  // patchpanel.
  virtual void GetDevices(GetDevicesCallback callback) = 0;

  // Called when power status of device is changed.
  virtual void NotifyAndroidInteractiveState(bool interactive) = 0;

  // Called when the status of Android WiFi multicast lock changes from held
  // to not held or vice versa.
  virtual void NotifyAndroidWifiMulticastLockChange(bool is_held) = 0;

  // Adds an observer.
  virtual void AddObserver(Observer* observer) = 0;

  // Removes an observer if added.
  virtual void RemoveObserver(Observer* observer) = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  PatchPanelClient();
  ~PatchPanelClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_
