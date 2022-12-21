// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_store.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

constexpr size_t kBytesPerMegabyte = 1024 * 1024;

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

// Parses the OptimizationTarget from the string.
proto::OptimizationTarget ParseOptimizationTargetFromString(
    const std::string& optimization_target_str) {
  int optimization_target;
  if (!base::StringToInt(optimization_target_str, &optimization_target)) {
    return proto::OPTIMIZATION_TARGET_UNKNOWN;
  }
  if (!proto::OptimizationTarget_IsValid(optimization_target)) {
    return proto::OPTIMIZATION_TARGET_UNKNOWN;
  }
  return static_cast<proto::OptimizationTarget>(optimization_target);
}

void RecordModelStorageMetrics(const base::FilePath& base_store_dir) {
  base::FileEnumerator enumerator(base_store_dir, false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath optimization_target_dir = enumerator.Next();
       !optimization_target_dir.empty();
       optimization_target_dir = enumerator.Next()) {
    proto::OptimizationTarget optimization_target =
        ParseOptimizationTargetFromString(
            optimization_target_dir.BaseName().AsUTF8Unsafe());
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN) {
      continue;
    }
    size_t total_models = 0;
    base::FileEnumerator models_enumerator(optimization_target_dir, false,
                                           base::FileEnumerator::DIRECTORIES);
    for (base::FilePath model_dir = models_enumerator.Next();
         !model_dir.empty(); model_dir = models_enumerator.Next()) {
      total_models++;
    }
    base::UmaHistogramCounts100(
        "OptimizationGuide.PredictionModelStore.ModelCount." +
            GetStringNameForOptimizationTarget(optimization_target),
        total_models);
    base::UmaHistogramMemoryMB(
        "OptimizationGuide.PredictionModelStore.TotalDirectorySize." +
            GetStringNameForOptimizationTarget(optimization_target),
        base::ComputeDirectorySize(optimization_target_dir) /
            kBytesPerMegabyte);
  }
}

}  // namespace

// static
PredictionModelStore* PredictionModelStore::GetInstance() {
  static base::NoDestructor<PredictionModelStore> model_store;
  return model_store.get();
}

PredictionModelStore::PredictionModelStore()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {
  DCHECK(optimization_guide::features::IsInstallWideModelStoreEnabled());
}

PredictionModelStore::~PredictionModelStore() = default;

void PredictionModelStore::Initialize(PrefService* local_state,
                                      const base::FilePath& base_store_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state);
  DCHECK(!base_store_dir.empty());

  // Should not be initialized already.
  DCHECK(!local_state_);
  DCHECK(base_store_dir_.empty());

  local_state_ = local_state;
  base_store_dir_ = base_store_dir;

  PurgeInactiveModels();

  // Clean up any model files that were slated for deletion in previous
  // sessions.
  CleanUpOldModelFiles();

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RecordModelStorageMetrics, base_store_dir_));
}

// static
std::unique_ptr<PredictionModelStore>
PredictionModelStore::CreatePredictionModelStoreForTesting(
    PrefService* local_state,
    const base::FilePath& base_store_dir) {
  auto store = base::WrapUnique(new PredictionModelStore());
  store->Initialize(local_state, base_store_dir);
  return store;
}

bool PredictionModelStore::HasModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto metadata = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state_, optimization_target, model_cache_key);
  if (!metadata) {
    return false;
  }
  // Check the existence of model dir as an indication of validity.
  return metadata->GetModelBaseDir().has_value();
}

bool PredictionModelStore::HasModelWithVersion(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    int64_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto metadata = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      local_state_, optimization_target, model_cache_key);
  if (!metadata) {
    return false;
  }
  auto actual_version = metadata->GetVersion();
  if (!actual_version) {
    RemoveModel(optimization_target, model_cache_key,
                PredictionModelStoreModelRemovalReason::kModelVersionInvalid);
    return false;
  }
  return *actual_version == version;
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
    RemoveModel(
        optimization_target, model_cache_key,
        PredictionModelStoreModelRemovalReason::kModelExpiredOnLoadModel);
    std::move(callback).Run(nullptr);
    return;
  }
  auto base_model_dir = metadata->GetModelBaseDir();
  if (!base_model_dir) {
    RemoveModel(optimization_target, model_cache_key,
                PredictionModelStoreModelRemovalReason::kInvalidModelDir);
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
    RemoveModel(optimization_target, model_cache_key,
                PredictionModelStoreModelRemovalReason::kModelLoadFailed);
    std::move(callback).Run(nullptr);
    return;
  }
  std::move(callback).Run(std::move(model));
}

void PredictionModelStore::UpdateMetadataForExistingModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    const proto::ModelInfo& model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_info.has_version());
  DCHECK_EQ(optimization_target, model_info.optimization_target());

  if (!HasModel(optimization_target, model_cache_key))
    return;

  ModelStoreMetadataEntryUpdater metadata(local_state_, optimization_target,
                                          model_cache_key);
  auto base_model_dir = metadata.GetModelBaseDir();
  DCHECK(base_store_dir_.IsParent(*base_model_dir));
  metadata.SetVersion(model_info.version());
  if (model_info.has_valid_duration()) {
    metadata.SetExpiryTime(
        base::Time::Now() +
        base::Seconds(model_info.valid_duration().seconds()));
  }
  metadata.SetKeepBeyondValidDuration(model_info.keep_beyond_valid_duration());
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
  metadata.SetVersion(model_info.version());
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
    RemoveModel(optimization_target, model_cache_key,
                PredictionModelStoreModelRemovalReason::
                    kModelUpdateFilePathVerifyFailed);
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

void PredictionModelStore::UpdateModelCacheKeyMapping(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& client_model_cache_key,
    const proto::ModelCacheKey& server_model_cache_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      local_state_, optimization_target, client_model_cache_key,
      server_model_cache_key);
}

void PredictionModelStore::RemoveModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    PredictionModelStoreModelRemovalReason model_remove_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!local_state_) {
    return;
  }

  RecordPredictionModelStoreModelRemovalVersionHistogram(model_remove_reason);
  ModelStoreMetadataEntryUpdater metadata(local_state_, optimization_target,
                                          model_cache_key);
  auto base_model_dir = metadata.GetModelBaseDir();
  if (base_model_dir) {
    DCHECK(base_store_dir_.IsParent(*base_model_dir));
    ScopedDictPrefUpdate pref_update(
        local_state_, prefs::localstate::kStoreFilePathsToDelete);
    pref_update->Set(FilePathToString(*base_model_dir), true);
  }
  // Continue removing the metadata even if the model dirs does not exist.
  metadata.ClearMetadata();
}

void PredictionModelStore::PurgeInactiveModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);
  for (const auto& expired_model_dir :
       ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(local_state_)) {
    DCHECK(base_store_dir_.IsParent(expired_model_dir));
    // This is called at startup. So no need to schedule the deletion of the
    // model dirs, and instead can be deleted immediately.
    background_task_runner_->PostTask(
        FROM_HERE, base::GetDeletePathRecursivelyCallback(expired_model_dir));
  }
}

void PredictionModelStore::CleanUpOldModelFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);
  for (const auto entry :
       local_state_->GetDict(prefs::localstate::kStoreFilePathsToDelete)) {
    auto path_to_delete = StringToFilePath(entry.first);
    DCHECK(path_to_delete);
    DCHECK(base_store_dir_.IsParent(*path_to_delete));
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&base::DeletePathRecursively, *path_to_delete),
        base::BindOnce(&PredictionModelStore::OnFilePathDeleted,
                       weak_ptr_factory_.GetWeakPtr(), entry.first));
  }
}

void PredictionModelStore::OnFilePathDeleted(const std::string& path_to_delete,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(local_state_);
  if (!success) {
    // Try to delete again later.
    return;
  }

  ScopedDictPrefUpdate pref_update(local_state_,
                                   prefs::localstate::kStoreFilePathsToDelete);
  pref_update->Remove(path_to_delete);
}

}  // namespace optimization_guide
