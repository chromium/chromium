// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/get_all_nodes_request_barrier.h"

#include <utility>

#include "base/logging.h"

namespace syncer {

GetAllNodesRequestBarrier::GetAllNodesRequestBarrier(
    DataTypeSet requested_types,
    base::OnceCallback<void(base::Value::List)> callback)
    : awaiting_types_(requested_types), callback_(std::move(callback)) {
  CHECK(!awaiting_types_.empty());
}

GetAllNodesRequestBarrier::~GetAllNodesRequestBarrier() {
  if (!awaiting_types_.empty()) {
    DLOG(WARNING)
        << "GetAllNodesRequest deleted before request was fulfilled.  "
        << "Missing types are: " << DataTypeSetToDebugString(awaiting_types_);
  }
}

// Called when the set of nodes for a type has been returned.
// Only return one type of nodes each time.
void GetAllNodesRequestBarrier::OnReceivedNodesForType(
    const DataType type,
    base::Value::List node_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Add these results to our list.
  auto type_dict = base::Value::Dict()
                       .Set("type", DataTypeToDebugString(type))
                       .Set("nodes", std::move(node_list));
  result_accumulator_.Append(std::move(type_dict));

  // Remember that this part of the request is satisfied.
  awaiting_types_.Remove(type);

  if (awaiting_types_.empty()) {
    std::move(callback_).Run(std::move(result_accumulator_));
  }
}

}  // namespace syncer
