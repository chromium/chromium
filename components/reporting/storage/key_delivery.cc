// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/key_delivery.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Factory method, returns smart pointer with deletion on sequence.
std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter> KeyDelivery::Create(
    base::TimeDelta key_check_period,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb) {
  auto sequence_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
  return std::unique_ptr<KeyDelivery, base::OnTaskRunnerDeleter>(
      new KeyDelivery(key_check_period, encryption_module,
                      async_start_upload_cb, sequence_task_runner),
      base::OnTaskRunnerDeleter(sequence_task_runner));
}

KeyDelivery::~KeyDelivery() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  upload_timer_.Stop();
  PostResponses(
      Status(error::UNAVAILABLE, "Key not delivered - Storage shuts down"));
}

void KeyDelivery::Request(RequestCallback callback) {
  StartPeriodicKeyUpdate();
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&KeyDelivery::EnqueueRequestAndPossiblyStart,
                                base::Unretained(this), std::move(callback)));
}

void KeyDelivery::OnKeyUpdateResult(Status status) {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&KeyDelivery::PostResponses,
                                base::Unretained(this), status));
}

void KeyDelivery::StartPeriodicKeyUpdate() {
  sequenced_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](KeyDelivery* self) {
                       DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
                       if (self->upload_timer_.IsRunning()) {
                         // We've already started the periodic key update.
                         return;
                       }
                       // `base::Unretained` is ok here because `upload_timer_`
                       // is destructed in the class destructor, and so is the
                       // callback.
                       self->upload_timer_.Start(
                           FROM_HERE, self->key_check_period_,
                           base::BindRepeating(&KeyDelivery::RequestKeyIfNeeded,
                                               base::Unretained(self)));
                     },
                     base::Unretained(this)));
}

KeyDelivery::KeyDelivery(
    base::TimeDelta key_check_period,
    scoped_refptr<EncryptionModuleInterface> encryption_module,
    UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : sequenced_task_runner_(sequenced_task_runner),
      key_check_period_(key_check_period),
      async_start_upload_cb_(async_start_upload_cb),
      encryption_module_(encryption_module) {
  CHECK(encryption_module_) << "Encryption module pointer not set";
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void KeyDelivery::RequestKeyIfNeeded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (encryption_module_->has_encryption_key() &&
      !encryption_module_->need_encryption_key()) {
    return;
  }
  // Request the key, do not expect any callback.
  Request(base::NullCallback());
}

void KeyDelivery::EnqueueRequestAndPossiblyStart(RequestCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (callback) {
    callbacks_.push_back(std::move(callback));
  }

  // Initiate upload with need_encryption_key flag and no records.
  UploaderInterface::UploaderInterfaceResultCb start_uploader_cb =
      base::BindOnce(&KeyDelivery::EncryptionKeyReceiverReady,
                     base::Unretained(this));
  async_start_upload_cb_.Run(UploaderInterface::UploadReason::KEY_DELIVERY,
                             std::move(start_uploader_cb));
}

void KeyDelivery::PostResponses(Status status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& callback : callbacks_) {
    std::move(callback).Run(status);
  }
  callbacks_.clear();
}

void KeyDelivery::EncryptionKeyReceiverReady(
    StatusOr<std::unique_ptr<UploaderInterface>> uploader_result) {
  if (!uploader_result.has_value()) {
    OnKeyUpdateResult(uploader_result.error());
    return;
  }
  uploader_result.value()->Completed(Status::StatusOK());
}
}  // namespace reporting
