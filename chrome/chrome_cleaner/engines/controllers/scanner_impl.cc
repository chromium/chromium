// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/scanner_impl.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/broker/logging_conversion.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/settings/settings.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

ScannerImpl::ScannerImpl(EngineClient* engine_client)
    : engine_client_(engine_client),
      task_runner_(base::SequencedTaskRunnerHandle::Get()) {
  DCHECK(engine_client_);
}

ScannerImpl::~ScannerImpl() {
  CHECK(!is_scanning_);
}

bool ScannerImpl::Start(const FoundUwSCallback& found_uws_callback,
                        DoneCallback done_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_scanning_);
  is_scanning_ = true;

  DCHECK(found_uws_callback);
  DCHECK(done_callback);

  found_uws_callback_ = found_uws_callback;
  done_callback_ = std::move(done_callback);

  DCHECK(Settings::GetInstance()->scan_switches_correct());
  std::vector<UwS::TraceLocation> enabled_locations =
      Settings::GetInstance()->locations_to_scan();
  LoggingServiceAPI::GetInstance()->SetScannedLocations(enabled_locations);

  // base::Unretained is safe because this object is owned by EngineFacade,
  // which lives until program exit.
  uint32_t result = engine_client_->StartScan(
      engine_client_->GetEnabledUwS(), enabled_locations,
      /*include_details=*/true,
      base::BindRepeating(&ScannerImpl::OnFoundUwS, base::Unretained(this)),
      base::BindOnce(&ScannerImpl::OnScanDone, base::Unretained(this)));
  if (result != EngineResultCode::kSuccess) {
    OnScanDone(result);
    return false;
  }
  return true;
}

void ScannerImpl::Stop() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsCompletelyDone())
    return;
  // Don't actually try to cancel the sandboxed engine. It will simply be killed
  // as we exit.
  HandleScanDone(EngineResultCode::kCancelled);
}

bool ScannerImpl::IsCompletelyDone() const {
  return !is_scanning_;
}

void ScannerImpl::HandleFoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(is_scanning_);

  if (found_uws_.find(pup_id) == found_uws_.end()) {
    // Note this calls |found_uws_callback_| even for UwS that will be
    // discarded during HandleScanDone (eg. if it has no files or the ID is
    // unsupported.) This is necessary because |found_uws_callback_| updates
    // state for the watchdog timeout, so if we wait until HandleScanDone it
    // might be too late.
    found_uws_callback_.Run(pup_id);
  }

  found_uws_.insert(pup_id);

  // HandleScanDone will handle invalid UwS case when invoked. Postpone the
  // decision for the moment.
  if (!PUPData::IsKnownPUP(pup_id))
    return;

  PUPData::GetPUP(pup_id)->MergeFrom(pup);
}

void ScannerImpl::HandleScanDone(uint32_t result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_scanning_) {
    // TODO(joenotcharles): This used to be called twice if the user cancelled
    // after a HandleCleanupDone task had already been posted. Check if this is
    // still possible and remove the test if not.
    return;
  }

  // Log details of all UwS found during the scan. This must be done at the
  // end of the scan because the sandboxed engine updates files found for each
  // UwS in parallel.
  std::vector<UwSId> supported_uws;
  bool found_unsupported_uws = false;
  for (UwSId pup_id : found_uws_) {
    switch (UwSLoggingDecision(pup_id)) {
      case LoggingDecision::kUnsupported:
        found_unsupported_uws = true;
        break;

      case LoggingDecision::kNotLogged:
        // Do nothing.
        break;

      case LoggingDecision::kLogged:
        LoggingServiceAPI::GetInstance()->AddDetectedUwS(
            PUPData::GetPUP(pup_id), kUwSDetectedFlagsNone);
        supported_uws.push_back(pup_id);
        break;
    }
  }

  is_scanning_ = false;

  ResultCode result_code = found_unsupported_uws
                               ? RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS
                               : ScanningResultCode(result);
  std::move(done_callback_).Run(result_code, supported_uws);
}

void ScannerImpl::OnFoundUwS(UwSId pup_id, const PUPData::PUP& pup) {
  // base::Unretained is safe because this object is owned by EngineFacade,
  // which lives until program exit.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ScannerImpl::HandleFoundUwS,
                                        base::Unretained(this), pup_id, pup));
}

void ScannerImpl::OnScanDone(uint32_t result) {
  DCHECK_NE(result, EngineResultCode::kCancelled);  // Deprecated.
  engine_client_->MaybeLogResultCode(EngineClient::Operation::ScanDoneCallback,
                                     result);
  // base::Unretained is safe because this object is owned by EngineFacade,
  // which lives until program exit.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ScannerImpl::HandleScanDone,
                                        base::Unretained(this), result));
}

}  // namespace chrome_cleaner
