// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_MUTABLE_DATA_BATCH_H_
#define COMPONENTS_SYNC_MODEL_MUTABLE_DATA_BATCH_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "components/sync/model/data_batch.h"

namespace syncer {

// An implementation of DataBatch that's purpose is to transfer ownership of
// EntityData objects. As soon as this batch recieves the EntityData, it owns
// them until Next() is invoked, when it gives up ownership. Because a vector
// is used internally, this impl is unaware when duplcate storage_keys are used,
// and it is the caller's job to avoid this.
class MutableDataBatch : public DataBatch {
 public:
  MutableDataBatch();

  MutableDataBatch(const MutableDataBatch&) = delete;
  MutableDataBatch& operator=(const MutableDataBatch&) = delete;

  ~MutableDataBatch() override;

  // Takes ownership of the data tied to a given key used for storage. Put
  // should be called at most once for any given storage_key. Data will be
  // readable in the same order that they are put into the batch.
  void Put(const std::string& storage_key,
           std::unique_ptr<EntityData> entity_data);

  // DataBatch implementation.
  bool HasNext() const override;
  KeyAndData Next() override;

 private:
  std::vector<KeyAndData> key_data_pairs_;
  size_t read_index_ = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_MUTABLE_DATA_BATCH_H_
