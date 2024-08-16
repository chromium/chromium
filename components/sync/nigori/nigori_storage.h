// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_

#include <optional>

namespace sync_pb {
class NigoriLocalData;
}  // namespace sync_pb

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
  // was stored, returns std::nullopt.
  virtual std::optional<sync_pb::NigoriLocalData> RestoreData() = 0;

  // Removes all previously stored data.
  virtual void ClearData() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
