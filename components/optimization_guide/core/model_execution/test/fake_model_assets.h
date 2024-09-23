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
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"
#include "services/on_device_model/public/cpp/model_assets.h"

namespace optimization_guide {

// Base model files and metadata suitable for a FakeOnDeviceModelService.
class FakeBaseModelAsset {
 public:
  FakeBaseModelAsset();
  ~FakeBaseModelAsset();

  void Write(std::optional<proto::OnDeviceModelExecutionFeatureConfig> config =
                 std::nullopt,
             std::optional<proto::OnDeviceModelExecutionFeatureConfig> config2 =
                 std::nullopt,
             std::optional<proto::OnDeviceModelValidationConfig>
                 validation_config = std::nullopt);

  const base::FilePath& path() { return temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir temp_dir_;
};

// Adaptation files and metadata suitable for a FakeOnDeviceModelService.
class FakeAdaptationAsset {
 public:
  struct Content {
    proto::OnDeviceModelExecutionFeatureConfig config;
    std::optional<uint32_t> weight;
  };
  explicit FakeAdaptationAsset(Content&& content);
  ~FakeAdaptationAsset();

  int64_t version() { return 12345; }
  ModelBasedCapabilityKey feature() { return feature_; }
  std::unique_ptr<OnDeviceModelAdaptationMetadata> metadata() {
    return std::make_unique<OnDeviceModelAdaptationMetadata>(*metadata_);
  }

 private:
  base::ScopedTempDir temp_dir_;
  ModelBasedCapabilityKey feature_;
  std::unique_ptr<on_device_model::AdaptationAssetPaths> paths_;
  std::unique_ptr<OnDeviceModelAdaptationMetadata> metadata_;
};

// Language model files and metadata suitable for a FakeOnDeviceModelService.
class FakeLanguageModelAsset {
 public:
  FakeLanguageModelAsset();
  ~FakeLanguageModelAsset();

  const ModelInfo& model_info() { return *model_info_; }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ModelInfo> model_info_;
};

// Safety model files and metadata suitable for a FakeOnDeviceModelService.
class FakeSafetyModelAsset {
 public:
  struct Content {
    proto::TextSafetyModelMetadata metadata;
  };
  // Constructs a safety model with the given content.
  explicit FakeSafetyModelAsset(Content&& content);

  // Constructs a simple safety model supporting a single feature.
  explicit FakeSafetyModelAsset(proto::FeatureTextSafetyConfiguration&& config);

  ~FakeSafetyModelAsset();

  const ModelInfo& model_info() { return *model_info_; }

  base::flat_set<base::FilePath> AdditionalFiles() {
    return model_info_->GetAdditionalFiles();
  }

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<ModelInfo> model_info_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_
