// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_

#include <cstdint>
#include <memory>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/values.h"
#include "components/optimization_guide/core/delivery/model_info.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

class OnDeviceModelComponentStateManager;

// Base model files and metadata suitable for a FakeOnDeviceModelService.
class FakeBaseModelAsset {
 public:
  struct Content {
    uint32_t weight = 0;
    proto::OnDeviceModelExecutionConfig config;
    uint32_t cache_weight = 0;
    uint32_t encoder_cache_weight = 0;
    uint32_t adapter_cache_weight = 0;
  };
  FakeBaseModelAsset();
  explicit FakeBaseModelAsset(Content content);
  explicit FakeBaseModelAsset(
      const std::vector<proto::OnDeviceModelPerformanceHint>& hints);
  explicit FakeBaseModelAsset(
      proto::OnDeviceModelValidationConfig&& validation_config);
  ~FakeBaseModelAsset();

  // Overwrites content in the same file.
  void Write(Content&& content);

  const base::FilePath& path() const { return temp_dir_.GetPath(); }

  void set_version(const std::string& version) { version_ = version; }
  const std::string& version() const { return version_; }

  // Returns a fake manifest content for this asset.
  base::Value::Dict Manifest() const;

  // Pass this asset to manager->SetReady.
  void SetReadyIn(OnDeviceModelComponentStateManager& manager) const;

  // Constructs metadata compatible with the default constructed asset.
  static proto::OnDeviceBaseModelMetadata DefaultSpec();

 private:
  std::string version_ = "0.0.1";
  base::Value::List supported_performance_hints_;
  base::ScopedTempDir temp_dir_;
};

// Adaptation files and metadata suitable for a FakeOnDeviceModelService.
class FakeAdaptationAsset {
 public:
  struct Content {
    proto::OnDeviceModelExecutionFeatureConfig config;
    std::optional<uint32_t> weight;
    proto::OnDeviceBaseModelMetadata metadata =
        FakeBaseModelAsset::DefaultSpec();
  };
  explicit FakeAdaptationAsset(Content&& content);
  ~FakeAdaptationAsset();

  int64_t version() const { return 12345; }
  mojom::OnDeviceFeature feature() const { return feature_; }
  OnDeviceModelAdaptationMetadata metadata() const { return *metadata_; }

  const ModelInfo& model_info() const { return *model_info_; }

  void SendTo(OnDeviceModelServiceController& controller) const;

  base::FilePath dir() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
  mojom::OnDeviceFeature feature_;
  std::unique_ptr<ModelInfo> model_info_;
  std::unique_ptr<on_device_model::AdaptationAssetPaths> paths_;
  std::unique_ptr<OnDeviceModelAdaptationMetadata> metadata_;
};

// Language model files and metadata suitable for a FakeOnDeviceModelService.
class FakeLanguageModelAsset {
 public:
  FakeLanguageModelAsset();
  ~FakeLanguageModelAsset();

  const ModelInfo& model_info() const { return *model_info_; }
  base::FilePath model_path() const;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ModelInfo> model_info_;
};

// Safety model files and metadata suitable for a FakeOnDeviceModelService.
class FakeSafetyModelAsset {
 public:
  struct Content {
    proto::TextSafetyModelMetadata metadata;
    int64_t model_info_version = 1;
  };
  // Constructs a safety model with the given content.
  explicit FakeSafetyModelAsset(Content&& content);

  // Constructs a simple safety model supporting a single feature.
  explicit FakeSafetyModelAsset(proto::FeatureTextSafetyConfiguration&& config);

  ~FakeSafetyModelAsset();

  const ModelInfo& model_info() const { return *model_info_; }

  base::flat_set<base::FilePath> AdditionalFiles() const {
    return model_info_->GetAdditionalFiles();
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ModelInfo> model_info_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_
