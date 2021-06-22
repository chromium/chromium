// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/segmentation_platform/internal/database/signal_database.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
namespace proto {
class SignalData;
}  // namespace proto

// Main implementation of SignalDatabase based on using a
// leveldb_proto::ProtoDatabase<proto::SignalData> as storage.
class SignalDatabaseImpl : public SignalDatabase {
 public:
  using SignalProtoDb = leveldb_proto::ProtoDatabase<proto::SignalData>;

  explicit SignalDatabaseImpl(std::unique_ptr<SignalProtoDb> database);
  ~SignalDatabaseImpl() override;

  // Disallow copy/assign.
  SignalDatabaseImpl(const SignalDatabaseImpl&) = delete;
  SignalDatabaseImpl& operator=(const SignalDatabaseImpl&) = delete;

  // SignalDatabase overrides.
  void Initialize(SuccessCallback callback) override;
  void WriteSample(SignalType signal_type,
                   uint64_t name_hash,
                   absl::optional<int32_t> value,
                   base::Time timestamp,
                   SuccessCallback callback) override;
  void GetSamples(SignalType signal_type,
                  uint64_t name_hash,
                  base::Time start_time,
                  base::Time end_time,
                  SampleCallback callback) override;
  void DeleteSamples(SignalType signal_type,
                     uint64_t name_hash,
                     base::Time end_time,
                     SuccessCallback callback) override;
  void CompactSamplesForDay(SignalType signal_type,
                            uint64_t name_hash,
                            base::Time time,
                            SuccessCallback callback) override;

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

  base::WeakPtrFactory<SignalDatabaseImpl> weak_ptr_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_IMPL_H_
