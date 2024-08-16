// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_
#define COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/sync/engine/commit_and_get_updates_types.h"
#include "components/sync/engine/data_type_processor.h"
#include "components/sync/protocol/data_type_state.pb.h"

namespace syncer {

// The state passed from DataTypeProcessor to Sync thread during DataType
// activation.
struct DataTypeActivationResponse {
  DataTypeActivationResponse();
  ~DataTypeActivationResponse();

  // Initial DataTypeState at the moment of activation.
  sync_pb::DataTypeState data_type_state;

  // The DataTypeProcessor for the worker. Note that this is owned because
  // it is generally a proxy object to the real processor.
  std::unique_ptr<DataTypeProcessor> type_processor;

  // Special flag used in advanced cases where there is actually no need to
  // activate/connect a datatype.
  bool skip_engine_connection = false;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_DATA_TYPE_ACTIVATION_RESPONSE_H_
