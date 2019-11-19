// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/engine_cleaner.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/engines/broker/logging_conversion.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/file_removal_status_updater.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

namespace {

void UpdateStatusOfMissingMatchedFiles(const std::vector<UwSId>& pup_ids) {
  for (const auto& pup_id : pup_ids) {
    PUPData::PUP* pup = PUPData::GetPUP(pup_id);
    for (const base::FilePath& file_path :
         pup->expanded_disk_footprints.file_paths()) {
      // If the file was explicitly and successfully removed before, its status
      // won't change. If the file was temporary and has disappeared on its own,
      // update its status to REMOVAL_STATUS_NOT_FOUND.
      if (!base::PathExists(file_path)) {
        FileRemovalStatusUpdater::GetInstance()->UpdateRemovalStatus(
            file_path, REMOVAL_STATUS_NOT_FOUND);
      }
    }
  }
}

}  // namespace

EngineCleaner::EngineCleaner(EngineClient* engine_client)
    : engine_client_(engine_client),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(engine_client);
}

EngineCleaner::~EngineCleaner() = default;

void EngineCleaner::Start(const std::vector<UwSId>& pup_ids,
                          DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(done_callback);
  DCHECK(!is_cleaning_);

  CHECK(CanClean(pup_ids)) << "Attempt to remove UwS not enabled for cleaning";

  done_callback_ = std::move(done_callback);
  is_cleaning_ = true;

  pups_to_clean_ = pup_ids;

  // base::Unretained is safe because this object is owned by EngineFacade,
  // which lives until program exit.
  uint32_t result = engine_client_->StartCleanup(
      pup_ids,
      base::BindOnce(&EngineCleaner::OnCleanupDone, base::Unretained(this)));

  if (result != EngineResultCode::kSuccess)
    OnCleanupDone(result);
}

void EngineCleaner::StartPostReboot(const std::vector<UwSId>& pup_ids,
                                    DoneCallback done_callback) {
  Start(pup_ids, std::move(done_callback));
}

void EngineCleaner::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsCompletelyDone())
    return;
  // Don't actually try to cancel the engine. It will simply be killed as
  // we exit.
  HandleCleanupDone(EngineResultCode::kCancelled);
}

bool EngineCleaner::IsCompletelyDone() const {
  return !is_cleaning_;
}

bool EngineCleaner::CanClean(const std::vector<UwSId>& pup_ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return PUPData::HasFlaggedPUP(pup_ids, &PUPData::HasRemovalFlag);
}

void EngineCleaner::HandleCleanupDone(uint32_t result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(joenotcharles): This used to be called twice if the user cancelled
  // after a HandleCleanupDone task had already been posted. Check if this is
  // still possible and remove the test if not.
  if (is_cleaning_) {
    is_cleaning_ = false;
    ResultCode engine_result =
        CleaningResultCode(result, engine_client_->needs_reboot());

    // Check if all files that were detected during the scanning were
    // successfully removed. This is a best-effort check, as the logging service
    // is not guaranteed to have information about all files.
    UpdateStatusOfMissingMatchedFiles(pups_to_clean_);
    if (!LoggingServiceAPI::GetInstance()->AllExpectedRemovalsConfirmed()) {
      LOG(ERROR) << "Engine failed to delete some of the detected files";

      // Override only successful result codes.
      // If the engine requested reboot (RESULT_CODE_PENDING_REBOOT), go for it
      // even if not all detected files were deleted, as we want the user to
      // delete scheduled for post-reboot removal files ASAP.
      if (engine_result == RESULT_CODE_SUCCESS)
        engine_result = RESULT_CODE_FAILED;
    }
    std::move(done_callback_).Run(engine_result);
  }
}

void EngineCleaner::OnCleanupDone(uint32_t result) {
  DCHECK_NE(result, EngineResultCode::kCancelled);  // Deprecated.
  engine_client_->MaybeLogResultCode(EngineClient::Operation::ScanDoneCallback,
                                     result);
  // base::Unretained is safe because this object is owned by EngineFacade,
  // which lives until program exit.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&EngineCleaner::HandleCleanupDone,
                                        base::Unretained(this), result));
}

}  // namespace chrome_cleaner
