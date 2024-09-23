// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback_forward.h"

namespace syncer {

struct LocalDataDescription;

// Data types possessing the distinction between "local" and "account" storages
// can implement this interface to allow moving data from the former to the
// latter. In other words, to upload the local data.
class DataTypeLocalDataBatchUploader {
 public:
  virtual ~DataTypeLocalDataBatchUploader() = default;

  // Retrieves information about the existing local data.
  virtual void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) = 0;

  // Triggers the process of moving the data. The process is in fact async,
  // but no notion of completion is currently exposed here.
  virtual void TriggerLocalDataMigration() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
