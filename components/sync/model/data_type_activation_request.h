// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_

#include <string>

#include "base/time/time.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/model/model_error.h"
#include "google_apis/gaia/core_account_id.h"

namespace syncer {

// The state passed from ModelTypeController to the delegate during DataType
// activation.
struct DataTypeActivationRequest {
  DataTypeActivationRequest();
  DataTypeActivationRequest(const DataTypeActivationRequest& request);
  DataTypeActivationRequest(DataTypeActivationRequest&& request);
  ~DataTypeActivationRequest();

  DataTypeActivationRequest& operator=(
      const DataTypeActivationRequest& request);
  DataTypeActivationRequest& operator=(DataTypeActivationRequest&& request);

  ModelErrorHandler error_handler;
  CoreAccountId authenticated_account_id;
  std::string cache_guid;
  SyncMode sync_mode = SyncMode::kFull;

  // The start time of the confuguration this activation is part of.
  base::Time configuration_start_time;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_
