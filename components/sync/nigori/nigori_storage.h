// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_

#include "components/sync/protocol/nigori_local_data.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {

// Interface for storing/loading Nigori data from the disk.
class NigoriStorage {
 public:
  NigoriStorage() = default;

  NigoriStorage(const NigoriStorage&) = delete;
  NigoriStorage& operator=(const NigoriStorage&) = delete;

  virtual ~NigoriStorage() = default;

  // Should atomically persist |data|.
  virtual void StoreData(const sync_pb::NigoriLocalData& data) = 0;

  // Returns previously stored NigoriLocalData. In case error occurs or no data
  // was stored, returns absl::nullopt.
  virtual absl::optional<sync_pb::NigoriLocalData> RestoreData() = 0;

  // Removes all previously stored data.
  virtual void ClearData() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
