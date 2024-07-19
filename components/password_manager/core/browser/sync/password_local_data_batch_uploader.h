// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_

#include <list>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/sync/service/model_type_local_data_batch_uploader.h"

namespace password_manager {

class PasswordStoreInterface;

class PasswordLocalDataBatchUploader
    : public syncer::ModelTypeLocalDataBatchUploader {
 public:
  PasswordLocalDataBatchUploader(
      scoped_refptr<PasswordStoreInterface> profile_store,
      scoped_refptr<PasswordStoreInterface> account_storage);

  PasswordLocalDataBatchUploader(const PasswordLocalDataBatchUploader&) =
      delete;
  PasswordLocalDataBatchUploader& operator=(
      const PasswordLocalDataBatchUploader&) = delete;

  ~PasswordLocalDataBatchUploader() override;

  // syncer::ModelTypeLocalDataBatchUploader implementation.
  void GetLocalDataDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)> callback) override;
  void TriggerLocalDataMigration() override;

 private:
  class PasswordFetchRequest;

  bool CanUpload() const;

  void OnGotLocalPasswordsForDescription(
      base::OnceCallback<void(syncer::LocalDataDescription)>
          description_callback,
      PasswordFetchRequest* request);

  void OnGotAllPasswordsForMigration(
      PasswordFetchRequest* profile_store_request,
      PasswordFetchRequest* account_store_request);

  const scoped_refptr<PasswordStoreInterface> profile_store_;
  const scoped_refptr<PasswordStoreInterface> account_store_;

  // Ongoing reads from one of the PasswordStores, either for
  // GetLocalDataDescription(), or TriggerLocalDataMigration().
  std::list<std::unique_ptr<PasswordFetchRequest>> ongoing_requests_;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_LOCAL_DATA_BATCH_UPLOADER_H_
