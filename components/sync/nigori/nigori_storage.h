// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/sync/protocol/nigori_local_data.pb.h"

namespace syncer {

// Interface for storing/loading Nigori data from the disk.
class NigoriStorage {
 public:
  NigoriStorage() = default;
  virtual ~NigoriStorage() = default;

  // Should atomically persist |data|.
  virtual void StoreData(const sync_pb::NigoriLocalData& data) = 0;

  // Returns previously stored NigoriLocalData. In case error occurs or no data
  // was stored, returns base::nullopt.
  virtual base::Optional<sync_pb::NigoriLocalData> RestoreData() = 0;

  // Removes all previously stored data.
  virtual void ClearData() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(NigoriStorage);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_STORAGE_H_
