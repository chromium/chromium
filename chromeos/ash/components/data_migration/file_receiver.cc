// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/data_migration/file_receiver.h"

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"

namespace data_migration {
namespace {

// Theoretically, this should never fail because the NC mojo service has told
// the caller that the transfer was a success by the time this function is
// called. That being said, file transfer is long and complex with lots of I/O
// involved. So this just double-checks the result before notifying the caller
// that the transfer succeeded.
bool DidFileTransferComplete(const base::FilePath& path,
                             int64_t expected_size_in_bytes) {
  static constexpr char kFailureLogPrefix[] =
      "File transfer verification failed. ";
  if (!base::PathExists(path)) {
    LOG(DFATAL) << kFailureLogPrefix << "File does not exist.";
    return false;
  }

  if (!base::PathIsReadable(path)) {
    LOG(DFATAL) << kFailureLogPrefix << "File is not even readable.";
    return false;
  }

  std::optional<int64_t> actual_file_size_in_bytes = base::GetFileSize(path);

  if (!actual_file_size_in_bytes.has_value()) {
    LOG(DFATAL) << kFailureLogPrefix << "Failed to get file size.";
    return false;
  }

  if (actual_file_size_in_bytes.value() != expected_size_in_bytes) {
    LOG(DFATAL) << kFailureLogPrefix << "actual_file_size_in_bytes="
                << actual_file_size_in_bytes.value()
                << " expected_size_in_bytes=" << expected_size_in_bytes;
    return false;
  }
  return true;
}

}  // namespace

FileReceiver::Observer::Observer(
    base::OnceClosure on_file_registered_in,
    base::OnceCallback<void(bool)> on_file_transfer_complete_in)
    : on_file_registered(std::move(on_file_registered_in)),
      on_file_transfer_complete(std::move(on_file_transfer_complete_in)) {
  CHECK(on_file_registered);
  CHECK(on_file_transfer_complete);
}

FileReceiver::Observer::Observer(Observer&&) = default;

FileReceiver::Observer& FileReceiver::Observer::operator=(Observer&&) = default;

FileReceiver::Observer::~Observer() = default;

FileReceiver::FileReceiver(int64_t payload_id,
                           base::FilePath path,
                           Observer observer,
                           NearbyConnectionsManager* nearby_connections_manager)
    : payload_id_(payload_id),
      path_(std::move(path)),
      observer_(std::move(observer)),
      nearby_connections_manager_(nearby_connections_manager) {
  CHECK(!path_.empty());
  CHECK(nearby_connections_manager_);
  nearby_connections_manager_->RegisterPayloadStatusListener(payload_id_,
                                                             GetWeakPtr());
  RegisterPayloadPath(/*attempt_number=*/1);
}

FileReceiver::~FileReceiver() {
  if (!transfer_completed_successfully_) {
    VLOG(1) << "FileReceiver destroyed before file transfer completed. "
               "Canceling payload transfer.";
    // Invalidate weak ptrs first so that we do not synchronously get an
    // `OnStatusUpdate(<canceled>)` notification from our own call to
    // `NearbyConnectionsManager::Cancel()`.
    weak_ptr_factory_.InvalidateWeakPtrs();
    // Note this is a no-op if the transfer completed with a failure.
    nearby_connections_manager_->Cancel(payload_id_);
  }
  // Closes all file descriptors associated with this payload. Prevents them
  // from accumulating over the course of a long data migration.
  nearby_connections_manager_->ClearIncomingPayloadWithId(payload_id_);
}

void FileReceiver::RegisterPayloadPath(int attempt_number) {
  // Past 3 attempts, the partition is probably in such a bad state that
  // retrying more will not help.
  constexpr int kMaxNumAttempts = 3;
  if (attempt_number > kMaxNumAttempts) {
    LOG(ERROR) << "RegisterPayloadPath() failed " << kMaxNumAttempts
               << " times. File transfer is a failure";
    CompleteTransfer(/*verification_status=*/false);
    return;
  }

  // Tell the NC library that incoming payload from the remote device with
  // `payload_id_` should get written to `path_`. Registration must happen
  // before the incoming payload arrives, or file transmission will fail.
  nearby_connections_manager_->RegisterPayloadPath(
      payload_id_, path_,
      base::BindOnce(&FileReceiver::OnRegisterPayloadPathComplete,
                     file_receiver_weak_factory_.GetWeakPtr(), attempt_number));
}

void FileReceiver::OnRegisterPayloadPathComplete(
    int attempt_number,
    NearbyConnectionsManager::ConnectionsStatus result) {
  constexpr base::TimeDelta kRetryDelay = base::Milliseconds(250);
  if (result == NearbyConnectionsManager::ConnectionsStatus::kSuccess) {
    VLOG(1) << "data_migration file successfully registered with NC";
    CHECK(observer_.on_file_registered);
    std::move(observer_.on_file_registered).Run();
  } else {
    // This can legitimately happen from transient file I/O errors in the NC
    // library. There are no network operations involved though, so
    // exponential backoff and jitter are unnecessary.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&FileReceiver::RegisterPayloadPath,
                       file_receiver_weak_factory_.GetWeakPtr(),
                       attempt_number + 1),
        kRetryDelay);
  }
}

void FileReceiver::OnStatusUpdate(PayloadTransferUpdatePtr update,
                                  std::optional<Medium> upgraded_medium) {
  CHECK(update);
  if (upgraded_medium) {
    VLOG(4) << "File transfer for using upgraded_medium " << *upgraded_medium;
  } else {
    VLOG(4) << "File transfer for has unknown upgraded_medium";
  }

  switch (update->status) {
    case ::nearby::connections::mojom::PayloadStatus::kSuccess:
      VLOG(1) << "File transfer completed";
      VerifyFileTransferResult(update->total_bytes);
      return;

    case ::nearby::connections::mojom::PayloadStatus::kInProgress: {
      // Catch for divide-by-zero when calculating `progress_as_percentage`.
      if (update->total_bytes == 0) {
        LOG(ERROR) << "File has expected size of zero bytes.";
        return;
      }
      if (update->bytes_transferred > update->total_bytes) {
        LOG(ERROR) << "File bytes_transferred(" << update->bytes_transferred
                   << ") > expected(" << update->total_bytes << ")";
        return;
      }
      VLOG(4) << "File transfer completion percentage: "
              << base::ClampFloor(
                     static_cast<double>(update->bytes_transferred) /
                     update->total_bytes * 100.f);
      return;
    }

    case ::nearby::connections::mojom::PayloadStatus::kCanceled:
    case ::nearby::connections::mojom::PayloadStatus::kFailure:
      LOG(ERROR) << "File transfer failed with status: " << update->status;
      CompleteTransfer(false);
      return;
  }
}

void FileReceiver::VerifyFileTransferResult(int64_t expected_size_in_bytes) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&DidFileTransferComplete, path_, expected_size_in_bytes),
      base::BindOnce(&FileReceiver::CompleteTransfer,
                     file_receiver_weak_factory_.GetWeakPtr()));
}

void FileReceiver::CompleteTransfer(bool verification_status) {
  if (!observer_.on_file_transfer_complete) {
    // This can only happen if the NC mojo service notifies us multiple times of
    // a successful or failed transfer.
    LOG(DFATAL) << "Received multiple payload completion status updates";
    return;
  }
  VLOG(1) << "File verification_status=" << verification_status;
  transfer_completed_successfully_ = verification_status;
  std::move(observer_.on_file_transfer_complete).Run(verification_status);
}

}  // namespace data_migration
