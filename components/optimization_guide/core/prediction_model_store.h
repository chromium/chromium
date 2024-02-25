// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_STORE_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_STORE_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/proto/models.pb.h"

class PrefService;

namespace optimization_guide {

// The new model store that manages the optimization guide prediction models.
// The model store is a key-value store where the optimization target,
// ModelCacheKey can be together considered as the key and the value is the
// model metadata and its files. For every model following are stored:
//   * The prediction model file and its additional files are stored in the
//   model dir.
//   * The full model metadata as model_info.pb in the model dir.
//   * Lightweight model metadata in the local state prefs, that is immediately
//   needed for managing the store, and to avoid the full metadata read.
// The model store is meant to be shared across profiles.
class PredictionModelStore {
 public:
  using PredictionModelLoadedCallback =
      base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>)>;

  PredictionModelStore();

  // Initializes the model store with |base_store_dir|. Model store will be
  // usable only after it is initialized.
  void Initialize(const base::FilePath& base_store_dir);

  PredictionModelStore(const PredictionModelStore&) = delete;
  PredictionModelStore& operator=(const PredictionModelStore&) = delete;
  virtual ~PredictionModelStore();

  // Initializes the model store with |local_state| and the |base_store_dir|, if
  // initialization hasn't happened already. Model store will be usable only
  // after it is initialized.

  // Returns whether the model represented by |optimization_target| and
  // |model_cache_key| is available in the store.
  bool HasModel(proto::OptimizationTarget optimization_target,
                const proto::ModelCacheKey& model_cache_key) const;

  // Returns whether the model represented by |optimization_target| and
  // |model_cache_key| with |version| is available in the store.
  bool HasModelWithVersion(proto::OptimizationTarget optimization_target,
                           const proto::ModelCacheKey& model_cache_key,
                           int64_t version);

  // Loads the model represented by |optimization_target| and
  // |model_cache_key|. Once the model is loaded and validated |callback|
  // is invoked. On any failures, callback is run with nullptr.
  void LoadModel(proto::OptimizationTarget optimization_target,
                 const proto::ModelCacheKey& model_cache_key,
                 PredictionModelLoadedCallback callback);

  // Update the model metadata for |model_info| if the model represented by
  // |optimization_target| and |model_cache_key| exists.
  void UpdateMetadataForExistingModel(
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& model_cache_key,
      const proto::ModelInfo& model_info);

  // Update the model for |model_info| in the store represented by
  // |optimization_target| and |model_cache_key|. The model files are stored in
  // |base_model_dir|. |callback| is invoked on completion. This will schedule
  // the old model files to be removed.
  void UpdateModel(proto::OptimizationTarget optimization_target,
                   const proto::ModelCacheKey& model_cache_key,
                   const proto::ModelInfo& model_info,
                   const base::FilePath& base_model_dir,
                   base::OnceClosure callback);

  // Returns the base model dir where the model files, full modelinfo, etc
  // should be stored, for the model represented by |optimization_target| and
  // |model_cache_key|.
  base::FilePath GetBaseModelDirForModelCacheKey(
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& model_cache_key);

  // Updates the mapping of |client_model_cache_key| to |server_model_cache_key|
  // for |optimization_target|.
  void UpdateModelCacheKeyMapping(
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& client_model_cache_key,
      const proto::ModelCacheKey& server_model_cache_key);

  // Removes the model represented by |optimization_target| and
  // |model_cache_key| from the store if it exists. The model metadata will be
  // removed immediately while the model directories will be slated for removal
  // at next startup, by CleanUpOldModelFiles.
  void RemoveModel(proto::OptimizationTarget optimization_target,
                   const proto::ModelCacheKey& model_cache_key,
                   PredictionModelStoreModelRemovalReason model_removal_reason);

  // Returns the local state that stores the prefs across all profiles.
  virtual PrefService* GetLocalState() const = 0;

  base::FilePath GetBaseStoreDirForTesting() const;

  // Allows tests to reset the store for subsequent tests since the store is a
  // singleton.
  void ResetForTesting();

 private:
  friend class PredictionModelStoreBrowserTestBase;

  // Loads the model and verifies if the model files exist and returns the
  // model. Otherwise nullptr is returned on any failures.
  static std::unique_ptr<proto::PredictionModel>
  LoadAndVerifyModelInBackgroundThread(
      proto::OptimizationTarget optimization_target,
      const base::FilePath& base_model_dir);

  // Invoked when the model loaded.
  void OnModelLoaded(proto::OptimizationTarget optimization_target,
                     const proto::ModelCacheKey& model_cache_key,
                     PredictionModelLoadedCallback callback,
                     std::unique_ptr<proto::PredictionModel> model);

  // Invoked when the model files are verified on a model update.
  void OnModelUpdateVerified(proto::OptimizationTarget optimization_target,
                             const proto::ModelCacheKey& model_cache_key,
                             base::OnceClosure callback,
                             bool model_paths_exist);

  // Schedules the removal of `base_model_dir` in the next Chrome session.
  void ScheduleModelDirRemoval(const base::FilePath& base_model_dir);

  // Removes all models that are considered inactive, such as expired models,
  // models unused for a long time. When models' |keep_beyond_valid_duration| is
  // set they are not treated as expired. This is called on startup, so the
  // model files can be deleted instantaneously.
  // TODO(b/244649670): Remove models that are unused for a long time.
  void PurgeInactiveModels();

  // Called at startup to remove the old model files slated for deletion in the
  // previous sessions.
  void CleanUpOldModelFiles();

  // Invoked when model files gets deleted.
  void OnFilePathDeleted(const std::string& path_to_delete, bool success);

  // The base dir where the prediction model dirs are saved.
  base::FilePath base_store_dir_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  // Background thread where file processing should be performed.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  base::WeakPtrFactory<PredictionModelStore> weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PREDICTION_MODEL_STORE_H_
