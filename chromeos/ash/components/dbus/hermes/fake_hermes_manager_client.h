// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/hermes/hermes_manager_client.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

// Fake implementation for HermesManagerClient.
class COMPONENT_EXPORT(HERMES_CLIENT) FakeHermesManagerClient
    : public HermesManagerClient,
      public HermesManagerClient::TestInterface {
 public:
  FakeHermesManagerClient();
  FakeHermesManagerClient(const FakeHermesManagerClient&) = delete;
  FakeHermesManagerClient& operator=(const FakeHermesManagerClient&) = delete;
  ~FakeHermesManagerClient() override;

  // HermesManagerClient::TestInterface:
  void AddEuicc(const dbus::ObjectPath& path,
                const std::string& eid,
                bool is_active,
                uint32_t physical_slot) override;
  void ClearEuiccs() override;

  // HermesManagerClient:
  const std::vector<dbus::ObjectPath>& GetAvailableEuiccs() override;
  HermesManagerClient::TestInterface* GetTestInterface() override;

 private:
  void ParseCommandLineSwitch();
  void NotifyAvailableEuiccListChanged();

  // List of paths to available and euicc objects.
  std::vector<dbus::ObjectPath> available_euiccs_;

  base::WeakPtrFactory<FakeHermesManagerClient> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_
