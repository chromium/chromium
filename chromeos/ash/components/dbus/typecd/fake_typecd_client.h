// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/typecd/typecd_client.h"
#include "third_party/cros_system_api/dbus/typecd/dbus-constants.h"

namespace ash {

class COMPONENT_EXPORT(TYPECD) FakeTypecdClient : public TypecdClient {
 public:
  FakeTypecdClient();
  FakeTypecdClient(const FakeTypecdClient&) = delete;
  FakeTypecdClient& operator=(const FakeTypecdClient&) = delete;
  ~FakeTypecdClient() override;

  // This is a simple fake to notify observers of a simulated D-Bus received
  // signal.
  void EmitThunderboltDeviceConnectedSignal(bool is_thunderbolt_only);
  void EmitCableWarningSignal(typecd::CableWarningType type);

  // TypecdClient:
  void SetPeripheralDataAccessPermissionState(bool permitted) override;
  void SetTypeCPortsUsingDisplays(
      const std::vector<uint32_t>& port_nums) override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_TYPECD_FAKE_TYPECD_CLIENT_H_
