// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_info.h"

#include <memory>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/optimization_guide/core/delivery/model_util.h"

namespace optimization_guide {

// static
std::optional<ModelInfo> ModelInfo::CreateFromProto(
    const proto::PredictionModel& model) {
  std::optional<base::FilePath> model_file_path =
      StringToFilePath(model.model().download_url());
  if (!model_file_path) {
    return std::nullopt;
  }
  if (!model.model_info().has_version()) {
    return std::nullopt;
  }

  base::flat_set<base::FilePath> additional_files;
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
    additional_files.insert(std::move(*additional_file_path));
  }

  std::optional<proto::Any> model_metadata;
  if (model.model_info().has_model_metadata()) {
    model_metadata = model.model_info().model_metadata();
  }

  return ModelInfo{
      .model_file_path = *model_file_path,
      .additional_files = std::move(additional_files),
      .version = model.model_info().version(),
      .model_metadata = std::move(model_metadata),
  };
}

std::optional<base::FilePath> ModelInfo::GetAdditionalFileWithBaseName(
    const base::FilePath::StringType& base_name) const {
  for (const auto& file : additional_files) {
    if (file.BaseName().value() == base_name) {
      return file;
    }
  }
  return std::nullopt;
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

std::optional<ModelInfo> LoadAndVerifyModelInfoOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir) {
  std::unique_ptr<proto::PredictionModel> model =
      LoadAndVerifyModelOffThread(optimization_target, base_model_dir);
  return model ? ModelInfo::CreateFromProto(*model) : std::nullopt;
}
}  // namespace optimization_guide
