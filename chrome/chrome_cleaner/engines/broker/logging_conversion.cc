// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/broker/logging_conversion.h"

#include "base/logging.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"

namespace chrome_cleaner {

LoggingDecision UwSLoggingDecision(UwSId pup_id) {
  if (!PUPData::IsKnownPUP(pup_id)) {
    LOG(ERROR) << "Found unsupported UwS " << pup_id;
    return LoggingDecision::kUnsupported;
  }

  PUPData::PUP* pup = PUPData::GetPUP(pup_id);

  // In case no matched file still exists, consider the UwS as not matched.
  pup->expanded_disk_footprints.DiscardNonExistingFiles();

  if (pup->expanded_disk_footprints.size() == 0) {
    LOG(INFO) << "Detected " << pup->signature().name
              << ", but not logging because files are missing.";
    return LoggingDecision::kNotLogged;
  }

  LOG(INFO) << "Detected " << pup->signature().name;
  return LoggingDecision::kLogged;
}

ResultCode ScanningResultCode(uint32_t engine_operation_status) {
  // IMPORTANT: When adding new possible exit codes here, you may need to update
  // their handling in MainController::DoneScanning,
  // MainController::DoneValidation and MainController::DoneCleanupValidation.
  LOG(INFO) << "Engine Scan exited with status code 0x" << std::hex
            << engine_operation_status << std::dec;
  if (engine_operation_status == EngineResultCode::kCancelled)
    return RESULT_CODE_CANCELED;
  if (engine_operation_status != EngineResultCode::kSuccess)
    return RESULT_CODE_SCANNING_ENGINE_ERROR;
  return RESULT_CODE_SUCCESS;
}

ResultCode CleaningResultCode(uint32_t result_code, bool needs_reboot) {
  LOG(INFO) << "Engine cleanup exited with status code 0x" << std::hex
            << result_code << std::dec;
  if (result_code == EngineResultCode::kCancelled)
    return RESULT_CODE_CANCELED;
  if (result_code == EngineResultCode::kCleaningFailed) {
    LOG(ERROR) << "Engine failed to clean the system";
    return RESULT_CODE_FAILED;
  }
  if (result_code != EngineResultCode::kSuccess)
    return RESULT_CODE_FAILED;
  if (needs_reboot)
    return RESULT_CODE_PENDING_REBOOT;
  return RESULT_CODE_SUCCESS;
}

}  // namespace chrome_cleaner
