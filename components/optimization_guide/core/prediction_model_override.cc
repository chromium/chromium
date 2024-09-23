// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_override.h"

#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/prediction_model_download_manager.h"
#include "components/services/unzip/public/cpp/unzip.h"

#if BUILDFLAG(IS_IOS)
#include "components/services/unzip/in_process_unzipper.h"  // nogncheck
#else
#include "components/services/unzip/content/unzip_service.h"  // nogncheck
#endif

namespace optimization_guide {

namespace {

void OnModelOverrideProcessed(OnPredictionModelBuiltCallback callback,
                              std::unique_ptr<proto::PredictionModel> model) {
  std::move(callback).Run(std::move(model));
}

std::unique_ptr<proto::PredictionModel> ProcessModelOverrideOnBGThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& unzipped_dir_path) {
  // Unpack and verify model info file.
  base::FilePath model_info_path =
      unzipped_dir_path.Append(GetBaseFileNameForModelInfo());
  std::string binary_model_info_pb;
  if (!base::ReadFileToString(model_info_path, &binary_model_info_pb)) {
    LOG(ERROR) << "Failed to read " << FilePathToString(model_info_path);
    return nullptr;
  }
  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info_pb)) {
    LOG(ERROR) << "Failed to parse " << FilePathToString(model_info_path);
    return nullptr;
  }

  if (!model_info.has_version() || !model_info.has_optimization_target()) {
    LOG(ERROR) << FilePathToString(model_info_path)
               << "is invalid because it does not contain a version and/or "
                  "optimization target";
    return nullptr;
  }

  for (int i = 0; i < model_info.additional_files_size(); i++) {
    proto::AdditionalModelFile* additional_file =
        model_info.mutable_additional_files(i);

    base::FilePath additional_file_basename =
        *StringToFilePath(additional_file->file_path());
    base::FilePath additional_file_absolute =
        unzipped_dir_path.Append(additional_file_basename);
    additional_file->set_file_path(FilePathToString(additional_file_absolute));
  }

  std::unique_ptr<proto::PredictionModel> model =
      std::make_unique<proto::PredictionModel>();
  *model->mutable_model_info() = model_info;
  model->mutable_model()->set_download_url(
      FilePathToString(unzipped_dir_path.Append(GetBaseFileNameForModels())));

  return model;
}

void OnModelOverrideUnzipped(proto::OptimizationTarget optimization_target,
                             const base::FilePath& base_model_dir,
                             OnPredictionModelBuiltCallback callback,
                             bool success) {
  if (!success) {
    LOG(ERROR) << FilePathToString(base_model_dir) << " failed to unzip";
    std::move(callback).Run(nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ProcessModelOverrideOnBGThread, optimization_target,
                     base_model_dir),
      base::BindOnce(&OnModelOverrideProcessed, std::move(callback)));
}

void OnModelOverrideVerified(proto::OptimizationTarget optimization_target,
                             const base::FilePath& passed_crx_file_path,
                             const base::FilePath& base_model_dir,
                             OnPredictionModelBuiltCallback callback,
                             bool is_verify_success) {
  if (!is_verify_success) {
    LOG(ERROR) << passed_crx_file_path << " failed verification";
    std::move(callback).Run(nullptr);
    return;
  }

#if BUILDFLAG(IS_IOS)
  auto unzipper = unzip::LaunchInProcessUnzipper();
#else
  auto unzipper = unzip::LaunchUnzipper();
#endif
  unzip::Unzip(std::move(unzipper), passed_crx_file_path, base_model_dir,
               unzip::mojom::UnzipOptions::New(), unzip::AllContents(),
               base::DoNothing(),
               base::BindOnce(&OnModelOverrideUnzipped, optimization_target,
                              base_model_dir, std::move(callback)));
}

}  // namespace

bool BuildPredictionModelFromCommandLineForOptimizationTarget(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir,
    OnPredictionModelBuiltCallback callback) {
  std::optional<std::pair<std::string, std::optional<proto::Any>>>
      model_file_path_and_metadata =
          GetModelOverrideForOptimizationTarget(optimization_target);
  if (!model_file_path_and_metadata) {
    std::move(callback).Run(nullptr);
    return false;
  }

  if (base::EndsWith(model_file_path_and_metadata->first, ".crx3")) {
    DVLOG(0) << "Attempting to parse the model override at "
             << model_file_path_and_metadata->first
             << " as a crx model package for "
             << GetStringNameForOptimizationTarget(optimization_target);
    if (model_file_path_and_metadata->second) {
      LOG(ERROR) << "Ignoring the metadata that was passed since a crx package "
                    "was given";
    }

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(PredictionModelDownloadManager::VerifyDownload,
                       *StringToFilePath(model_file_path_and_metadata->first),
                       base_model_dir,
                       /*delete_file_on_error=*/false),
        base::BindOnce(&OnModelOverrideVerified, optimization_target,
                       *StringToFilePath(model_file_path_and_metadata->first),
                       base_model_dir, std::move(callback)));
    return true;
  }

  std::unique_ptr<proto::PredictionModel> prediction_model =
      std::make_unique<proto::PredictionModel>();
  prediction_model->mutable_model_info()->set_optimization_target(
      optimization_target);
  prediction_model->mutable_model_info()->set_version(123);
  if (model_file_path_and_metadata->second) {
    *prediction_model->mutable_model_info()->mutable_model_metadata() =
        model_file_path_and_metadata->second.value();
  }
  prediction_model->mutable_model()->set_download_url(
      model_file_path_and_metadata->first);
  std::move(callback).Run(std::move(prediction_model));
  return true;
}

}  // namespace optimization_guide
