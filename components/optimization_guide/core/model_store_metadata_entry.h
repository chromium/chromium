// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
class Time;
}  // namespace base

class PrefService;

namespace optimization_guide {

// Encapsulates the lightweight metadata entry that is stored in local state
// prefs for one model in the model store. The model is represented by the key
// pair OptimizationTarget and hash of ModelCacheKey.
class ModelStoreMetadataEntry {
 public:
  // Returns the metadata entry in the store if it exists.
  static absl::optional<ModelStoreMetadataEntry> GetModelMetadataEntryIfExists(
      PrefService* local_state,
      proto::OptimizationTarget optimization_target,
      const proto::ModelCacheKey& model_cache_key);

  ModelStoreMetadataEntry& operator=(const ModelStoreMetadataEntry&) = delete;
  ~ModelStoreMetadataEntry();

  // Gets the model base dir where the model files, its additional files
  // and the model info files are stored.
  absl::optional<base::FilePath> GetModelBaseDir() const;

  // Gets the expiry time.
  base::Time GetExpiryTime() const;

  // Gets whether the model should be kept beyond the expiry duration.
  bool GetKeepBeyondValidDuration() const;

 protected:
  explicit ModelStoreMetadataEntry(const base::Value::Dict* metadata_entry);

  void SetMetadataEntry(const base::Value::Dict* metadata_entry);

 private:
  // The root metadata entry for this model.
  const base::Value::Dict* metadata_entry_;
};

// The pref updater for ModelStoreMetadataEntry.
class ModelStoreMetadataEntryUpdater : public ModelStoreMetadataEntry {
 public:
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
  void SetKeepBeyondValidDuration(bool keep_beyond_valid_duration);
  void SetExpiryTime(base::Time expiry_time);

 private:
  // The root metadata entry that is linked with the |pref_updater_|.
  raw_ptr<base::Value::Dict> metadata_entry_updater_;

  ScopedDictPrefUpdate pref_updater_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_MODEL_STORE_METADATA_ENTRY_H_
