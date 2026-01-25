// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/delivery/model_store_metadata_entry.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/delivery/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"

namespace optimization_guide {

namespace {

// Key names for the metadata entries.
const char kKeyModelBaseDir[] = "mbd";
const char kKeyVersion[] = "v";
const char kKeyExpiryTime[] = "et";
const char kKeyKeepBeyondValidDuration[] = "kbvd";

// Use |optimization_target| as (part of) a base::Dict key.
std::string AsKey(proto::OptimizationTarget optimization_target) {
  return base::NumberToString(static_cast<int>(optimization_target));
}

// Returns the hash of server returned ModelCacheKey for
// |client_model_cache_key| from |local_state|.
std::string GetServerModelCacheKeyHash(
    PrefService& local_state,
    proto::OptimizationTarget optimization_target,
    const ClientCacheKey& client_model_cache_key) {
  std::string client_model_cache_key_hash = client_model_cache_key.hexhash;
  auto* server_model_cache_key_hash =
      local_state.GetDict(prefs::localstate::kModelCacheKeyMapping)
          .FindString(AsKey(optimization_target) + client_model_cache_key_hash);
  if (server_model_cache_key_hash) {
    return *server_model_cache_key_hash;
  }
  // Fallback to using the client ModelCacheKey if server mapping did not
  // exist.
  return client_model_cache_key_hash;
}

std::optional<proto::OptimizationTarget> ParseOptimizationTarget(
    const std::string& optimization_target_str) {
  int optimization_target_number;
  if (!base::StringToInt(optimization_target_str,
                         &optimization_target_number)) {
    return std::nullopt;
  }
  if (!proto::OptimizationTarget_IsValid(optimization_target_number)) {
    return std::nullopt;
  }
  return static_cast<proto::OptimizationTarget>(optimization_target_number);
}

}  // namespace

ClientCacheKey ClientCacheKey::FromLocale(std::string locale) {
  proto::ModelCacheKey key;
  key.set_locale(locale);
  return ClientCacheKey{.hexhash = GetModelCacheKeyHash(key)};
}

ModelStoreMetadataEntry::ModelStoreMetadataEntry(
    const base::DictValue* metadata_entry)
    : metadata_entry_(metadata_entry) {}

ModelStoreMetadataEntry::~ModelStoreMetadataEntry() = default;

std::optional<base::FilePath> ModelStoreMetadataEntry::GetModelBaseDir() const {
  return base::ValueToFilePath(metadata_entry_->Find(kKeyModelBaseDir));
}

std::optional<int64_t> ModelStoreMetadataEntry::GetVersion() const {
  auto* version_str = metadata_entry_->FindString(kKeyVersion);
  if (!version_str) {
    return std::nullopt;
  }
  int64_t version;
  if (!base::StringToInt64(*version_str, &version)) {
    return std::nullopt;
  }
  return version;
}

base::Time ModelStoreMetadataEntry::GetExpiryTime() const {
  // TODO: crbug.com/40191801 - It seems like we should probably be able to
  // consider all entries that don't have expiries as expired at this point.
  return base::ValueToTime(metadata_entry_->Find(kKeyExpiryTime))
      .value_or(base::Time::Now() + kDefaultStoredModelValidDuration);
}

bool ModelStoreMetadataEntry::GetKeepBeyondValidDuration() const {
  return metadata_entry_->FindBool(kKeyKeepBeyondValidDuration).value_or(false);
}

ModelStoreMetadataEntryUpdater::ModelStoreMetadataEntryUpdater(
    PrefService& local_state,
    proto::OptimizationTarget optimization_target,
    const std::string& server_key_hash)
    : pref_updater_(local_state, prefs::localstate::kModelStoreMetadata),
      entry_(pref_updater_->EnsureDict(AsKey(optimization_target))
                 ->EnsureDict(server_key_hash)) {}

void ModelStoreMetadataEntryUpdater::SetModelBaseDir(
    base::FilePath model_base_dir) {
  entry_->Set(kKeyModelBaseDir, base::FilePathToValue(model_base_dir));
}

void ModelStoreMetadataEntryUpdater::SetVersion(int64_t version) {
  entry_->Set(kKeyVersion, base::NumberToString(version));
}

void ModelStoreMetadataEntryUpdater::SetExpiryTime(base::Time expiry_time) {
  entry_->Set(kKeyExpiryTime, base::TimeToValue(expiry_time));
}

void ModelStoreMetadataEntryUpdater::SetKeepBeyondValidDuration(
    bool keep_beyond_valid_duration) {
  entry_->Set(kKeyKeepBeyondValidDuration, keep_beyond_valid_duration);
}

void ModelStoreMetadataEntryUpdater::ClearMetadata() {
  entry_->clear();
}

ModelStoreLedger::ModelStoreLedger(PrefService& local_state)
    : local_state_(local_state) {}
ModelStoreLedger::~ModelStoreLedger() = default;

std::optional<ModelStoreMetadataEntry> ModelStoreLedger::GetEntryIfExists(
    proto::OptimizationTarget optimization_target,
    const ClientCacheKey& model_cache_key) const {
  auto* metadata_target =
      local_state_->GetDict(prefs::localstate::kModelStoreMetadata)
          .FindDict(AsKey(optimization_target));
  if (!metadata_target) {
    return std::nullopt;
  }

  auto* metadata_entry = metadata_target->FindDict(GetServerModelCacheKeyHash(
      *local_state_, optimization_target, model_cache_key));
  if (!metadata_entry) {
    return std::nullopt;
  }

  return ModelStoreMetadataEntry(metadata_entry);
}

std::set<base::FilePath> ModelStoreLedger::GetValidModelDirs() const {
  std::set<base::FilePath> valid_model_dirs;
  for (const auto optimization_target_entry :
       local_state_->GetDict(prefs::localstate::kModelStoreMetadata)) {
    if (!optimization_target_entry.second.is_dict()) {
      continue;
    }
    auto optimization_target =
        ParseOptimizationTarget(optimization_target_entry.first);
    if (!optimization_target) {
      continue;
    }
    for (auto model_cache_key_hash :
         optimization_target_entry.second.GetDict()) {
      if (!model_cache_key_hash.second.is_dict()) {
        continue;
      }
      auto metadata =
          ModelStoreMetadataEntry(&model_cache_key_hash.second.GetDict());
      auto model_base_dir = metadata.GetModelBaseDir();
      if (model_base_dir) {
        valid_model_dirs.insert(*model_base_dir);
      }
    }
  }
  return valid_model_dirs;
}

ModelStoreMetadataEntryUpdater ModelStoreLedger::UpdateEntry(
    proto::OptimizationTarget optimization_target,
    const ClientCacheKey& model_cache_key) {
  return ModelStoreMetadataEntryUpdater(
      *local_state_, optimization_target,
      GetServerModelCacheKeyHash(*local_state_, optimization_target,
                                 model_cache_key));
}

void ModelStoreLedger::UpdateModelCacheKeyMapping(
    proto::OptimizationTarget optimization_target,
    const ClientCacheKey& client_model_cache_key,
    const proto::ModelCacheKey& server_model_cache_key) {
  ScopedDictPrefUpdate pref_updater(*local_state_,
                                    prefs::localstate::kModelCacheKeyMapping);
  pref_updater->Set(
      base::NumberToString(static_cast<int>(optimization_target)) +
          client_model_cache_key.hexhash,
      GetModelCacheKeyHash(server_model_cache_key));
}

std::vector<base::FilePath> ModelStoreLedger::PurgeAllInactiveMetadata() {
  ScopedDictPrefUpdate updater(*local_state_,
                               prefs::localstate::kModelStoreMetadata);
  std::vector<std::pair<std::string, std::string>> entries_to_remove;
  std::vector<base::FilePath> inactive_model_dirs;
  auto killswitch_model_versions =
      features::GetPredictionModelVersionsInKillSwitch();
  for (auto optimization_target_entry : *updater) {
    if (!optimization_target_entry.second.is_dict()) {
      continue;
    }
    auto optimization_target =
        ParseOptimizationTarget(optimization_target_entry.first);
    if (!optimization_target) {
      continue;
    }
    for (auto model_cache_key_hash :
         optimization_target_entry.second.GetDict()) {
      if (!model_cache_key_hash.second.is_dict()) {
        continue;
      }
      bool should_remove_model =
          switches::ShouldPurgeModelAndFeaturesStoreOnStartup();

      // Check if the model expired.
      auto metadata =
          ModelStoreMetadataEntry(&model_cache_key_hash.second.GetDict());
      if (!should_remove_model && !metadata.GetKeepBeyondValidDuration() &&
          metadata.GetExpiryTime() <= base::Time::Now()) {
        should_remove_model = true;
        RecordPredictionModelStoreModelRemovalVersionHistogram(
            *optimization_target,
            PredictionModelStoreModelRemovalReason::kModelExpired);
      }
      if (!should_remove_model && metadata.GetVersion() &&
          IsPredictionModelVersionInKillSwitch(killswitch_model_versions,
                                               *optimization_target,
                                               *metadata.GetVersion())) {
        should_remove_model = true;
        RecordPredictionModelStoreModelRemovalVersionHistogram(
            *optimization_target,
            PredictionModelStoreModelRemovalReason::kModelInKillSwitchList);
      }

      if (should_remove_model) {
        auto base_model_dir = metadata.GetModelBaseDir();
        if (base_model_dir) {
          inactive_model_dirs.emplace_back(*base_model_dir);
        }
        entries_to_remove.emplace_back(optimization_target_entry.first,
                                       model_cache_key_hash.first);
      }
    }
    for (const auto& entry : entries_to_remove) {
      if (auto* optimization_target_dict = updater->FindDict(entry.first)) {
        optimization_target_dict->Remove(entry.second);
      }
    }
  }
  return inactive_model_dirs;
}

void ModelStoreLedger::AddPathToDelete(base::FilePath path) {
  ScopedDictPrefUpdate pref_update(&*local_state_,
                                   prefs::localstate::kStoreFilePathsToDelete);
  pref_update->Set(FilePathToString(path), true);
}

const base::DictValue& ModelStoreLedger::GetPathsToDelete() const {
  return local_state_->GetDict(prefs::localstate::kStoreFilePathsToDelete);
}

void ModelStoreLedger::RemovePathToDelete(base::FilePath path) {
  ScopedDictPrefUpdate pref_update(&*local_state_,
                                   prefs::localstate::kStoreFilePathsToDelete);
  pref_update->Remove(FilePathToString(path));
}

}  // namespace optimization_guide
