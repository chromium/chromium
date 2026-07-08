// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_info.h"

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/model_util.h"

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
  if (!model_file_path) {
    return nullptr;
  }
  if (!model.model_info().has_version()) {
    return nullptr;
  }

  base::flat_map<base::FilePath::StringType, base::FilePath> additional_files;
  for (const proto::AdditionalModelFile& additional_file :
       model.model_info().additional_files()) {
    std::optional<base::FilePath> additional_file_path =
        StringToFilePath(additional_file.file_path());
    if (!additional_file_path) {
      continue;
    }
    if (!additional_file_path->IsAbsolute()) {
      NOTREACHED() << FilePathToString(*additional_file_path);
    }
    additional_files[additional_file_path->BaseName().value()] =
        *additional_file_path;
  }

  std::optional<proto::Any> model_metadata;
  if (model.model_info().has_model_metadata()) {
    model_metadata = model.model_info().model_metadata();
  }

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
  if (auto it = additional_files_.find(base_name);
      it != additional_files_.end()) {
    return it->second;
  }
  return std::nullopt;
}

int64_t ModelInfo::GetVersion() const {
  return version_;
}

std::optional<proto::Any> ModelInfo::GetModelMetadata() const {
  return model_metadata_;
}

std::unique_ptr<proto::PredictionModel> LoadAndVerifyModelOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir) {
  TRACE_EVENT("optimization_guide", "LoadAndVerifyModelOffThread", "target",
              GetStringNameForOptimizationTarget(optimization_target));

  auto model_info = ParseModelInfoFromFile(
      base_model_dir.Append(GetBaseFileNameForModelInfo()));
  if (!model_info) {
    return nullptr;
  }
  DCHECK_EQ(optimization_target, model_info->optimization_target());
  // Make sure the model file, the full modelinfo file and all additional
  // files still exist.
  auto file_paths_to_check = GetModelFilePaths(*model_info, base_model_dir);
  if (!CheckAllPathsExist(file_paths_to_check)) {
    return nullptr;
  }
  std::unique_ptr<proto::PredictionModel> model =
      std::make_unique<proto::PredictionModel>();
  *model->mutable_model_info() = *model_info;
  model->mutable_model()->set_download_url(
      FilePathToString(base_model_dir.Append(GetBaseFileNameForModels())));

  // Convert the additional files to absolute paths.
  model->mutable_model_info()->clear_additional_files();
  for (const auto& additional_file : model_info->additional_files()) {
    auto additional_filepath = StringToFilePath(additional_file.file_path());
    if (!additional_filepath->IsAbsolute()) {
      additional_filepath = base_model_dir.Append(*additional_filepath);
    }
    model->mutable_model_info()->add_additional_files()->set_file_path(
        FilePathToString(*additional_filepath));
  }
  return model;
}

std::unique_ptr<ModelInfo> LoadAndVerifyModelInfoOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir) {
  std::unique_ptr<proto::PredictionModel> model =
      LoadAndVerifyModelOffThread(optimization_target, base_model_dir);
  return model ? ModelInfo::Create(*model) : nullptr;
}

}  // namespace optimization_guide
