// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_KEY_DELIVERY_H_
#define COMPONENTS_REPORTING_STORAGE_KEY_DELIVERY_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Class for key upload/download to the file system in storage.
class KeyDelivery {
 public:
  using RequestCallback = base::OnceCallback<void(Status)>;

  // Factory method, returns smart pointer with deletion on sequence.
  static std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter> Create(
      base::TimeDelta key_check_period,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb);

  ~KeyDelivery();

  // Makes a request to update key, invoking `callback` once responded (if
  // specified).
  void Request(RequestCallback callback);

  // Starts periodic updates of the key (every time `period` has passed).
  // Does nothing if the periodic update is already scheduled.
  // Should be called after the initial key is set up.
  void StartPeriodicKeyUpdate();

  // Called upon key update success/failure.
  void OnKeyUpdateResult(Status status);

 private:
  // Constructor called by factory only.
  explicit KeyDelivery(
      base::TimeDelta key_check_period,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  void RequestKeyIfNeeded();

  void EnqueueRequestAndPossiblyStart(RequestCallback callback);

  void PostResponses(Status status);

  void EncryptionKeyReceiverReady(
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result);

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Period of checking possible key update.
  const base::TimeDelta key_check_period_;

  // Upload provider callback.
  const UploaderInterface::AsyncStartUploaderCb async_start_upload_cb_;

  // List of all request callbacks.
  std::vector<RequestCallback> callbacks_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used to check whether or not encryption is enabled and if we need to
  // request the key.
  const scoped_refptr<EncryptionModuleInterface> encryption_module_;

  // Used to periodically trigger check for encryption key
  base::RepeatingTimer upload_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_KEY_DELIVERY_H_
