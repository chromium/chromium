// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/model/model_error.h"
#include "google_apis/gaia/gaia_id.h"

namespace syncer {

class AttachmentUploader;

// The state passed from DataTypeController to the delegate during DataType
// activation.
struct DataTypeActivationRequest {
  DataTypeActivationRequest();
  DataTypeActivationRequest(const DataTypeActivationRequest& request);
  DataTypeActivationRequest(DataTypeActivationRequest&& request);
  ~DataTypeActivationRequest();

  DataTypeActivationRequest& operator=(
      const DataTypeActivationRequest& request);
  DataTypeActivationRequest& operator=(DataTypeActivationRequest&& request);

  bool IsValid() const;

  ModelErrorHandler error_handler;
  GaiaId authenticated_gaia_id;
  std::string cache_guid;
  SyncMode sync_mode = SyncMode::kFull;

  // The start time of the confuguration this activation is part of.
  base::Time configuration_start_time;

  // Optional uploader used to coordinate blob attachment uploads.
  scoped_refptr<AttachmentUploader> attachment_uploader;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_ACTIVATION_REQUEST_H_
