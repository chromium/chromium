// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_BATCH_H_
#define COMPONENTS_SYNC_MODEL_DATA_BATCH_H_

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace syncer {

struct EntityData;

using KeyAndData = std::pair<std::string, std::unique_ptr<EntityData>>;

// Interface used by the processor to read data requested from the service.
class DataBatch {
 public:
  DataBatch() = default;
  virtual ~DataBatch() = default;

  // Returns if the data batch has another pair or not.
  virtual bool HasNext() const = 0;

  // Returns a pair of storage tag and owned entity data object. Invoking this
  // method will remove the pair from the batch, and should not be called if
  // HasNext() returns false.
  virtual KeyAndData Next() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_BATCH_H_
