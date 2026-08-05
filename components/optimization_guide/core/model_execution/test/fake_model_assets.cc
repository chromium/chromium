// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/on_device_features.h"
#include "components/optimization_guide/core/model_execution/on_device_model_adaptation_loader.h"
#include "components/optimization_guide/core/model_execution/on_device_model_component.h"
#include "components/optimization_guide/core/model_execution/on_device_model_feature_adapter.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/version_info/version_info.h"
#include "services/on_device_model/public/cpp/test_support/fake_service.h"
#include "third_party/dawn/include/dawn/dawn_proc.h"

namespace optimization_guide {

#if BUILDFLAG(IS_WIN)
const char kTestAbsoluteFilePath[] = "C:\\absolute\\file\\path";
#else
const char kTestAbsoluteFilePath[] = "/absolutefilepath";
#endif

FakeBaseModelAsset::FakeBaseModelAsset()
    : FakeBaseModelAsset(FakeBaseModelAsset::Content{}) {}
FakeBaseModelAsset::FakeBaseModelAsset(Content content) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  // Support all performance hints by default.
  supported_performance_hints_ =
      base::ListValue()
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
FakeBaseModelAsset::FakeBaseModelAsset(
    const std::vector<proto::OnDeviceModelPerformanceHint>& hints,
    Content content) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  for (const auto& hint : hints) {
    supported_performance_hints_.Append(hint);
  }
  Write(std::move(content));
}
FakeBaseModelAsset::~FakeBaseModelAsset() = default;

void FakeBaseModelAsset::Write(Content&& content) {
  CHECK(base::WriteFile(temp_dir_.GetPath().Append(kWeightsFile),
                        base::NumberToString(content.weight)));
  if (content.cache_weight) {
    CHECK(base::WriteFile(temp_dir_.GetPath().Append(kWeightCacheFile),
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
  if (content.shader_cache_data) {
    base::FilePath program_cache_path =
        temp_dir_.GetPath().Append(kProgramCacheFile);
    CHECK(base::WriteFile(program_cache_path,
                          std::string(content.shader_cache_data)));
    std::string_view dawn_rev(
        reinterpret_cast<const char*>(dawnProcGetVersion()), 20);
    // TODO(crbug.com/538727789): Remove Chrome version check once ML Drift
    // incorporates kernel revision versioning into fingerprint keys.
    std::string current_version =
        base::StrCat({version_info::GetVersionNumber(), "_", dawn_rev});
    CHECK(base::WriteFile(
        program_cache_path.AddExtension(FILE_PATH_LITERAL(".dawn_version")),
        current_version));
  }
  CHECK(base::WriteFile(
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile),
      content.config.SerializeAsString()));
}

base::DictValue FakeBaseModelAsset::Manifest() const {
  return base::DictValue().Set("BaseModelSpec",
                               base::DictValue()
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
  std::vector<base::FilePath> additional_files = {config_path};
  if (content.weight) {
    paths_ = std::make_unique<on_device_model::AdaptationAssetPaths>();
    paths_->weights =
        temp_dir_.GetPath().Append(kOnDeviceModelAdaptationWeightsFile);
    CHECK(base::WriteFile(paths_->weights,
                          base::NumberToString(content.weight.value())));
    additional_files.push_back(paths_->weights);
  }
  model_info_ = {
      .model_file_path = base::FilePath::FromUTF8Unsafe(kTestAbsoluteFilePath),
      .additional_files = std::move(additional_files),
      .version = version(),
      .model_metadata = AnyWrapProto(content.metadata),
  };
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
  model_info_ = {
      .model_file_path = model_path,
      .version = 123,
  };
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
  auto data_path =
      temp_dir_.GetPath().Append(FILE_PATH_LITERAL("model.tflite"));
  CHECK(base::WriteFile(data_path, on_device_model::FakeTsData()));
  model_info_ = {
      .model_file_path = data_path,
      .version = content.model_info_version,
      .model_metadata = AnyWrapProto(content.metadata),
  };
}

FakeSafetyModelAsset::~FakeSafetyModelAsset() = default;

}  // namespace optimization_guide
