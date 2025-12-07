// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_storage_config.h"

#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "components/segmentation_platform/internal/metadata/metadata_utils.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {
namespace {
// The key of the one and only entry in the database.
const char kDatabaseKey[] = "config";
}  // namespace

SignalStorageConfig::SignalStorageConfig(
    std::unique_ptr<SignalStorageConfigProtoDb> database,
    base::Clock* clock)
    : database_(std::move(database)), clock_(clock) {}

SignalStorageConfig::~SignalStorageConfig() = default;

void SignalStorageConfig::InitAndLoad(SuccessCallback callback) {
  database_->Init(
      leveldb_proto::CreateSimpleOptions(),
      base::BindOnce(&SignalStorageConfig::OnDatabaseInitialized,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SignalStorageConfig::OnDatabaseInitialized(
    SuccessCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  if (status != leveldb_proto::Enums::InitStatus::kOK) {
    std::move(callback).Run(false);
    return;
  }

  database_->LoadEntries(base::BindOnce(&SignalStorageConfig::OnDataLoaded,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback)));
}

void SignalStorageConfig::OnDataLoaded(
    SuccessCallback callback,
    bool success,
    std::unique_ptr<std::vector<proto::SignalStorageConfigs>> entries) {
  if (!success || !entries) {
    std::move(callback).Run(false);
    return;
  }

  // We should only have one entry in the DB, or zero if it is the first time.
  if (entries->empty()) {
    std::move(callback).Run(true);
    return;
  }

  DCHECK(entries->size() == 1);
  config_ = std::move(entries->at(0));
  std::move(callback).Run(true);
}

proto::SignalStorageConfig* SignalStorageConfig::FindSignal(
    uint64_t signal_hash,
    uint64_t event_hash,
    proto::SignalType signal_type) {
  // TODO(shaktisahu): May be have an internal map of signals.
  for (int i = 0; i < config_.signals().size(); ++i) {
    auto* signal_config = config_.mutable_signals(i);
    if (signal_config->name_hash() == signal_hash &&
        signal_config->event_hash() == event_hash &&
        signal_config->signal_type() == signal_type) {
      return signal_config;
    }
  }
  return nullptr;
}

void SignalStorageConfig::UpdateConfigForUMASignal(
    int signal_storage_length,
    bool* is_dirty,
    const proto::UMAFeature& feature) {
  if (metadata_utils::ValidateMetadataUmaFeature(feature) !=
      metadata_utils::ValidationResult::kValidationSuccess) {
    return;
  }
  if (UpdateConfigForSignal(signal_storage_length, feature.name_hash(),
                            CleanupItem::kNonUkmEventHash, feature.type())) {
    *is_dirty = true;
  }
}

bool SignalStorageConfig::UpdateConfigForSignal(int signal_storage_length,
                                                uint64_t signal_hash,
                                                uint64_t event_hash,
                                                proto::SignalType signal_type) {
  proto::SignalStorageConfig* config =
      FindSignal(signal_hash, event_hash, signal_type);
  if (config) {
    if (config->storage_length_s() < signal_storage_length) {
      // We found a model that has a longer storage length requirement. Update
      // it to DB.
      config->set_storage_length_s(signal_storage_length);
      return true;
    }
  } else {
    // This is the first time we have encountered this signal. Just create an
    // entry in the DB, and set collection start time.
    proto::SignalStorageConfig* signal_config = config_.add_signals();
    signal_config->set_name_hash(signal_hash);
    if (signal_type == proto::SignalType::UKM_EVENT)
      signal_config->set_event_hash(event_hash);
    signal_config->set_signal_type(signal_type);
    signal_config->set_storage_length_s(signal_storage_length);
    signal_config->set_collection_start_time_s(
        clock_->Now().ToDeltaSinceWindowsEpoch().InSeconds());
    return true;
  }
  return false;
}

bool SignalStorageConfig::MeetsSignalCollectionRequirementForSignal(
    base::TimeDelta min_signal_collection_length,
    uint64_t signal_hash,
    uint64_t event_hash,
    proto::SignalType signal_type) {
  if (min_signal_collection_length.is_zero()) {
    return true;
  }
  const proto::SignalStorageConfig* config =
      FindSignal(signal_hash, event_hash, signal_type);
  if (!config || config->collection_start_time_s() == 0)
    return false;

  base::Time collection_start_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Seconds(config->collection_start_time_s()));
  return clock_->Now() - collection_start_time >= min_signal_collection_length;
}

bool SignalStorageConfig::MeetsSignalCollectionRequirement(
    const proto::SegmentationModelMetadata& model_metadata,
    bool include_outputs) {
  base::TimeDelta min_signal_collection_length =
      model_metadata.min_signal_collection_length() *
      metadata_utils::GetTimeUnit(model_metadata);

  // Loop through all the signals specified in the model, and check if they have
  // been collected long enough.
  bool meets_requirement = true;
  auto feature_visit = base::BindRepeating(
      [](SignalStorageConfig* config,
         base::TimeDelta min_signal_collection_length, bool* meets_requirement,
         const proto::UMAFeature& feature) {
        // Skip the signals that has bucket_count set to 0. These ones are
        // only for collection purposes and hence don't get used in model
        // evaluation.
        if (feature.bucket_count() == 0) {
          return;
        }

        if (metadata_utils::ValidateMetadataUmaFeature(feature) !=
            metadata_utils::ValidationResult::kValidationSuccess) {
          return;
        }

        if (!config->MeetsSignalCollectionRequirementForSignal(
                min_signal_collection_length, feature.name_hash(),
                CleanupItem::kNonUkmEventHash, feature.type())) {
          *meets_requirement = false;
          return;
        };
      },
      base::Unretained(this), min_signal_collection_length,
      base::Unretained(&meets_requirement));
  metadata_utils::VisitAllUmaFeatures(model_metadata, include_outputs,
                                      std::move(feature_visit));
  if (!meets_requirement) {
    return false;
  }

  // Loop through sql features.
  for (auto const& feature : model_metadata.input_features()) {
    if (!feature.has_sql_feature())
      continue;

    if (metadata_utils::ValidateMetadataSqlFeature(feature.sql_feature()) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      continue;
    }

    const proto::SignalFilterConfig& sql_config =
        feature.sql_feature().signal_filter();

    for (auto const& event : sql_config.ukm_events()) {
      for (auto const& metric_hash : event.metric_hash_filter()) {
        if (!MeetsSignalCollectionRequirementForSignal(
                min_signal_collection_length, metric_hash, event.event_hash(),
                proto::SignalType::UKM_EVENT)) {
          return false;
        };
      }
    }
  }

  return true;
}

void SignalStorageConfig::OnSignalCollectionStarted(
    const proto::SegmentationModelMetadata& model_metadata) {
  int signal_storage_length =
      model_metadata.signal_storage_length() *
      metadata_utils::GetTimeUnit(model_metadata).InSeconds();

  // Run through the model and calculate for each signal.
  bool is_dirty = false;
  metadata_utils::VisitAllUmaFeatures(
      model_metadata, /*include_outputs=*/true,
      base::BindRepeating(&SignalStorageConfig::UpdateConfigForUMASignal,
                          base::Unretained(this), signal_storage_length,
                          base::Unretained(&is_dirty)));

  // Add signals for sql features.
  for (auto const& feature : model_metadata.input_features()) {
    if (!feature.has_sql_feature())
      continue;

    if (metadata_utils::ValidateMetadataSqlFeature(feature.sql_feature()) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      continue;
    }

    const proto::SignalFilterConfig& sql_config =
        feature.sql_feature().signal_filter();

    for (auto const& event : sql_config.ukm_events()) {
      for (auto const& metric_hash : event.metric_hash_filter()) {
        if (UpdateConfigForSignal(signal_storage_length, metric_hash,
                                  event.event_hash(),
                                  proto::SignalType::UKM_EVENT)) {
          is_dirty = true;
        }
      }
    }
  }

  if (is_dirty)
    WriteToDB();
}

void SignalStorageConfig::GetSignalsForCleanup(
    const std::set<std::pair<uint64_t, proto::SignalType>>& known_signals,
    std::vector<CleanupItem>& result) const {
  // Ukm signals are included only when its over required length.
  for (int i = 0; i < config_.signals_size(); ++i) {
    const auto& signal_config = config_.signals(i);
    base::Time collection_start_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(signal_config.collection_start_time_s()));
    base::TimeDelta required_storage_length =
        base::Seconds(signal_config.storage_length_s());
    base::Time earliest_needed_timestamp =
        clock_->Now() - required_storage_length;

    if (earliest_needed_timestamp < collection_start_time)
      continue;

    result.emplace_back(signal_config.name_hash(), signal_config.event_hash(),
                        signal_config.signal_type(), earliest_needed_timestamp);
  }

  // Now collect the signals that aren't used by any of the models.
  if (known_signals.empty())
    return;

  for (int i = 0; i < config_.signals_size(); ++i) {
    const auto& signal_config = config_.signals(i);
    // UKM database cleans up signals after `kUkmEntriesTTL` time. Hence don't
    // include signals when not needed. For UMA signals, skip adding signals
    // that are used by any models.
    // TODO(ssid) : Handle this for UKM signals.
    if (base::Contains(known_signals,
                       std::make_pair(signal_config.name_hash(),
                                      signal_config.signal_type())) ||
        signal_config.signal_type() == proto::SignalType::UKM_EVENT) {
      continue;
    }

    result.emplace_back(signal_config.name_hash(), signal_config.event_hash(),
                        signal_config.signal_type(), clock_->Now());
  }
}

void SignalStorageConfig::UpdateSignalsForCleanup(
    const std::vector<CleanupItem>& signals) {
  bool is_dirty = false;
  for (auto& signal_for_cleanup : signals) {
    proto::SignalStorageConfig* signal_config =
        FindSignal(signal_for_cleanup.name_hash, signal_for_cleanup.event_hash,
                   signal_for_cleanup.signal_type);
    if (!signal_config)
      continue;

    signal_config->set_collection_start_time_s(
        signal_for_cleanup.timestamp.ToDeltaSinceWindowsEpoch().InSeconds());
    is_dirty = true;
  }

  if (is_dirty)
    WriteToDB();
}

void SignalStorageConfig::WriteToDB() {
  auto entries_to_save = std::make_unique<
      std::vector<std::pair<std::string, proto::SignalStorageConfigs>>>();
  auto keys_to_delete = std::make_unique<std::vector<std::string>>();

  entries_to_save->emplace_back(std::make_pair(kDatabaseKey, config_));
  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(keys_to_delete), base::DoNothing());
}

}  // namespace segmentation_platform
