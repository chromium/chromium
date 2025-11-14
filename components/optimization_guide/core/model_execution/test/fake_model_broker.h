// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

// A ScopedFeatureList initialized with reasonable defaults for testing
// ModelBroker related features.
class ScopedModelBrokerFeatureList {
 public:
  ScopedModelBrokerFeatureList();
  ~ScopedModelBrokerFeatureList();

 private:
  base::test::ScopedFeatureList feature_list_;
};

// A TestingPrefServiceSimple with model broker prefs registered.
class ModelBrokerPrefService {
 public:
  ModelBrokerPrefService();
  ~ModelBrokerPrefService();

  PrefService& local_state() { return local_state_; }

 private:
  TestingPrefServiceSimple local_state_;
};

class FakeModelBroker {
 public:
  // Options for how to setup this fixture.
  struct Options {
    // Initializes prefs so that this is the determined performance class.
    // Setting this to kUnknown will emulate first-run state.
    OnDeviceModelPerformanceClass performance_class =
        OnDeviceModelPerformanceClass::kHigh;
    // If true, installs a base model to the component_state_.
    bool preinstall_base_model = true;
  };
  explicit FakeModelBroker(const Options& options);
  ~FakeModelBroker();

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote();

  on_device_model::FakeOnDeviceServiceSettings& settings() {
    return fake_settings_;
  }

  void SimulateShutdown() {
    model_broker_state_.reset();
    component_state_.SimulateShutdown();
  }
  void CrashService() { fake_launcher_.CrashService(); }

  void InstallBaseModel(FakeBaseModelAsset::Content content);
  void InstallBaseModel(std::unique_ptr<FakeBaseModelAsset> asset);
  void UpdateTarget(proto::OptimizationTarget target,
                    const ModelInfo& model_info);
  void UpdateModelAdaptation(const FakeAdaptationAsset& asset);
  void UpdateSafetyModel(const FakeSafetyModelAsset& asset);
  void UpdateLanguageDetectionModel(const FakeLanguageModelAsset& asset);

  PrefService& local_state() { return local_state_.local_state(); }
  TestComponentState& component_state() { return component_state_; }
  ModelProviderRegistry& model_provider() { return model_provider_; }

  // Lazily instantiates model_broker_state_
  ModelBrokerState& GetOrCreateBrokerState();

  on_device_model::FakeOnDeviceServiceSettings& service_settings() {
    return fake_settings_;
  }
  on_device_model::FakeServiceLauncher& launcher() { return fake_launcher_; }

 private:
  ScopedModelBrokerFeatureList feature_list_;
  ModelBrokerPrefService local_state_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  TestComponentState component_state_;
  OptimizationGuideLogger logger_;
  ModelProviderRegistry model_provider_{&logger_};
  std::optional<ModelBrokerState> model_broker_state_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
