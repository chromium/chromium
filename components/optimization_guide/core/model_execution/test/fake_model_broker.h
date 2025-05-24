// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_

#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/model_broker_client.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/model_execution/test/test_on_device_model_component_state_manager.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

class FakeModelBroker {
 public:
  explicit FakeModelBroker(const FakeAdaptationAsset& asset);
  ~FakeModelBroker();

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote();

  on_device_model::FakeOnDeviceServiceSettings& settings() {
    return fake_settings_;
  }

  void CrashService() { fake_launcher_.CrashService(); }

  void UpdateModelAdaptation(const FakeAdaptationAsset& asset);
  void UpdateSafetyModel(const optimization_guide::ModelInfo& model_info) {
    test_controller_->MaybeUpdateSafetyModel(model_info);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingPrefServiceSimple pref_service_;
  FakeBaseModelAsset base_model_;
  on_device_model::FakeOnDeviceServiceSettings fake_settings_;
  on_device_model::FakeServiceLauncher fake_launcher_{&fake_settings_};
  TestOnDeviceModelComponentStateManager component_manager_{&pref_service_};
  scoped_refptr<OnDeviceModelServiceController> test_controller_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_H_
