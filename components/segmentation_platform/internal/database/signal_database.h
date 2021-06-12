// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_

#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"

namespace segmentation_platform {
namespace proto {
class SignalData;
}  // namespace proto

// Responsible for storing histogram signals and user action events in a
// database. The signal samples are lazily bucketed into daily buckets for
// efficient storage and retrieval. A periodic job is responsible for running
// the compaction and deletion of old entries.
class SignalDatabase {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using SampleCallback = base::OnceCallback<void(
      std::vector<std::pair<base::Time, absl::optional<int32_t>>>)>;
  using SignalProtoDb = leveldb_proto::ProtoDatabase<proto::SignalData>;

  explicit SignalDatabase(std::unique_ptr<SignalProtoDb> database);
  virtual ~SignalDatabase();

  // Disallow copy/assign.
  SignalDatabase(const SignalDatabase&) = delete;
  SignalDatabase& operator=(const SignalDatabase&) = delete;

  // Called to initialize the database. Must be called before other methods.
  virtual void Initialize(SuccessCallback callback);

  // Called to write UMA events to the database. Sample timestamps are converted
  // to delta from UTC midnight for efficient storage.
  virtual void WriteSample(SignalType signal_type,
                           uint64_t name_hash,
                           absl::optional<int32_t> value,
                           base::Time timestamp,
                           SuccessCallback callback);

  // Called to get signals collected between any two timestamps. The samples are
  // returned in the |callback| as a list of pairs containing signal timestamp
  // and and an optional value.
  virtual void GetSamples(SignalType signal_type,
                          uint64_t name_hash,
                          base::Time start_time,
                          base::Time end_time,
                          SampleCallback callback);

  // Called to delete database entries having end time earlier than |end_time|.
  virtual void DeleteSamples(SignalType signal_type,
                             uint64_t name_hash,
                             base::Time end_time,
                             SuccessCallback callback);

  // Called to compact the signals collected for the given day. Do not run this
  // for the current day as it might lead to read/write race condition. Meant to
  // be used for compacting the entries for the previous day from a background
  // job. Nevertheless, the database will work correctly without the need for
  // any compaction. |time| is used for finding the associated day.
  virtual void CompactSamplesForDay(SignalType signal_type,
                                    uint64_t name_hash,
                                    base::Time time,
                                    SuccessCallback callback);

 private:
  void OnDatabaseInitialized(SuccessCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  void OnGetSamples(
      SampleCallback callback,
      base::Time start_time,
      base::Time end_time,
      bool success,
      std::unique_ptr<std::map<std::string, proto::SignalData>> entries);

  void OnGetSamplesForCompaction(
      SuccessCallback callback,
      std::string compact_key,
      bool success,
      std::unique_ptr<std::map<std::string, proto::SignalData>> entries);

  void OnGetSamplesForDeletion(
      SuccessCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, proto::SignalData>> entries);

  // The backing LevelDB proto database.
  std::unique_ptr<SignalProtoDb> database_;

  base::WeakPtrFactory<SignalDatabase> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_
