// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
#define COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "components/performance_manager/graph/node_base.h"
#include "components/performance_manager/public/graph/node_attached_data.h"

namespace performance_manager {

// Helper class for providing internal storage of a NodeAttachedData
// implementation directly in a node. The storage is provided as a raw buffer of
// bytes which is initialized externally by the NodeAttachedDataImpl via a
// placement new. In this way the node only needs to know about the
// NodeAttachedData base class, and the size of the required storage.
template <size_t DataSize>
class InternalNodeAttachedDataStorage {
 public:
  static constexpr size_t kDataSize = DataSize;

  InternalNodeAttachedDataStorage() {}

  ~InternalNodeAttachedDataStorage() { Reset(); }

  // Returns a pointer to the data object, if allocated.
  NodeAttachedData* Get() { return data_; }

  void Reset() {
    if (data_)
      data_->~NodeAttachedData();
    data_ = nullptr;
  }

  uint8_t* buffer() { return buffer_; }

 protected:
  friend class InternalNodeAttachedDataStorageAccess;

  // Transitions this object to being allocated.
  void Set(NodeAttachedData* data) {
    DCHECK(!data_);
    // Depending on the object layout, once it has been cast to a
    // NodeAttachedData there's no guarantee that the pointer will be at the
    // head of the object, only that the pointer will be somewhere inside of the
    // full object extent.
    DCHECK_LE(buffer_, reinterpret_cast<uint8_t*>(data));
    DCHECK_GT(buffer_ + kDataSize, reinterpret_cast<uint8_t*>(data));
    data_ = data;
  }

 private:
  NodeAttachedData* data_ = nullptr;
  uint8_t buffer_[kDataSize];
  DISALLOW_COPY_AND_ASSIGN(InternalNodeAttachedDataStorage);
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_GRAPH_NODE_ATTACHED_DATA_H_
