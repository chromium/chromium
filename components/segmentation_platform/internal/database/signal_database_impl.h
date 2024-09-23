// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"

namespace base {
class Clock;
}  // namespace base

namespace segmentation_platform {
namespace proto {
class SignalData;
}  // namespace proto

// Main implementation of SignalDatabase based on using a
// leveldb_proto::ProtoDatabase<proto::SignalData> as storage.
class SignalDatabaseImpl : public SignalDatabase {
 public:
  using SignalProtoDb = leveldb_proto::ProtoDatabase<proto::SignalData>;

  SignalDatabaseImpl(std::unique_ptr<SignalProtoDb> database,
                     base::Clock* clock,
                     scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~SignalDatabaseImpl() override;

  // Disallow copy/assign.
  SignalDatabaseImpl(const SignalDatabaseImpl&) = delete;
  SignalDatabaseImpl& operator=(const SignalDatabaseImpl&) = delete;

  // SignalDatabase overrides.
  void Initialize(SuccessCallback callback) override;
  void WriteSample(proto::SignalType signal_type,
                   uint64_t name_hash,
                   std::optional<int32_t> value,
                   SuccessCallback callback) override;
  void GetSamples(proto::SignalType signal_type,
                  uint64_t name_hash,
                  base::Time start_time,
                  base::Time end_time,
                  EntriesCallback callback) override;
  const std::vector<DbEntry>* GetAllSamples() override;
  void DeleteSamples(proto::SignalType signal_type,
                     uint64_t name_hash,
                     base::Time end_time,
                     SuccessCallback callback) override;
  void CompactSamplesForDay(proto::SignalType signal_type,
                            uint64_t name_hash,
                            base::Time time,
                            SuccessCallback callback) override;

 private:
  void OnDatabaseInitialized(SuccessCallback callback,
                             leveldb_proto::Enums::InitStatus status);

  void OnGetSamples(
      EntriesCallback callback,
      base::Time start_time,
      base::Time end_time,
      bool success,
      std::unique_ptr<std::map<std::string, proto::SignalData>> entries);

  void OnGetAllSamples(
      SuccessCallback callback,
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

  // Cleans up entries from |recently_added_signals_| cache that are more than 1
  // second old.
  void CleanupStaleCachedEntries(base::Time current_timestamp);

  // The backing LevelDB proto database.
  std::unique_ptr<SignalProtoDb> database_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Used for getting current time.
  raw_ptr<base::Clock> clock_;

  // Whether or not initialization has been completed.
  bool initialized_{false};

  const bool enable_signal_cache_;
  std::vector<DbEntry> all_signals_;

  // A cache of recently added signals. Used for avoiding collisions between two
  // signals if they end up generating the same signal key, which can happen if
  // the two WriteSample() calls are less than 1 second apart. In that case, the
  // samples will be appended and rewritten to the database. Any entries older
  // than 1 second are cleaned up on the subsequent invocation to WriteSample().
  std::map<SignalKey, proto::SignalData> recently_added_signals_;

  // Enables the compaction fix. TODO(crbug.com/40860954): remove this
  // after fixing the bug.
  const bool should_fix_compaction_;

  base::WeakPtrFactory<SignalDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_
