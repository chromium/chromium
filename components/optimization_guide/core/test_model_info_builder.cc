// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/test_model_info_builder.h"

#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"

namespace optimization_guide {

TestModelInfoBuilder::TestModelInfoBuilder() {
  // Valid (dummy values) by default.
  model_.mutable_model()->set_download_url(kTestAbsoluteFilePath);
  model_.mutable_model_info()->set_version(123);
}
TestModelInfoBuilder::~TestModelInfoBuilder() = default;

TestModelInfoBuilder& TestModelInfoBuilder::SetModelFilePath(
    const base::FilePath& file_path) {
  model_.mutable_model()->set_download_url(FilePathToString(file_path));
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetAdditionalFiles(
    const base::flat_set<base::FilePath>& additional_files) {
  for (const base::FilePath& file_path : additional_files) {
    model_.mutable_model_info()->add_additional_files()->set_file_path(
        FilePathToString(file_path));
  }
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetVersion(int64_t version) {
  model_.mutable_model_info()->set_version(version);
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetModelMetadata(
    std::optional<proto::Any> model_metadata) {
  if (!model_metadata) {
    model_.mutable_model_info()->clear_model_metadata();
    return *this;
  }
  *model_.mutable_model_info()->mutable_model_metadata() =
      model_metadata.value();
  return *this;
}

std::unique_ptr<ModelInfo> TestModelInfoBuilder::Build() {
  return ModelInfo::Create(model_);
}

}  // namespace optimization_guide
