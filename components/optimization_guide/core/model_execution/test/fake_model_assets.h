// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/proto/on_device_model_execution_config.pb.h"
#include "components/optimization_guide/proto/text_safety_model_metadata.pb.h"

namespace optimization_guide {

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

// Constructs paths for extra files required by safety model.
// These files won't support actual reads and writes, but must be specified
// for the ModelInfo to pass validation.
base::flat_set<base::FilePath> FakeSafetyModelAdditionalFiles();

// Constructs a ModelInfo object holding the feature_config in metadata that
// should pass as a valid safety model.
std::unique_ptr<ModelInfo> FakeSafetyModelInfo(
    proto::FeatureTextSafetyConfiguration&& feature_config);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_EXECUTION_TEST_FAKE_MODEL_ASSETS_H_
