// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/test/fake_model_assets.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"

namespace optimization_guide {

namespace {

base::FilePath UnusedTestDir() {
#if BUILDFLAG(IS_WIN)
  return base::FilePath(FILE_PATH_LITERAL("C:\\unused\\test\\path"));
#else
  return base::FilePath(FILE_PATH_LITERAL("/unused/test/path"));
#endif
}

}  // namespace

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

base::flat_set<base::FilePath> FakeSafetyModelAdditionalFiles() {
  return {
      UnusedTestDir().Append(kTsDataFile),
      UnusedTestDir().Append(kTsSpModelFile),
  };
}

std::unique_ptr<ModelInfo> FakeSafetyModelInfo(
    proto::FeatureTextSafetyConfiguration&& feature_config) {
  proto::TextSafetyModelMetadata model_metadata;
  *model_metadata.add_feature_text_safety_configurations() = feature_config;
  proto::Any any;
  any.set_type_url(
      "type.googleapis.com/optimization_guide.proto.TextSafetyModelMetadata");
  model_metadata.SerializeToString(any.mutable_value());
  return TestModelInfoBuilder()
      .SetAdditionalFiles(FakeSafetyModelAdditionalFiles())
      .SetModelMetadata(any)
      .Build();
}

}  // namespace optimization_guide
