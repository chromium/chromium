// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_PROCESS_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_PROCESS_MANAGER_H_

#include <string>
#include <string_view>

#include "base/component_export.h"
#include "chromeos/ash/components/data_migration/testing/fake_nearby_connections.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/shared_remote.h"

namespace data_migration {

// Does not actually launch a utility process. Everything is done in the same
// process as the test. This connects
// `data_migration::FakeNearbyConnections` to any remotes requesting the
// `::nearby::connections::mojom::NearbyConnections` API.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION)
    FakeNearbyProcessManager : public ash::nearby::NearbyProcessManager {
 public:
  explicit FakeNearbyProcessManager(std::string_view remote_endpoint_id);
  FakeNearbyProcessManager(const FakeNearbyProcessManager&) = delete;
  FakeNearbyProcessManager& operator=(const FakeNearbyProcessManager&) = delete;
  ~FakeNearbyProcessManager() override;

  FakeNearbyConnections& fake_nearby_connections() {
    return fake_nearby_connections_;
  }

 private:
  // nearby::NearbyProcessManager implementation:
  std::unique_ptr<NearbyProcessReference> GetNearbyProcessReference(
      NearbyProcessStoppedCallback on_process_stopped_callback) override;
  void ShutDownProcess() override;
  void InitializeProcess();

  FakeNearbyConnections fake_nearby_connections_;
  mojo::Receiver<::nearby::connections::mojom::NearbyConnections> receiver_;
  mojo::SharedRemote<::nearby::connections::mojom::NearbyConnections> remote_;
};

}  // namespace data_migration

#endif  // CHROMEOS_ASH_COMPONENTS_DATA_MIGRATION_TESTING_FAKE_NEARBY_PROCESS_MANAGER_H_
