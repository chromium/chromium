// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom.h"
#include "components/prefs/testing_pref_service.h"
#include "services/on_device_model/android/model_downloader_android.h"
#include "services/on_device_model/android/on_device_model_bridge_native_unittest_helper.h"

namespace optimization_guide {
class FakeAndroidManifestAsset;

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
  struct Options {
    on_device_model::ModelDownloaderAndroid::BaseModelSpec base_model_spec{
        .name = "Test",
        .version = "0.0.1",
    };
    // If true, the Java ModelDownloader will report the base model as already
    // installed.
    bool preinstall_base_model = true;
  };

  explicit FakeModelBrokerAndroid(const Options& options);
  ~FakeModelBrokerAndroid();

  mojo::PendingRemote<mojom::ModelBroker> BindAndPassRemote();

  void InstallBaseModel();
  void UnInstallBaseModel();

  void UpdateManifest(std::unique_ptr<FakeAndroidManifestAsset> asset);

  PrefService& local_state() { return local_state_; }
  on_device_model::OnDeviceModelBridgeNativeUnitTestHelper& java_helper() {
    return java_helper_;
  }
  ModelBrokerAndroid& broker() { return EnsureBroker(); }

 private:
  ModelBrokerAndroid& EnsureBroker();

  Options options_;
  ScopedModelBrokerAndroidFeatureList feature_list_;
  TestingPrefServiceSimple local_state_;
  on_device_model::OnDeviceModelBridgeNativeUnitTestHelper java_helper_;
  std::optional<ModelBrokerAndroid> broker_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_BROKER_ANDROID_H_
