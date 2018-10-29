// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_H__
#define COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_H__

#include <memory>
#include <string>

#include "base/location.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

class DataTypeErrorHandler {
 public:
  virtual ~DataTypeErrorHandler() {}

  // Call this to disable a datatype while it is running. This is usually
  // called for a runtime failure that is specific to a datatype.
  virtual void OnUnrecoverableError(const SyncError& error) = 0;

  // This will create a SyncError object. This will also upload a breakpad call
  // stack to crash server. A sync error usually means that sync has to be
  // disabled either for that type or completely.
  virtual SyncError CreateAndUploadError(const base::Location& location,
                                         const std::string& message,
                                         ModelType type) = 0;

  // Create a copy of this error handler.
  virtual std::unique_ptr<DataTypeErrorHandler> Copy() const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_DATA_TYPE_ERROR_HANDLER_H__
