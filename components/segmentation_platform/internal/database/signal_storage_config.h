// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_STORAGE_CONFIG_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_STORAGE_CONFIG_H_

#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/clock.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"

namespace segmentation_platform {

// SignalStorageConfig is used to determine whether the signals for a model have
// been captured long enough to be used for model evaluation. It is also used
// for cleaning up the old entries for a signal in the signal database. It's
// able to answer these two questions based on storing (1) The timestamp of the
// first time we encountered the signal. (2) The longest storage requirement to
// store the signal across models. The DB is read to memory on startup, so that
// subsequent queries can be answered synchronously.
class SignalStorageConfig {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using SignalStorageConfigProtoDb =
      leveldb_proto::ProtoDatabase<proto::SignalStorageConfigs>;
  SignalStorageConfig(std::unique_ptr<SignalStorageConfigProtoDb> database,
                      base::Clock* clock);
  virtual ~SignalStorageConfig();

  // Disallow copy/assign.
  SignalStorageConfig(const SignalStorageConfig& other) = delete;
  SignalStorageConfig operator=(const SignalStorageConfig& other) = delete;

  // Initializes the DB and loads it to memory. Only called at startup, and
  // after that all client operations are synchronous.
  virtual void InitAndLoad(SuccessCallback callback);

  // Called to determine whether or not all the required signals in the given
  // model have been collected long enough to be eligible for using in model
  // evaluation. The model should be skipped if it hasn't been captured long
  // enough.
  virtual bool MeetsSignalCollectionRequirement(
      const proto::SegmentationModelMetadata& model_metadata,
      bool include_output = false);

  // Called whenever we find a model. Noop for existing models. Loops through
  // all the signals and updates their storage requirements. Also sets their
  // collection start time if not set already. This will be called for all the
  // models and eventually will store the longest storage length requirement for
  // every signal.
  virtual void OnSignalCollectionStarted(
      const proto::SegmentationModelMetadata& model_metadata);

  // Called to get a list of signals that can be cleaned up along with the
  // respective timestamp before which the signal can be cleaned up. The signals
  // can be cleaned up in two situations.
  // 1. If we have longer signal data than the maximum length required to store.
  // 2. If the signal isn't needed by any model any more.
  // (2) will be ignored if |known_signals| is passed as empty.
  // The result of the operation will be stored in the |result|.
  virtual void GetSignalsForCleanup(
      const std::set<std::pair<uint64_t, proto::SignalType>>& known_signals,
      std::vector<CleanupItem>& result) const;

  // Called to notify that the SignalDatabase entries have been cleaned up. Now
  // it should update the collection start timestamp in the SignalStorageConfig.
  virtual void UpdateSignalsForCleanup(const std::vector<CleanupItem>& signals);

 private:
  void OnDatabaseInitialized(SuccessCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  void OnDataLoaded(
      SuccessCallback callback,
      bool success,
      std::unique_ptr<std::vector<proto::SignalStorageConfigs>> entries);

  proto::SignalStorageConfig* FindSignal(uint64_t signal_hash,
                                         uint64_t event_hash,
                                         proto::SignalType signal_type);

  void UpdateConfigForUMASignal(int signal_storage_length,
                                bool* is_dirty,
                                const proto::UMAFeature& feature);

  bool UpdateConfigForSignal(int signal_storage_length,
                             uint64_t signal_hash,
                             uint64_t event_hash,
                             proto::SignalType signal_type);

  bool MeetsSignalCollectionRequirementForSignal(
      base::TimeDelta min_signal_collection_length,
      uint64_t signal_hash,
      uint64_t event_hash,
      proto::SignalType signal_type);

  // Helper method to flush the cached data to the DB. Called whenever the cache
  // is dirty.
  void WriteToDB();

  // Cached copy of the DB. Loaded at startup and used subsequently for faster
  // read and write. Written back to DB whenever it is updated.
  proto::SignalStorageConfigs config_;

  std::unique_ptr<SignalStorageConfigProtoDb> database_;

  raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<SignalStorageConfig> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_STORAGE_CONFIG_H_
