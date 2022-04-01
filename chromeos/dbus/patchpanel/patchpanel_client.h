// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_
#define CHROMEOS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "chromeos/dbus/common/dbus_client.h"
#include "chromeos/dbus/patchpanel/patchpanel_service.pb.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// Simple wrapper around patchpanel DBus API. The method names and protobuf
// schema used by patchpanel DBus API are defined in
// third_party/cros_system_api/dbus/patchpanel.
class COMPONENT_EXPORT(PATCHPANEL) PatchPanelClient : public DBusClient {
 public:
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

 protected:
  // Initialize/Shutdown should be used instead.
  PatchPanelClient();
  ~PatchPanelClient() override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_PATCHPANEL_PATCHPANEL_CLIENT_H_
