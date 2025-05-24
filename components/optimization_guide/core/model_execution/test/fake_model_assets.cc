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
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

FakeBaseModelAsset::FakeBaseModelAsset()
    : FakeBaseModelAsset(FakeBaseModelAsset::Content{}) {}
FakeBaseModelAsset::FakeBaseModelAsset(Content&& content)
    : version_(content.version) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  Write(std::move(content));
}
FakeBaseModelAsset::FakeBaseModelAsset(
    proto::OnDeviceModelValidationConfig&& validation_config)
    : FakeBaseModelAsset({
          .config = ExecutionConfigWithValidation(std::move(validation_config)),
      }) {}
FakeBaseModelAsset::~FakeBaseModelAsset() = default;

void FakeBaseModelAsset::Write(Content&& content) {
  CHECK(base::WriteFile(temp_dir_.GetPath().Append(kWeightsFile),
                        base::NumberToString(content.weight)));
  if (content.cache_weight) {
    CHECK(base::WriteFile(temp_dir_.GetPath().Append(kExperimentalCacheFile),
                          base::NumberToString(content.cache_weight)));
  }
  CHECK(base::WriteFile(
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile),
      content.config.SerializeAsString()));
}

base::Value::Dict FakeBaseModelAsset::Manifest() const {
  return base::Value::Dict().Set(
      "BaseModelSpec",
      base::Value::Dict().Set("version", "0.0.1").Set("name", "Test"));
}

void FakeBaseModelAsset::SetReadyIn(
    OnDeviceModelComponentStateManager& manager) const {
  manager.SetReady(base::Version(version()), path(), Manifest());
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

void FakeAdaptationAsset::SendTo(
    OnDeviceModelServiceController& controller) const {
  controller.MaybeUpdateModelAdaptation(feature(), metadata());
}

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
  model_info_ = TestModelInfoBuilder()
                    .SetVersion(content.model_info_version)
                    .SetAdditionalFiles({data_path, model_path})
                    .SetModelMetadata(AnyWrapProto(content.metadata))
                    .Build();
}

FakeSafetyModelAsset::~FakeSafetyModelAsset() = default;

}  // namespace optimization_guide
