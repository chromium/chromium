// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_store.h"

#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "model_store_metadata_entry.h"

namespace optimization_guide {

namespace {

// Returns the model info parsed from |model_info_path|.
absl::optional<proto::ModelInfo> ParseModelInfoFromFile(
    const base::FilePath& model_info_path) {
  std::string binary_model_info;
  if (!base::ReadFileToString(model_info_path, &binary_model_info))
    return absl::nullopt;

  proto::ModelInfo model_info;
  if (!model_info.ParseFromString(binary_model_info))
    return absl::nullopt;

  DCHECK(model_info.has_version());
  DCHECK(model_info.has_optimization_target());
  return model_info;
}

// Returns all the model file paths for the model |model_info| in
// |base_model_dir|.
std::vector<base::FilePath> GetModelFilePaths(
    const proto::ModelInfo& model_info,
    const base::FilePath& base_model_dir) {
  std::vector<base::FilePath> model_file_paths;
  model_file_paths.emplace_back(
      base_model_dir.Append(GetBaseFileNameForModels()));
  model_file_paths.emplace_back(
      base_model_dir.Append(GetBaseFileNameForModelInfo()));
  for (const auto& additional_file : model_info.additional_files()) {
    auto additional_filepath = StringToFilePath(additional_file.file_path());
    if (!additional_filepath)
      continue;
    DCHECK(base_model_dir.IsParent(*additional_filepath));
    model_file_paths.emplace_back(*additional_filepath);
  }
  return model_file_paths;
}

}  // namespace

PredictionModelStore::PredictionModelStore(PrefService* local_state,
                                           const base::FilePath& base_store_dir)
    : local_state_(local_state),
      base_store_dir_(base_store_dir),
      background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DCHECK(optimization_guide::features::IsInstallWideModelStoreEnabled());
  DCHECK(local_state);
  DCHECK(!base_store_dir.empty());
}

PredictionModelStore::~PredictionModelStore() = default;

bool PredictionModelStore::HasModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
             local_state_, optimization_target, model_cache_key)
      .has_value();
}

void PredictionModelStore::LoadModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    PredictionModelLoadedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto metadata = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state_, optimization_target, model_cache_key);
  if (!metadata) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!metadata->GetKeepBeyondValidDuration() &&
      metadata->GetExpiryTime() <= base::Time::Now()) {
    // TODO(b/244649670): Remove the invalid model.
    std::move(callback).Run(nullptr);
    return;
  }
  auto base_model_dir = metadata->GetModelBaseDir();
  if (!base_model_dir) {
    // TODO(b/244649670): Remove the invalid model.
    std::move(callback).Run(nullptr);
    return;
  }
  DCHECK(base_store_dir_.IsParent(*base_model_dir));

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &PredictionModelStore::LoadAndVerifyModelInBackgroundThread,
          optimization_target, *base_model_dir),
      base::BindOnce(&PredictionModelStore::OnModelLoaded,
                     weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     model_cache_key, std::move(callback)));
}

// static
std::unique_ptr<proto::PredictionModel>
PredictionModelStore::LoadAndVerifyModelInBackgroundThread(
    proto::OptimizationTarget optimization_target,
    const base::FilePath& base_model_dir) {
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

  return model;
}

void PredictionModelStore::OnModelLoaded(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    PredictionModelLoadedCallback callback,
    std::unique_ptr<proto::PredictionModel> model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model) {
    // TODO(b/244649670): Remove the invalid model.
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(std::move(model));
}

void PredictionModelStore::UpdateModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    const proto::ModelInfo& model_info,
    const base::FilePath& base_model_dir,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_info.has_version());
  DCHECK_EQ(optimization_target, model_info.optimization_target());
  DCHECK(base_store_dir_.IsParent(base_model_dir));

  ModelStoreMetadataEntryUpdater metadata(local_state_, optimization_target,
                                          model_cache_key);
  metadata.SetExpiryTime(
      base::Time::Now() +
      (model_info.has_valid_duration()
           ? base::Seconds(model_info.valid_duration().seconds())
           : features::StoredModelsValidDuration()));
  metadata.SetKeepBeyondValidDuration(model_info.keep_beyond_valid_duration());
  metadata.SetModelBaseDir(base_model_dir);

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CheckAllPathsExist,
                     GetModelFilePaths(model_info, base_model_dir)),
      base::BindOnce(&PredictionModelStore::OnModelUpdateVerified,
                     weak_ptr_factory_.GetWeakPtr(), optimization_target,
                     model_cache_key, std::move(callback)));
}

void PredictionModelStore::OnModelUpdateVerified(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    base::OnceClosure callback,
    bool model_paths_exist) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model_paths_exist) {
    // TODO(b/244649670): Remove the invalid model.
  }
  std::move(callback).Run();
}

base::FilePath PredictionModelStore::GetBaseModelDirForModelCacheKey(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(!base_store_dir_.empty());
  auto base_model_dir = base_store_dir_
                            .AppendASCII(base::NumberToString(
                                static_cast<int>(optimization_target)))
                            .AppendASCII(GetModelCacheKeyHash(model_cache_key));
  return base_model_dir.AppendASCII(base::HexEncode(
      base::as_bytes(base::make_span(base::RandBytesAsString(8)))));
}

}  // namespace optimization_guide
