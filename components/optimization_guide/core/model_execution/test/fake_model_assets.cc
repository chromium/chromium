// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

FakeBaseModelAsset::FakeBaseModelAsset() {
  CHECK(temp_dir_.CreateUniqueTempDir());
}
FakeBaseModelAsset::~FakeBaseModelAsset() = default;

void FakeBaseModelAsset::Write(
    std::optional<proto::OnDeviceModelExecutionFeatureConfig> config,
    std::optional<proto::OnDeviceModelExecutionFeatureConfig> config2,
    std::optional<proto::OnDeviceModelValidationConfig> validation_config) {
  proto::OnDeviceModelExecutionConfig execution_config;
  if (config) {
    *execution_config.add_feature_configs() = *config;
  }
  if (config2) {
    *execution_config.add_feature_configs() = *config2;
  }
  if (validation_config) {
    *execution_config.mutable_validation_config() = *validation_config;
  }
  CHECK(base::WriteFile(
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile),
      execution_config.SerializeAsString()));
}

FakeAdaptationAsset::FakeAdaptationAsset(FakeAdaptationAsset::Content&& content)
    : feature_(ToModelBasedCapabilityKey(content.config.feature())) {
  if (content.weight) {
    CHECK(temp_dir_.CreateUniqueTempDir());
    paths_ = std::make_unique<on_device_model::AdaptationAssetPaths>();
    paths_->weights =
        temp_dir_.GetPath().Append(kOnDeviceModelAdaptationWeightsFile);
    CHECK(base::WriteFile(paths_->weights,
                          base::NumberToString(content.weight.value())));
  }
  metadata_ = OnDeviceModelAdaptationMetadata::New(
      paths_.get(), version(),
      base::MakeRefCounted<OnDeviceModelFeatureAdapter>(
          std::move(content.config)));
}
FakeAdaptationAsset::~FakeAdaptationAsset() = default;

FakeLanguageModelAsset::FakeLanguageModelAsset() {
  CHECK(temp_dir_.CreateUniqueTempDir());
  auto model_path = temp_dir_.GetPath().Append(kWeightsFile);
  CHECK(base::WriteFile(model_path, on_device_model::FakeLanguageModel()));
  model_info_ = TestModelInfoBuilder()
                    .SetModelFilePath(model_path)
                    .SetVersion(123)
                    .Build();
}
FakeLanguageModelAsset::~FakeLanguageModelAsset() = default;

FakeSafetyModelAsset::FakeSafetyModelAsset(
    proto::FeatureTextSafetyConfiguration&& config)
    : FakeSafetyModelAsset(FakeSafetyModelAsset::Content{
          .metadata = SafetyMetadata({std::move(config)})}) {}

FakeSafetyModelAsset::FakeSafetyModelAsset(
    FakeSafetyModelAsset::Content&& content) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  auto data_path = temp_dir_.GetPath().Append(kTsDataFile);
  auto model_path = temp_dir_.GetPath().Append(kTsSpModelFile);
  CHECK(base::WriteFile(data_path, on_device_model::FakeTsData()));
  CHECK(base::WriteFile(model_path, on_device_model::FakeTsSpModel()));
  proto::Any any;
  any.set_type_url(
      "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
  content.metadata.SerializeToString(any.mutable_value());
  model_info_ = TestModelInfoBuilder()
                    .SetAdditionalFiles({data_path, model_path})
                    .SetModelMetadata(any)
                    .Build();
}

FakeSafetyModelAsset::~FakeSafetyModelAsset() = default;

}  // namespace optimization_guide
