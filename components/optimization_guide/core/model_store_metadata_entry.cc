// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_store_metadata_entry.h"

#include "base/files/file_path.h"
#include "base/json/values_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "model_store_metadata_entry.h"

namespace optimization_guide {

namespace {

// Key names for the metadata entries.
const char kKeyModelBaseDir[] = "mbd";
const char kKeyExpiryTime[] = "et";
const char kKeyKeepBeyondValidDuration[] = "kbvd";

}  // namespace

// static
absl::optional<ModelStoreMetadataEntry>
ModelStoreMetadataEntry::GetModelMetadataEntryIfExists(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key) {
  auto* metadata_target =
      local_state->GetDict(prefs::localstate::kModelStoreMetadata)
          .FindDict(
              base::NumberToString(static_cast<int>(optimization_target)));
  if (!metadata_target)
    return absl::nullopt;
  auto* metadata_entry =
      metadata_target->FindDict(GetModelCacheKeyHash(model_cache_key));
  if (!metadata_entry)
    return absl::nullopt;
  return ModelStoreMetadataEntry(metadata_entry);
}

ModelStoreMetadataEntry::ModelStoreMetadataEntry(
    const base::Value::Dict* metadata_entry)
    : metadata_entry_(metadata_entry) {}

ModelStoreMetadataEntry::~ModelStoreMetadataEntry() = default;

absl::optional<base::FilePath> ModelStoreMetadataEntry::GetModelBaseDir()
    const {
  return base::ValueToFilePath(metadata_entry_->Find(kKeyModelBaseDir));
}

base::Time ModelStoreMetadataEntry::GetExpiryTime() const {
  return base::ValueToTime(metadata_entry_->Find(kKeyExpiryTime))
      .value_or(base::Time::Now() + features::StoredModelsValidDuration());
}

bool ModelStoreMetadataEntry::GetKeepBeyondValidDuration() const {
  return metadata_entry_->FindBool(kKeyKeepBeyondValidDuration).value_or(false);
}

void ModelStoreMetadataEntry::SetMetadataEntry(
    const base::Value::Dict* metadata_entry) {
  metadata_entry_ = metadata_entry;
}

ModelStoreMetadataEntryUpdater::ModelStoreMetadataEntryUpdater(
    PrefService* local_state,
    proto::OptimizationTarget optimization_target,
    const proto::ModelCacheKey& model_cache_key)
    : ModelStoreMetadataEntry(/*metadata_entry=*/nullptr),
      pref_updater_(local_state, prefs::localstate::kModelStoreMetadata) {
  auto* metadata_target = pref_updater_->EnsureDict(
      base::NumberToString(static_cast<int>(optimization_target)));
  metadata_entry_updater_ =
      metadata_target->EnsureDict(GetModelCacheKeyHash(model_cache_key));
  SetMetadataEntry(metadata_entry_updater_);
}

void ModelStoreMetadataEntryUpdater::SetModelBaseDir(
    base::FilePath model_base_dir) {
  metadata_entry_updater_->Set(kKeyModelBaseDir,
                               base::FilePathToValue(model_base_dir));
}

void ModelStoreMetadataEntryUpdater::SetExpiryTime(base::Time expiry_time) {
  metadata_entry_updater_->Set(kKeyExpiryTime, base::TimeToValue(expiry_time));
}

void ModelStoreMetadataEntryUpdater::SetKeepBeyondValidDuration(
    bool keep_beyond_valid_duration) {
  metadata_entry_updater_->Set(kKeyKeepBeyondValidDuration,
                               keep_beyond_valid_duration);
}

}  // namespace optimization_guide