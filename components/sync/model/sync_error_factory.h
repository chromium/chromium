// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_H_
#define COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_H_

#include <string>

#include "base/location.h"
#include "components/sync/model/sync_error.h"

namespace syncer {

class SyncErrorFactory {
 public:
  SyncErrorFactory() = default;
  virtual ~SyncErrorFactory() = default;

  // Creates a SyncError object and uploads this call stack to breakpad.
  virtual SyncError CreateAndUploadError(const base::Location& location,
                                         const std::string& message) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_MODEL_SYNC_ERROR_FACTORY_H_
