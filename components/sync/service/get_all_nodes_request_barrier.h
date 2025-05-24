// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_GET_ALL_NODES_REQUEST_BARRIER_H_
#define COMPONENTS_SYNC_SERVICE_GET_ALL_NODES_REQUEST_BARRIER_H_

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/sync/base/data_type.h"

namespace syncer {

// A helper class for DataTypeManagerImpl's implementation of
// GetAllNodesForDebugging. It accumulates the responses from each type's
// DataTypeController, and runs the supplied callback once they're all done.
class GetAllNodesRequestBarrier
    : public base::RefCountedThreadSafe<GetAllNodesRequestBarrier> {
 public:
  // Once `OnReceivedNodesForType` has been called for each type in
  // `requested_types`, the `callback` will be run. `requested_types` must not
  // be empty.
  GetAllNodesRequestBarrier(
      DataTypeSet requested_types,
      base::OnceCallback<void(base::Value::List)> callback);

  GetAllNodesRequestBarrier(const GetAllNodesRequestBarrier&) = delete;
  GetAllNodesRequestBarrier& operator=(const GetAllNodesRequestBarrier&) =
      delete;

  void OnReceivedNodesForType(const DataType type, base::Value::List node_list);

 private:
  friend class base::RefCountedThreadSafe<GetAllNodesRequestBarrier>;
  virtual ~GetAllNodesRequestBarrier();

  DataTypeSet awaiting_types_;
  base::Value::List result_accumulator_;
  base::OnceCallback<void(base::Value::List)> callback_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_GET_ALL_NODES_REQUEST_BARRIER_H_
