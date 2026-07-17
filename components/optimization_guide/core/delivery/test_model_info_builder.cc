// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/test_model_info_builder.h"

#include <utility>
#include <vector>

#include "build/build_config.h"

namespace optimization_guide {

#if BUILDFLAG(IS_WIN)
const char kTestAbsoluteFilePath[] = "C:\\absolute\\file\\path";
#else
const char kTestAbsoluteFilePath[] = "/absolutefilepath";
#endif

TestModelInfoBuilder::TestModelInfoBuilder()
    : model_file_path_(base::FilePath::FromUTF8Unsafe(kTestAbsoluteFilePath)),
      version_(123) {}

TestModelInfoBuilder::TestModelInfoBuilder(const ModelInfo& model_info)
    : model_file_path_(model_info.model_file_path),
      additional_files_(model_info.additional_files),
      version_(model_info.version),
      model_metadata_(model_info.model_metadata) {}

TestModelInfoBuilder::~TestModelInfoBuilder() = default;
TestModelInfoBuilder::TestModelInfoBuilder(TestModelInfoBuilder&&) = default;
TestModelInfoBuilder& TestModelInfoBuilder::operator=(TestModelInfoBuilder&&) =
    default;

TestModelInfoBuilder& TestModelInfoBuilder::SetModelFilePath(
    const base::FilePath& file_path) {
  model_file_path_ = file_path;
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetAdditionalFiles(
    std::vector<base::FilePath> additional_files) {
  additional_files_ = std::move(additional_files);
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::RemoveAdditionalFileWithBasename(
    const base::FilePath::StringType& base_name) {
  std::erase_if(additional_files_, [&](const base::FilePath& path) {
    return path.BaseName().value() == base_name;
  });
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetVersion(int64_t version) {
  version_ = version;
  return *this;
}

TestModelInfoBuilder& TestModelInfoBuilder::SetModelMetadata(
    std::optional<proto::Any> model_metadata) {
  model_metadata_ = std::move(model_metadata);
  return *this;
}

ModelInfo TestModelInfoBuilder::Build() {
  return ModelInfo{
      .model_file_path = model_file_path_,
      .additional_files = additional_files_,
      .version = version_,
      .model_metadata = model_metadata_,
  };
}

}  // namespace optimization_guide
