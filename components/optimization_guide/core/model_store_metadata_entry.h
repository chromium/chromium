// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

class PrefService;

namespace optimization_guide {

class ModelStoreMetadataEntryUpdater;

// Encapsulates the lightweight metadata entry that is stored in local state
// prefs for one model in the model store. The model is represented by the key
// pair OptimizationTarget and hash of ModelCacheKey.
class ModelStoreMetadataEntry {
 public:
  // Returns the metadata entry in the store if it exists.
  static std::optional<ModelStoreMetadataEntry> GetModelMetadataEntryIfExists(
      PrefService* local_state,
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& model_cache_key);

  // Returns the valid model dirs in the model store base dir, that were in sync
  // with the `local_state`.
  static std::set<base::FilePath> GetValidModelDirs(PrefService* local_state);

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
  friend class ModelStoreMetadataEntryUpdater;

  explicit ModelStoreMetadataEntry(const base::Value::Dict* metadata_entry);

  void SetMetadataEntry(const base::Value::Dict* metadata_entry);

  // The root metadata entry for this model.
  raw_ptr<const base::Value::Dict> metadata_entry_;
};

// The pref updater for ModelStoreMetadataEntry.
class ModelStoreMetadataEntryUpdater : public ModelStoreMetadataEntry {
 public:
  // Updates the mapping of |client_model_cache_key| to |server_model_cache_key|
  // for |optimization_target| in |local_state|.
  static void UpdateModelCacheKeyMapping(
      PrefService* local_state,
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& client_model_cache_key,
      const proto::ModelCacheKey& server_model_cache_key);

  // Removes all the model metadata entries that are considered inactive, such
  // as expired models, models unused for a long time, and returns the model
  // dirs of the removed entries.
  // TODO(b/244649670): Remove models that are unused for a long time.
  static std::vector<base::FilePath> PurgeAllInactiveMetadata(
      PrefService* local_state);

  // Returns the metadata entry in the store, creating it if it does not exist.
  ModelStoreMetadataEntryUpdater(PrefService* local_state,
                                 proto::OptimizationTarget optimization_target,
                                 const proto::ModelCacheKey& model_cache_key);

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

 private:
  // The root metadata entry that is linked with the |pref_updater_|.
  raw_ptr<base::Value::Dict> metadata_entry_updater_;

  ScopedDictPrefUpdate pref_updater_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_
