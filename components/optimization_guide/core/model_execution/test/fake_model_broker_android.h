// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_

#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/delivery/model_provider_registry.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"
#include "components/optimization_guide/core/model_execution/test/fake_model_broker.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"

namespace optimization_guide {

// A ScopedFeatureList initialized with defaults for testing
// ModelBrokerAndroid related features.
class ScopedModelBrokerAndroidFeatureList {
 public:
  ScopedModelBrokerAndroidFeatureList();
  ~ScopedModelBrokerAndroidFeatureList();

 private:
  base::test::ScopedFeatureList feature_list_;
};

class FakeModelBrokerAndroid {
 public:
  FakeModelBrokerAndroid();
  ~FakeModelBrokerAndroid();

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote();

  void UpdateModelAdaptation(const FakeAdaptationAsset& asset);

  PrefService& local_state() { return local_state_.local_state(); }
  on_device_model::OnDeviceModelBridgeNativeUnitTestHelper& java_helper() {
    return java_helper_;
  }

 private:
  ModelBrokerAndroid& EnsureBroker();
  void UpdateTarget(proto::OptimizationTarget target,
                    const ModelInfo& model_info);

  ScopedModelBrokerAndroidFeatureList feature_list_;
  ModelBrokerPrefService local_state_;
  OptimizationGuideLogger logger_;
  ModelProviderRegistry model_provider_{&logger_};
  on_device_model::OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
  std::optional<ModelBrokerAndroid> broker_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_
