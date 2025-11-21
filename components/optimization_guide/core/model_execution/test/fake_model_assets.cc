// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/test_model_info_builder.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/public/mojom/model_broker.mojom-data-view.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"

namespace optimization_guide {

FakeBaseModelAsset::FakeBaseModelAsset()
    : FakeBaseModelAsset(FakeBaseModelAsset::Content{}) {}
FakeBaseModelAsset::FakeBaseModelAsset(Content content) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  // Support all performance hints by default.
  supported_performance_hints_ =
      base::Value::List()
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE)
          .Append(proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
  Write(std::move(content));
}
FakeBaseModelAsset::FakeBaseModelAsset(
    const std::vector<proto::OnDeviceModelPerformanceHint>& hints) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  for (const auto& hint : hints) {
    supported_performance_hints_.Append(hint);
  }
  Write({});
}
FakeBaseModelAsset::FakeBaseModelAsset(
    proto::OnDeviceModelValidationConfig&& validation_config)
    : FakeBaseModelAsset(Content{
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
  if (content.encoder_cache_weight) {
    CHECK(base::WriteFile(temp_dir_.GetPath().Append(kEncoderCacheFile),
                          base::NumberToString(content.encoder_cache_weight)));
  }
  if (content.adapter_cache_weight) {
    CHECK(base::WriteFile(temp_dir_.GetPath().Append(kAdapterCacheFile),
                          base::NumberToString(content.adapter_cache_weight)));
  }
  CHECK(base::WriteFile(
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile),
      content.config.SerializeAsString()));
}

base::Value::Dict FakeBaseModelAsset::Manifest() const {
  return base::Value::Dict().Set(
      "BaseModelSpec", base::Value::Dict()
                           .Set("version", "0.0.1")
                           .Set("name", "Test")
                           .Set("supported_performance_hints",
                                supported_performance_hints_.Clone()));
}

void FakeBaseModelAsset::SetReadyIn(
    OnDeviceModelComponentStateManager& manager) const {
  manager.SetReady(base::Version(version()), path(), Manifest());
}

proto::OnDeviceBaseModelMetadata FakeBaseModelAsset::DefaultSpec() {
  proto::OnDeviceBaseModelMetadata result;
  result.set_base_model_version("0.0.1");
  result.set_base_model_name("Test");
  result.add_supported_performance_hints(
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_HIGHEST_QUALITY);
  result.add_supported_performance_hints(
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_FASTEST_INFERENCE);
  result.add_supported_performance_hints(
      proto::ON_DEVICE_MODEL_PERFORMANCE_HINT_CPU);
  return result;
}

FakeAdaptationAsset::FakeAdaptationAsset(FakeAdaptationAsset::Content&& content)
    : feature_(*ToOnDeviceFeature(content.config.feature())) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  base::FilePath config_path =
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile);
  {
    proto::OnDeviceModelExecutionConfig config;
    *config.add_feature_configs() = content.config;
    CHECK(base::WriteFile(config_path, config.SerializeAsString()));
  }
  TestModelInfoBuilder builder;
  builder.SetVersion(version())
      .SetAdditionalFiles({config_path})
      .SetModelMetadata(AnyWrapProto(content.metadata));
  if (content.weight) {
    paths_ = std::make_unique<on_device_model::AdaptationAssetPaths>();
    paths_->weights =
        temp_dir_.GetPath().Append(kOnDeviceModelAdaptationWeightsFile);
    CHECK(base::WriteFile(paths_->weights,
                          base::NumberToString(content.weight.value())));
    builder.SetAdditionalFiles({config_path, paths_->weights});
  }
  model_info_ = builder.Build();
  metadata_ = std::make_unique<OnDeviceModelAdaptationMetadata>(
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
  auto model_path = this->model_path();
  CHECK(base::WriteFile(model_path, on_device_model::FakeLanguageModel()));
  model_info_ = TestModelInfoBuilder()
                    .SetModelFilePath(model_path)
                    .SetVersion(123)
                    .Build();
}
FakeLanguageModelAsset::~FakeLanguageModelAsset() = default;

base::FilePath FakeLanguageModelAsset::model_path() const {
  return temp_dir_.GetPath().Append(kWeightsFile);
}

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
