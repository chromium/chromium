// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_info.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/model_util.h"

namespace optimization_guide {

ModelInfo::ModelInfo(const base::FilePath& model_file_path,
                     const base::flat_map<base::FilePath::StringType,
                                          base::FilePath>& additional_files,
                     const int64_t version,
                     const std::optional<proto::Any>& model_metadata)
    : model_file_path_(model_file_path),
      additional_files_(additional_files),
      version_(version),
      model_metadata_(model_metadata) {}

ModelInfo::~ModelInfo() = default;
ModelInfo::ModelInfo(const ModelInfo&) = default;

// static
std::unique_ptr<ModelInfo> ModelInfo::Create(
    const proto::PredictionModel& model) {
  std::optional<base::FilePath> model_file_path =
      StringToFilePath(model.model().download_url());
  if (!model_file_path)
    return nullptr;
  if (!model.model_info().has_version())
    return nullptr;

  base::flat_map<base::FilePath::StringType, base::FilePath> additional_files;
  for (const proto::AdditionalModelFile& additional_file :
       model.model_info().additional_files()) {
    std::optional<base::FilePath> additional_file_path =
        StringToFilePath(additional_file.file_path());
    if (!additional_file_path) {
      continue;
    }
    if (!additional_file_path->IsAbsolute()) {
      NOTREACHED_IN_MIGRATION() << FilePathToString(*additional_file_path);
      continue;
    }
    additional_files[additional_file_path->BaseName().value()] =
        *additional_file_path;
  }

  std::optional<proto::Any> model_metadata;
  if (model.model_info().has_model_metadata())
    model_metadata = model.model_info().model_metadata();

  // Private ctor, so we can't use std::make_unique.
  return base::WrapUnique(new ModelInfo(*model_file_path, additional_files,
                                        model.model_info().version(),
                                        model_metadata));
}

base::FilePath ModelInfo::GetModelFilePath() const {
  return model_file_path_;
}

base::flat_set<base::FilePath> ModelInfo::GetAdditionalFiles() const {
  base::flat_set<base::FilePath> files;
  for (auto it = additional_files_.begin(); it != additional_files_.end();
       it++) {
    files.insert(it->second);
  }
  return files;
}

std::optional<base::FilePath> ModelInfo::GetAdditionalFileWithBaseName(
    const base::FilePath::StringType& base_name) const {
  if (base::Contains(additional_files_, base_name)) {
    return additional_files_.at(base_name);
  }
  return std::nullopt;
}

int64_t ModelInfo::GetVersion() const {
  return version_;
}

std::optional<proto::Any> ModelInfo::GetModelMetadata() const {
  return model_metadata_;
}

}  // namespace optimization_guide
