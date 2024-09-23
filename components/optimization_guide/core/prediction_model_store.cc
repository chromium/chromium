// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/prediction_model_store.h"

#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/task/thread_pool.h"
#include "base/uuid.h"
#include "components/optimization_guide/core/model_store_metadata_entry.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

constexpr size_t kBytesPerMegabyte = 1024 * 1024;

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
    if (!additional_filepath) {
      continue;
    }
    if (!additional_filepath->IsAbsolute()) {
      model_file_paths.emplace_back(
          base_model_dir.Append(*additional_filepath));
    } else {
      // In older versions (<=127), additional files had absolute path in model
      // info in the store. For backward compatibility, allow the absolute path
      // to be used. This can be changed to
      // `CHECK(!additional_filepath->IsAbsolute())` after a few of
      // milestones.
      model_file_paths.emplace_back(*additional_filepath);
    }
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

void RemoveInvalidModelDirs(const base::FilePath& base_store_dir,
                            std::set<base::FilePath> valid_model_dirs) {
  std::vector<base::FilePath> invalid_model_dirs;
  base::FileEnumerator enumerator(base_store_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath optimization_target_dir = enumerator.Next();
       !optimization_target_dir.empty();
       optimization_target_dir = enumerator.Next()) {
    proto::OptimizationTarget optimization_target =
        ParseOptimizationTargetFromString(
            optimization_target_dir.BaseName().AsUTF8Unsafe());
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN) {
      // Remove the unknown dirs within the model store dir. This can
      // potentially happen when the opt target is deprecated, and marked as
      // reserved.
      invalid_model_dirs.push_back(optimization_target_dir);
      RecordPredictionModelStoreModelRemovalVersionHistogram(
          proto::OPTIMIZATION_TARGET_UNKNOWN,
          PredictionModelStoreModelRemovalReason::kInconsistentModelDir);
      continue;
    }
    base::FileEnumerator model_cache_keys_enumerator(
        optimization_target_dir, false, base::FileEnumerator::DIRECTORIES);
    for (base::FilePath model_cache_key_dir =
             model_cache_keys_enumerator.Next();
         !model_cache_key_dir.empty();
         model_cache_key_dir = model_cache_keys_enumerator.Next()) {
      base::FileEnumerator models_enumerator(model_cache_key_dir,
                                             /*recursive=*/false,
                                             base::FileEnumerator::DIRECTORIES);
      for (base::FilePath model_dir = models_enumerator.Next();
           !model_dir.empty(); model_dir = models_enumerator.Next()) {
        DCHECK(model_dir.IsAbsolute());
        if (valid_model_dirs.find(ConvertToRelativePath(
                base_store_dir, model_dir)) == valid_model_dirs.end()) {
          invalid_model_dirs.push_back(model_dir);
          RecordPredictionModelStoreModelRemovalVersionHistogram(
              optimization_target,
              PredictionModelStoreModelRemovalReason::kInconsistentModelDir);
        }
      }
    }
  }
  // The invalid dirs can be removed immediately, since this is called at init.
  for (const auto& invalid_model_dir : invalid_model_dirs) {
    DCHECK(invalid_model_dir.IsAbsolute());
    base::DeletePathRecursively(invalid_model_dir);
  }
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

PredictionModelStore::PredictionModelStore()
    : background_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT})) {}

PredictionModelStore::~PredictionModelStore() = default;

void PredictionModelStore::Initialize(const base::FilePath& base_store_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base_store_dir.empty());

  if (!background_task_runner_) {
    // In unit tests, to avoid leaking a task runner between test runs, the task
    // runner can be reset at the end of each test. In that case, we'll need to
    // recreate it here.
    background_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT});
  }

  // Should not be initialized already.
  DCHECK(base_store_dir_.empty());

  base_store_dir_ = base_store_dir;
  PurgeInactiveModels();

  // Clean up any model files that were slated for deletion in previous
  // sessions.
  CleanUpOldModelFiles();

  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RemoveInvalidModelDirs, base_store_dir_,
                                ModelStoreMetadataEntry::GetValidModelDirs(
                                    GetLocalState())));
  background_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&RecordModelStorageMetrics, base_store_dir_));
}

bool PredictionModelStore::HasModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto metadata = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      GetLocalState(), optimization_target, model_cache_key);
  if (!metadata) {
    return false;
  }
  // Model dir should exist and be a relative path.
  return metadata->GetModelBaseDir() &&
         !metadata->GetModelBaseDir()->IsAbsolute();
}

bool PredictionModelStore::HasModelWithVersion(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    int64_t version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto metadata = ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
      GetLocalState(), optimization_target, model_cache_key);
  if (!metadata) {
    return false;
  }
  if (!metadata->GetModelBaseDir() ||
      metadata->GetModelBaseDir()->IsAbsolute()) {
    // Model dir should exist and be a relative path.
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
      GetLocalState(), optimization_target, model_cache_key);
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
  if (!base_model_dir || base_model_dir->IsAbsolute()) {
    RemoveModel(optimization_target, model_cache_key,
                PredictionModelStoreModelRemovalReason::kInvalidModelDir);
    std::move(callback).Run(nullptr);
    return;
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &PredictionModelStore::LoadAndVerifyModelInBackgroundThread,
          optimization_target, base_store_dir_.Append(*base_model_dir)),
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

  if (!HasModel(optimization_target, model_cache_key)) {
    return;
  }

  ModelStoreMetadataEntryUpdater metadata(GetLocalState(), optimization_target,
                                          model_cache_key);
  DCHECK(!metadata.GetModelBaseDir()->IsAbsolute());
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

  ModelStoreMetadataEntryUpdater metadata(GetLocalState(), optimization_target,
                                          model_cache_key);
  metadata.SetVersion(model_info.version());
  metadata.SetExpiryTime(
      base::Time::Now() +
      (model_info.has_valid_duration()
           ? base::Seconds(model_info.valid_duration().seconds())
           : features::StoredModelsValidDuration()));
  metadata.SetKeepBeyondValidDuration(model_info.keep_beyond_valid_duration());

  auto old_model_dir = metadata.GetModelBaseDir();
  if (old_model_dir) {
    RecordPredictionModelStoreModelRemovalVersionHistogram(
        optimization_target,
        PredictionModelStoreModelRemovalReason::kNewModelUpdate);
    ScheduleModelDirRemoval(*old_model_dir);
  }
  metadata.SetModelBaseDir(
      ConvertToRelativePath(base_store_dir_, base_model_dir));

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
  return base_model_dir.AppendASCII(
      base::HexEncode(base::RandBytesAsVector(8)));
}

void PredictionModelStore::UpdateModelCacheKeyMapping(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& client_model_cache_key,
    const proto::ModelCacheKey& server_model_cache_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ModelStoreMetadataEntryUpdater::UpdateModelCacheKeyMapping(
      GetLocalState(), optimization_target, client_model_cache_key,
      server_model_cache_key);
}

void PredictionModelStore::RemoveModel(
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key,
    PredictionModelStoreModelRemovalReason model_remove_reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!GetLocalState()) {
    return;
  }

  RecordPredictionModelStoreModelRemovalVersionHistogram(optimization_target,
                                                         model_remove_reason);
  ModelStoreMetadataEntryUpdater metadata(GetLocalState(), optimization_target,
                                          model_cache_key);
  auto base_model_dir = metadata.GetModelBaseDir();
  if (base_model_dir) {
    ScheduleModelDirRemoval(*base_model_dir);
  }
  // Continue removing the metadata even if the model dirs does not exist.
  metadata.ClearMetadata();
}

void PredictionModelStore::ScheduleModelDirRemoval(
    const base::FilePath& base_model_dir) {
  // Backward compatibility: Model dirs were absolute in the earlier versions,
  // and it was only in experiment. The latest versions use relative paths.
  // Convert to absolute paths to save in the pref, since absolute dirs could
  // become non-existent if IOS Chrome upgrade changes the sandbox dirs.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base_model_dir.IsAbsolute() ||
         base_store_dir_.IsParent(base_model_dir));
  base::FilePath relative_model_dir =
      base_model_dir.IsAbsolute()
          ? ConvertToRelativePath(base_store_dir_, base_model_dir)
          : base_model_dir;
  ScopedDictPrefUpdate pref_update(GetLocalState(),
                                   prefs::localstate::kStoreFilePathsToDelete);
  pref_update->Set(FilePathToString(relative_model_dir), true);
}

void PredictionModelStore::PurgeInactiveModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetLocalState());
  for (const auto& expired_model_dir :
       ModelStoreMetadataEntryUpdater::PurgeAllInactiveMetadata(
           GetLocalState())) {
    // Backward compatibility: Model dirs were absolute in the earlier versions,
    // and it was only in experiment. The latest versions use relative paths.
    DCHECK(!expired_model_dir.IsAbsolute() ||
           base_store_dir_.IsParent(expired_model_dir));
    base::FilePath absolute_model_dir =
        expired_model_dir.IsAbsolute()
            ? expired_model_dir
            : base_store_dir_.Append(expired_model_dir);
    // This is called at startup. So no need to schedule the deletion of the
    // model dirs, and instead can be deleted immediately.
    background_task_runner_->PostTask(
        FROM_HERE, base::GetDeletePathRecursivelyCallback(absolute_model_dir));
  }
}

void PredictionModelStore::CleanUpOldModelFiles() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetLocalState());
  for (const auto entry :
       GetLocalState()->GetDict(prefs::localstate::kStoreFilePathsToDelete)) {
    // Backward compatibility: Model dirs were absolute in the earlier versions.
    // The latest versions use relative paths.
    auto path_to_delete = StringToFilePath(entry.first);
    DCHECK(path_to_delete);
    DCHECK(!path_to_delete->IsAbsolute() ||
           base_store_dir_.IsParent(*path_to_delete));
    base::FilePath absolute_path_to_delete =
        path_to_delete->IsAbsolute() ? *path_to_delete
                                     : base_store_dir_.Append(*path_to_delete);
    background_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&base::DeletePathRecursively, absolute_path_to_delete),
        base::BindOnce(&PredictionModelStore::OnFilePathDeleted,
                       weak_ptr_factory_.GetWeakPtr(), entry.first));
  }
}

void PredictionModelStore::OnFilePathDeleted(const std::string& path_to_delete,
                                             bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetLocalState());
  if (!success) {
    // Try to delete again later.
    return;
  }

  ScopedDictPrefUpdate pref_update(GetLocalState(),
                                   prefs::localstate::kStoreFilePathsToDelete);
  pref_update->Remove(path_to_delete);
}

base::FilePath PredictionModelStore::GetBaseStoreDirForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base_store_dir_;
}

void PredictionModelStore::ResetForTesting() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base_store_dir_ = base::FilePath();
  background_task_runner_.reset();
}

}  // namespace optimization_guide
