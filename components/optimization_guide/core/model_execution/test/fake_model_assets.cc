// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_execution/on_device_model_names.h"
#include "components/optimization_guide/core/model_execution/test/feature_config_builder.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "services/on_device_model/public/cpp/model_assets.h"
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

  // Writes a cache along with its fingerprint so it isn't invalidated when the
  // model assets are loaded.
  auto write_cache = [this](const base::FilePath::CharType* file_name,
                            std::string_view data,
                            std::string_view fingerprint) {
    base::FilePath path = temp_dir_.GetPath().Append(file_name);
    CHECK(base::WriteFile(path, data));
    CHECK(base::WriteFile(on_device_model::GetCacheFingerprintPath(path),
                          fingerprint));
  };

  const std::string_view weight_fingerprint =
      content.weight_cache_fingerprint ? content.weight_cache_fingerprint : "";
  if (content.cache_weight) {
    write_cache(kWeightCacheFile, base::NumberToString(content.cache_weight),
                weight_fingerprint);
  }
  if (content.encoder_cache_weight) {
    write_cache(kEncoderCacheFile,
                base::NumberToString(content.encoder_cache_weight),
                weight_fingerprint);
  }
  if (content.adapter_cache_weight) {
    write_cache(kAdapterCacheFile,
                base::NumberToString(content.adapter_cache_weight),
                weight_fingerprint);
  }
  if (content.mtp_drafter_cache_weight) {
    write_cache(kMtpDrafterCacheFile,
                base::NumberToString(content.mtp_drafter_cache_weight),
                weight_fingerprint);
  }

  const std::string_view dawn_version(
      reinterpret_cast<const char*>(dawnProcGetVersion()), 20);
  if (content.shader_cache_data && content.shader_cache_data[0] != '\0') {
    write_cache(kProgramCacheFile, content.shader_cache_data, dawn_version);
  }
  if (content.mtp_drafter_shader_cache_data &&
      content.mtp_drafter_shader_cache_data[0] != '\0') {
    write_cache(kMtpDrafterProgramCacheFile,
                content.mtp_drafter_shader_cache_data, dawn_version);
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

FakeAdaptationAsset::FakeAdaptationAsset(
    FakeAdaptationAsset::Content&& content) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  base::FilePath config_path =
      temp_dir_.GetPath().Append(kOnDeviceModelExecutionConfigFile);
  {
    proto::OnDeviceModelExecutionConfig config;
    *config.add_feature_configs() = content.config;
    CHECK(base::WriteFile(config_path, config.SerializeAsString()));
  }
  if (content.weight) {
    base::FilePath weights_path =
        temp_dir_.GetPath().Append(kOnDeviceModelAdaptationWeightsFile);
    CHECK(base::WriteFile(weights_path,
                          base::NumberToString(content.weight.value())));
  }
}
FakeAdaptationAsset::~FakeAdaptationAsset() = default;

}  // namespace optimization_guide
