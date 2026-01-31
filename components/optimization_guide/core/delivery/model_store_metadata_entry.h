// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_STORE_METADATA_ENTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_STORE_METADATA_ENTRY_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

class PrefService;

namespace optimization_guide {

struct ClientCacheKey {
  std::string hexhash;
  static ClientCacheKey FromLocale(std::string);
};

// A read-only view of the metadata in prefs about a model on disk.
class ModelStoreMetadataEntry {
 public:
  // This is the default duration for models that do not specify retention.
  static constexpr base::TimeDelta kDefaultStoredModelValidDuration =
      base::Days(30);

  explicit ModelStoreMetadataEntry(const base::DictValue* metadata_entry);
  ModelStoreMetadataEntry(const ModelStoreMetadataEntry&) = default;
  ModelStoreMetadataEntry& operator=(const ModelStoreMetadataEntry&) = delete;
  ~ModelStoreMetadataEntry();

  // Gets the model base dir where the model files, its additional files
  // and the model info files are stored.
  std::optional<base::FilePath> GetModelBaseDir() const;

  // Gets the model version.
  std::optional<int64_t> GetVersion() const;

  // Gets the expiry time.
  base::Time GetExpiryTime() const;

  // Gets whether the model should be kept beyond the expiry duration.
  bool GetKeepBeyondValidDuration() const;

 private:
  // The root metadata entry for this model.
  raw_ptr<const base::DictValue> metadata_entry_;
};

// The pref updater for ModelStoreMetadataEntry.
class ModelStoreMetadataEntryUpdater {
 public:
  // Returns the metadata entry in the store, creating it if it does not exist.
  ModelStoreMetadataEntryUpdater(PrefService& local_state,
                                 proto::OptimizationTarget optimization_target,
                                 const std::string& server_key_hash);

  ModelStoreMetadataEntryUpdater(const ModelStoreMetadataEntryUpdater&) =
      delete;
  ModelStoreMetadataEntryUpdater& operator=(
      const ModelStoreMetadataEntryUpdater&) = delete;

  // The setters for the various model metadata.
  void SetModelBaseDir(base::FilePath model_base_dir);
  void SetVersion(int64_t version);
  void SetKeepBeyondValidDuration(bool keep_beyond_valid_duration);
  void SetExpiryTime(base::Time expiry_time);

  // Clear metadata for the model entry.
  void ClearMetadata();

  const ModelStoreMetadataEntry entry() const {
    return ModelStoreMetadataEntry(entry_);
  }

 private:
  ScopedDictPrefUpdate pref_updater_;
  // The part of the Value owned by |pref_updater_| which backs the entry
  // to be updated.
  raw_ptr<base::DictValue> entry_;
};

// A ledger that tracks what models should be stored on disk.
class ModelStoreLedger {
 public:
  explicit ModelStoreLedger(PrefService& local_state);
  ~ModelStoreLedger();

  // Returns the metadata entry in the store if it exists.
  std::optional<ModelStoreMetadataEntry> GetEntryIfExists(
      proto::OptimizationTarget optimization_target,
      const ClientCacheKey& model_cache_key) const;

  // Returns the valid model dirs in the model store base dir, that were in sync
  // with the `local_state`.
  std::set<base::FilePath> GetValidModelDirs() const;

  // Creates a scoped updater to update one model's entry.
  // Creates the entry if it does not exist.
  ModelStoreMetadataEntryUpdater UpdateEntry(
      proto::OptimizationTarget optimization_target,
      const ClientCacheKey& model_cache_key);

  // Updates the mapping of |client_model_cache_key| to |server_model_cache_key|
  // for |optimization_target| in |local_state|.
  void UpdateModelCacheKeyMapping(
      proto::OptimizationTarget optimization_target,
      const ClientCacheKey& client_model_cache_key,
      const proto::ModelCacheKey& server_model_cache_key);

  // Removes all the model metadata entries that are considered inactive, such
  // as expired models, models unused for a long time, and returns the model
  // dirs of the removed entries.
  // TODO(crbug.com/244649670): Remove models that are unused for a long time.
  std::vector<base::FilePath> PurgeAllInactiveMetadata();

  // Record that a model should be deleted on next restart.
  void AddPathToDelete(base::FilePath path);

  // Get all the deferred deletions, keys are paths, values don't matter.
  const base::DictValue& GetPathsToDelete() const;

  // Remove a deferred deletion.
  void RemovePathToDelete(base::FilePath path);

 private:
  raw_ref<PrefService> local_state_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_DELIVERY_MODEL_STORE_METADATA_ENTRY_H_
