// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_FAKE_MANIFEST_BROKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_FAKE_MANIFEST_BROKER_H_

#include <memory>

#include "components/optimization_guide/core/model_execution/manifest_broker/manifest_broker_state.h"
#include "components/optimization_guide/core/model_execution/manifest_broker/test/test_manifest_asset_manager_component_state.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/test/fake_component_update_service.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

class FakeManifestBroker {
 public:
  FakeManifestBroker();
  ~FakeManifestBroker();

  FakeManifestBroker(const FakeManifestBroker&) = delete;
  FakeManifestBroker& operator=(const FakeManifestBroker&) = delete;

  // Constructs the broker and a client.  Idempotent.
  void Startup();
  // Destroys the broker and client, and restarts the fake
  // ComponentUpdateService.
  void SimulateShutdown();
  // Constructs a simple Session for the "test" use case.
  ModelBrokerClient::CreateSessionResult CreateSession();
  // Brings the system into a normal steady-state by doing single startup,
  // create session, restart cycle.  This ensures performance class is computed
  // and assets for the "test" use case are downloaded.  Returns true iff
  // successful.
  bool WarmupPrefsAndAssets();

  ModelBrokerClient& client() { return *model_broker_client_; }
  ManifestBrokerState& state() { return *manifest_broker_state_; }
  PrefService& local_state() { return local_state_.local_state(); }
  TestManifestAssetManagerComponentState& component_state() {
    return component_state_;
  }
  on_device_model::FakeOnDeviceServiceSettings& settings() { return settings_; }
  on_device_model::FakeServiceLauncher& launcher() { return launcher_; }

 private:
  ScopedModelBrokerFeatureList scoped_feature_list_;
  testing::NiceMock<FakeComponentUpdateService> component_update_service_;
  ModelBrokerPrefService local_state_;
  TestManifestAssetManagerComponentState component_state_;
  on_device_model::FakeOnDeviceServiceSettings settings_;
  on_device_model::FakeServiceLauncher launcher_{&settings_};

  std::unique_ptr<ManifestBrokerState> manifest_broker_state_;
  std::unique_ptr<ModelBrokerClient> model_broker_client_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_MANIFEST_BROKER_TEST_FAKE_MANIFEST_BROKER_H_
