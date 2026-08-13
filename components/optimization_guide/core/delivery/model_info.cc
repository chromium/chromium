// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_info.h"

#include <memory>
#include <vector>

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

  std::vector<base::FilePath> additional_files;
  additional_files.reserve(model.model_info().additional_files_size());
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
    additional_files.push_back(std::move(*additional_file_path));
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

std::optional<ModelInfo> LoadAndVerifyModelOffThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir) {
  TRACE_EVENT("optimization_guide", "LoadAndVerifyModelOffThread", "target",
              GetStringNameForOptimizationTarget(optimization_target));

  auto model_info = ParseModelInfoFromFile(
      base_model_dir.Append(GetBaseFileNameForModelInfo()));
  if (!model_info || !model_info->has_version()) {
    return std::nullopt;
  }
  DCHECK_EQ(optimization_target, model_info->optimization_target());
  // Make sure the model file, the full modelinfo file and all additional
  // files still exist.
  auto file_paths_to_check = GetModelFilePaths(*model_info, base_model_dir);
  if (!CheckAllPathsExist(file_paths_to_check)) {
    return std::nullopt;
  }

  std::vector<base::FilePath> additional_files;
  additional_files.reserve(model_info->additional_files_size());
  for (const proto::AdditionalModelFile& additional_file :
       model_info->additional_files()) {
    std::optional<base::FilePath> additional_file_path =
        StringToFilePath(additional_file.file_path());
    if (!additional_file_path) {
      continue;
    }
    if (!additional_file_path->IsAbsolute()) {
      additional_file_path = base_model_dir.Append(*additional_file_path);
    }
    additional_files.push_back(std::move(*additional_file_path));
  }

  std::optional<proto::Any> model_metadata;
  if (model_info->has_model_metadata()) {
    model_metadata = model_info->model_metadata();
  }

  return ModelInfo{
      .model_file_path = base_model_dir.Append(GetBaseFileNameForModels()),
      .additional_files = std::move(additional_files),
      .version = model_info->version(),
      .model_metadata = std::move(model_metadata),
  };
}
}  // namespace optimization_guide
