// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/test_model_info_builder.h"

#include "components/optimization_guide/core/delivery/model_util.h"

namespace optimization_guide {

#if BUILDFLAG(IS_WIN)
const char kTestAbsoluteFilePath[] = "C:\\absolute\\file\\path";
#else
const char kTestAbsoluteFilePath[] = "/absolutefilepath";
#endif

TestModelInfoBuilder::TestModelInfoBuilder() {
  // Valid (dummy values) by default.
  model_.mutable_model()->set_download_url(kTestAbsoluteFilePath);
  model_.mutable_model_info()->set_version(123);
}
TestModelInfoBuilder::TestModelInfoBuilder(const ModelInfo& model_info) {
  SetModelFilePath(model_info.GetModelFilePath());
  SetVersion(model_info.GetVersion());
  SetModelMetadata(model_info.GetModelMetadata());
  SetAdditionalFiles(model_info.GetAdditionalFiles());
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

TestModelInfoBuilder& TestModelInfoBuilder::RemoveAdditionalFileWithBasename(
    const base::FilePath::StringType& base_name) {
  auto* files = model_.mutable_model_info()->mutable_additional_files();
  for (auto it = files->begin(); it != files->end(); it++) {
    std::optional<base::FilePath> path = StringToFilePath(it->file_path());
    if (path && path->BaseName().value() == base_name) {
      files->erase(it);
      return *this;
    }
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
