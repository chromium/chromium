// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_

#include "components/sync/service/model_type_local_data_batch_uploader.h"

namespace password_manager {

class PasswordLocalDataBatchUploader
    : public syncer::ModelTypeLocalDataBatchUploader {
 public:
  PasswordLocalDataBatchUploader() = default;

  PasswordLocalDataBatchUploader(const PasswordLocalDataBatchUploader&) =
      delete;
  PasswordLocalDataBatchUploader& operator=(
      const PasswordLocalDataBatchUploader&) = delete;

  ~PasswordLocalDataBatchUploader() override = default;

  // syncer::ModelTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
