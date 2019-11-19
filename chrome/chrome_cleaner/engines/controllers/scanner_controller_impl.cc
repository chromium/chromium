// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/controllers/scanner_controller_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/broker/logging_conversion.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/logging/logging_service_api.h"
#include "chrome/chrome_cleaner/parsers/shortcut_parser/broker/shortcut_parser_api.h"

namespace chrome_cleaner {

ScannerControllerImpl::ScannerControllerImpl(
    EngineClient* engine_client,
    RegistryLogger* registry_logger,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    ShortcutParserAPI* shortcut_parser)
    : ScannerController(registry_logger, shortcut_parser),
      scanner_(engine_client),
      engine_client_(engine_client),
      task_runner_(task_runner) {
  watchdog_timeout_in_seconds_ =
      engine_client_->ScanningWatchdogTimeoutInSeconds();
}

ScannerControllerImpl::~ScannerControllerImpl() {
  CHECK(!base::RunLoop::IsRunningOnCurrentThread());
  // TODO(joenotcharles) Cleanup RunUntilIdle usage in loops.
  while (state_ != State::kIdle)
    base::RunLoop().RunUntilIdle();
}

void ScannerControllerImpl::StartScan() {
  DCHECK_EQ(State::kIdle, state_);

  // Set the state to non-Idle, so that HandleScanDone will not assert, but
  // not yet to kScanningInProgress because engine_client_->Finalize should not
  // be called unless engine_client_->StartScan returns SUCCESS.
  state_ = State::kScanningStarting;

  // base::Unretained is safe because this object lives until program exit.
  if (scanner_.Start(base::BindRepeating(&ScannerControllerImpl::OnFoundUwS,
                                         base::Unretained(this)),
                     base::BindOnce(&ScannerControllerImpl::OnScanDone,
                                    base::Unretained(this)))) {
    state_ = State::kScanningInProgress;
  }
}

int ScannerControllerImpl::WatchdogTimeoutCallback() {
  if (!IsSandboxTargetRunning(SandboxType::kEngine)) {
    engine_client_->MaybeLogResultCode(EngineClient::Operation::UNKNOWN,
                                       EngineResultCode::kSandboxUnavailable);
  }

  ResultCode exit_code = ScannerController::WatchdogTimeoutCallback();
  HandleWatchdogTimeout(exit_code);
  return exit_code;
}

void ScannerControllerImpl::OnFoundUwS(UwSId pup_id) {
  // base::Unretained is safe because this object lives until program exit.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ScannerControllerImpl::UpdateResultsOnFoundUwS,
                                base::Unretained(this), pup_id));
}

void ScannerControllerImpl::OnScanDone(ResultCode result_code,
                                       const std::vector<UwSId>& found_uws) {
  // base::Unretained is safe because this object lives until program exit.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ScannerControllerImpl::HandleScanDone,
                     base::Unretained(this), result_code, found_uws));
}

void ScannerControllerImpl::UpdateResultsOnFoundUwS(UwSId pup_id) {
  // TODO(joenotcharles): This has an early return because, in theory, this
  // could be triggered in the wrong state due to bugs in the sandboxed engine.
  // This should really be enforced in engine_commands_impl.cc and just be a
  // CHECK.
  if (!IsScanningInProgress()) {
    NOTREACHED() << "AddFoundUwS called while scanning was not in progress";
    return;
  }

  // No need to update the result code if no new PUP is found.
  if (pup_ids_.find(pup_id) != pup_ids_.end())
    return;

  // Note that the scan results will include PUP's that would be dropped from
  // the list in HandleScanDone (eg. if they have no files or their ID is
  // unsupported). This is ok because HandleScanDone will overwrite the
  // scan results with the final list, and in the meantime the list should
  // contain everything that might be reported in case the watchdog times
  // out.
  pup_ids_.insert(pup_id);
  UpdateScanResults(std::vector<UwSId>(pup_ids_.begin(), pup_ids_.end()));
}

void ScannerControllerImpl::HandleScanDone(
    ResultCode result_code,
    const std::vector<UwSId>& found_uws) {
  // TODO(joenotcharles): This has an early return because, in theory, this
  // could be triggered in the wrong state due to bugs in the sandboxed engine.
  // This should really be enforced in engine_commands_impl.cc and just be a
  // CHECK.
  if (state_ == State::kIdle) {
    NOTREACHED() << "HandleScanDone called twice, most likely from "
                    "StartScan returning an error and calling the callbacks "
                    "anyways.";
    return;
  }

  if (IsScanningInProgress())
    engine_client_->Finalize();

  state_ = State::kIdle;
  DoneScanning(result_code, found_uws);
}

bool ScannerControllerImpl::IsScanningInProgress() const {
  return state_ == State::kScanningInProgress ||
         state_ == State::kScanningFinishing;
}

}  // namespace chrome_cleaner
