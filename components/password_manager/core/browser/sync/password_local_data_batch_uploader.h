// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

namespace password_manager {

class PasswordStoreInterface;

class PasswordLocalDataBatchUploader
    : public syncer::DataTypeLocalDataBatchUploader {
 public:
  PasswordLocalDataBatchUploader(
      scoped_refptr<PasswordStoreInterface> profile_store,
      scoped_refptr<PasswordStoreInterface> account_storage);

  PasswordLocalDataBatchUploader(const PasswordLocalDataBatchUploader&) =
      delete;
  PasswordLocalDataBatchUploader& operator=(
      const PasswordLocalDataBatchUploader&) = delete;

  ~PasswordLocalDataBatchUploader() override;

  // syncer::DataTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;

  // Test-only APIs.
  bool trigger_local_data_migration_ongoing_for_test() const {
    return trigger_local_data_migration_ongoing_;
  }

 private:
  class PasswordFetchRequest;

  bool CanUpload() const;

  void OnGotLocalPasswordsForDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)>
          description_callback,
      std::unique_ptr<PasswordFetchRequest> request);

  void OnGotAllPasswordsForMigration(
      std::unique_ptr<PasswordFetchRequest> profile_store_request,
      std::unique_ptr<PasswordFetchRequest> account_store_request);

  const scoped_refptr<PasswordStoreInterface> profile_store_;
  const scoped_refptr<PasswordStoreInterface> account_store_;

  bool trigger_local_data_migration_ongoing_ = false;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
