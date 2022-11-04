// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_

#include "chromeos/ash/components/dbus/arc/arcvm_data_migrator_client.h"
#include "chromeos/ash/components/dbus/arcvm_data_migrator/arcvm_data_migrator.pb.h"

namespace ash {

// Fake implementation of ArcVmDataMigratorClient.
class COMPONENT_EXPORT(ASH_DBUS_ARC) FakeArcVmDataMigratorClient
    : public ArcVmDataMigratorClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeArcVmDataMigratorClient* Get();

  // ArcVmDataMigratorClient overrides:
  void StartMigration(const arc::data_migrator::StartMigrationRequest& request,
                      chromeos::VoidDBusMethodCallback callback) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  FakeArcVmDataMigratorClient(const FakeArcVmDataMigratorClient&) = delete;
  FakeArcVmDataMigratorClient& operator=(const FakeArcVmDataMigratorClient&) =
      delete;

 protected:
  friend class ArcVmDataMigratorClient;

  FakeArcVmDataMigratorClient();
  ~FakeArcVmDataMigratorClient() override;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_ARC_FAKE_ARCVM_DATA_MIGRATOR_CLIENT_H_
