// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_H_

#include <string_view>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/sequence_bound.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage.h"
#include "components/reporting/storage/storage_base.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

class StorageModule : public StorageModuleInterface {
 public:
  // Factory method creates `StorageModule` object.
  static void Create(
      const StorageOptions& options,
      const std::string_view legacy_storage_enabled,
      const scoped_refptr<QueuesContainer> queues_container,
      const scoped_refptr<EncryptionModuleInterface> encryption_module,
      const scoped_refptr<CompressionModule> compression_module,
      const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)>
          callback);

  StorageModule(const StorageModule& other) = delete;
  StorageModule& operator=(const StorageModule& other) = delete;

  // AddRecord will add |record| (taking ownership) to the |StorageModule|
  // according to the provided |priority|. On completion, |callback| will be
  // called.
  void AddRecord(Priority priority,
                 Record record,
                 EnqueueCallback callback) override;

  // Initiates upload of collected records according to the priority.
  // Called usually for a queue with an infinite or very large upload period.
  // Multiple |Flush| calls can safely run in parallel.
  // Returns error if cannot start upload.
  void Flush(Priority priority, FlushCallback callback) override;

  // Once a record has been successfully uploaded, the sequence information
  // can be passed back to the StorageModule here for record deletion.
  // If |force| is false (which is used in most cases), |sequence_information|
  // only affects Storage if no higher sequencing was confirmed before;
  // otherwise it is accepted unconditionally.
  // Declared virtual for testing purposes.
  virtual void ReportSuccess(SequenceInformation sequence_information,
                             bool force,
                             base::OnceCallback<void(Status)> done_cb);

  // If the server attached signed encryption key to the response, it needs to
  // be paased here.
  // Declared virtual for testing purposes.
  virtual void UpdateEncryptionKey(SignedEncryptionInfo signed_encryption_key);

  // Parse list of priorities to be in legacy single-generation action state
  // from now on. All other priorities are in multi-generation action state.
  void SetLegacyEnabledPriorities(std::string_view legacy_storage_enabled);

  // Attaches a repeating callback to be invoked every time `ReportSuccess`
  // detects material progress in upload.
  void AttachUploadSuccessCb(
      base::RepeatingCallback<void()> storage_upload_success_cb);

 protected:
  // Constructor can only be called by `Create` factory method.
  explicit StorageModule(const StorageOptions& options);

  // Refcounted object must have destructor declared protected or private.
  ~StorageModule() override;

  void InitStorage(
      const StorageOptions& options,
      const scoped_refptr<QueuesContainer> queues_container,
      const scoped_refptr<EncryptionModuleInterface> encryption_module,
      const scoped_refptr<CompressionModule> compression_module,
      const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)>
          callback);

  // Sets `storage_` to a valid `Storage` or returns error status via
  // `callback`.
  void SetStorage(
      base::OnceCallback<void(StatusOr<scoped_refptr<StorageModule>>)> callback,
      StatusOr<scoped_refptr<Storage>> storage);

  void InjectStorageUnavailableErrorForTesting();

 private:
  class UploadProgressTracker;
  friend class StorageModuleTest;
  friend base::RefCountedThreadSafe<StorageModule>;

  // Upload progress tracker.
  base::SequenceBound<UploadProgressTracker> upload_progress_tracker_;

  // Reference to `Storage` object.
  scoped_refptr<Storage> storage_;

  // Parameters used to create Storage
  const StorageOptions options_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_MODULE_H_
