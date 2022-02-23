// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_storage_config.h"

#include "base/callback_helpers.h"
#include "base/containers/contains.h"
#include "components/segmentation_platform/internal/database/metadata_utils.h"

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
    proto::SignalType signal_type) {
  // TODO(shaktisahu): May be have an internal map of signals.
  for (int i = 0; i < config_.signals().size(); ++i) {
    auto* signal_config = config_.mutable_signals(i);
    if (signal_config->name_hash() == signal_hash &&
        signal_config->signal_type() == signal_type) {
      return signal_config;
    }
  }
  return nullptr;
}

bool SignalStorageConfig::MeetsSignalCollectionRequirement(
    const proto::SegmentationModelMetadata& model_metadata) {
  base::TimeDelta min_signal_collection_length =
      model_metadata.min_signal_collection_length() *
      metadata_utils::GetTimeUnit(model_metadata);

  // Loop through all the signals specified in the model, and check if they have
  // been collected long enough.
  auto features = metadata_utils::GetAllUmaFeatures(model_metadata,
                                                    /*include_outputs=*/false);
  for (auto const& feature : features) {
    // Skip the signals that has bucket_count set to 0. These ones are only for
    // collection purposes and hence don't get used in model evaluation.
    if (feature.bucket_count() == 0)
      continue;

    if (metadata_utils::ValidateMetadataUmaFeature(feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      continue;
    }

    proto::SignalStorageConfig* config =
        FindSignal(feature.name_hash(), feature.type());
    if (!config || config->collection_start_time_s() == 0)
      return false;

    base::Time collection_start_time = base::Time::FromDeltaSinceWindowsEpoch(
        base::Seconds(config->collection_start_time_s()));
    if (clock_->Now() - collection_start_time < min_signal_collection_length)
      return false;
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
  auto features = metadata_utils::GetAllUmaFeatures(model_metadata,
                                                    /*include_outputs=*/true);
  for (auto const& feature : features) {
    if (metadata_utils::ValidateMetadataUmaFeature(feature) !=
        metadata_utils::ValidationResult::kValidationSuccess) {
      continue;
    }

    proto::SignalStorageConfig* config =
        FindSignal(feature.name_hash(), feature.type());
    if (config) {
      if (config->storage_length_s() < signal_storage_length) {
        // We found a model that has a longer storage length requirement. Update
        // it to DB.
        config->set_storage_length_s(signal_storage_length);
        is_dirty = true;
      }
    } else {
      // This is the first time we have encountered this signal. Just create an
      // entry in the DB, and set collection start time.
      proto::SignalStorageConfig* signal_config = config_.add_signals();
      signal_config->set_name_hash(feature.name_hash());
      signal_config->set_signal_type(feature.type());
      signal_config->set_storage_length_s(signal_storage_length);
      signal_config->set_collection_start_time_s(
          clock_->Now().ToDeltaSinceWindowsEpoch().InSeconds());
      is_dirty = true;
    }
  }

  if (is_dirty)
    WriteToDB();
}

void SignalStorageConfig::GetSignalsForCleanup(
    const std::set<std::pair<uint64_t, proto::SignalType>>& known_signals,
    std::vector<std::tuple<uint64_t, proto::SignalType, base::Time>>& result)
    const {
  // Collect the signals that have longer than required data.
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

    result.emplace_back(std::make_tuple(signal_config.name_hash(),
                                        signal_config.signal_type(),
                                        earliest_needed_timestamp));
  }

  // Now collect the signals that aren't used by any of the models.
  if (known_signals.empty())
    return;

  for (int i = 0; i < config_.signals_size(); ++i) {
    const auto& signal_config = config_.signals(i);
    if (base::Contains(known_signals,
                       std::make_pair(signal_config.name_hash(),
                                      signal_config.signal_type()))) {
      continue;
    }

    result.emplace_back(std::make_tuple(
        signal_config.name_hash(), signal_config.signal_type(), clock_->Now()));
  }
}

void SignalStorageConfig::UpdateSignalsForCleanup(
    const std::vector<std::tuple<uint64_t, proto::SignalType, base::Time>>&
        signals) {
  bool is_dirty = false;
  for (auto& tuple : signals) {
    uint64_t name_hash = std::get<0>(tuple);
    proto::SignalType signal_type = std::get<1>(tuple);
    base::Time timestamp = std::get<2>(tuple);

    proto::SignalStorageConfig* signal_config =
        FindSignal(name_hash, signal_type);
    if (!signal_config)
      continue;

    signal_config->set_collection_start_time_s(
        timestamp.ToDeltaSinceWindowsEpoch().InSeconds());
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
