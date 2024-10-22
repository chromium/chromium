// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_

#include "base/functional/callback_forward.h"
#include "components/sync/service/local_data_description.h"

namespace syncer {

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

  // Triggers the process of moving the data restricted to the data that matches
  // the `syncer::LocalDataItemModel::DataId` in `items`. The process is in fact
  // async, but no notion of completion is currently exposed here.
  virtual void TriggerLocalDataMigration(
      std::vector<syncer::LocalDataItemModel::DataId> items);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_DATA_TYPE_LOCAL_DATA_BATCH_UPLOADER_H_
