// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/engines/target/engine_commands_impl.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "chrome/chrome_cleaner/crash/crash_keys.h"
#include "chrome/chrome_cleaner/engines/target/cleaner_engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_cleanup_results_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_file_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_requests_proxy.h"
#include "chrome/chrome_cleaner/engines/target/engine_scan_results_proxy.h"

namespace chrome_cleaner {

namespace {

constexpr char kStageCrashKey[] = "stage";

class ScopedCrashStageRecorder {
 public:
  explicit ScopedCrashStageRecorder(const std::string& stage) : stage_(stage) {
    SetCrashKey(kStageCrashKey, stage_);
  }

  ~ScopedCrashStageRecorder() {
    stage_ += "-done";
    SetCrashKey(kStageCrashKey, stage_);
  }

 private:
  std::string stage_;

  DISALLOW_COPY_AND_ASSIGN(ScopedCrashStageRecorder);
};

}  // namespace

EngineCommandsImpl::EngineCommandsImpl(
    scoped_refptr<EngineDelegate> engine_delegate,
    mojo::PendingReceiver<mojom::EngineCommands> receiver,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::OnceClosure error_handler)
    : engine_delegate_(engine_delegate),
      receiver_(this, std::move(receiver)),
      task_runner_(task_runner) {
  receiver_.set_disconnect_handler(std::move(error_handler));
}

EngineCommandsImpl::~EngineCommandsImpl() = default;

void EngineCommandsImpl::Initialize(
    mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
    const base::FilePath& log_directory_path,
    InitializeCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(std::move(file_requests),
                                                    task_runner_);

  // This object is not retained because it outlives the callback: it's
  // destroyed on this sequence, once the main thread returns, which should only
  // happen after initialization has completed. If the broker process
  // terminates, then this process will be also be terminated by the connection
  // error handler, and there is not need to add complexity to handle it.
  engine_delegate_->Initialize(
      log_directory_path, file_requests_proxy,
      base::BindOnce(&EngineCommandsImpl::PostInitializeCallback,
                     base::Unretained(this), base::Passed(&callback)));
}

void EngineCommandsImpl::StartScan(
    const std::vector<UwSId>& enabled_uws,
    const std::vector<UwS::TraceLocation>& enabled_trace_locations,
    bool include_details,
    mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
    mojo::PendingAssociatedRemote<mojom::EngineRequests>
        sandboxed_engine_requests,
    mojo::PendingAssociatedRemote<mojom::EngineScanResults> scan_results,
    StartScanCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(std::move(file_requests),
                                                    task_runner_);

  scoped_refptr<EngineRequestsProxy> engine_requests_proxy =
      base::MakeRefCounted<EngineRequestsProxy>(
          std::move(sandboxed_engine_requests), task_runner_);

  // Create an EngineScanResults proxy to send results back over the
  // Mojo connection.
  scoped_refptr<EngineScanResultsProxy> scan_results_proxy =
      base::MakeRefCounted<EngineScanResultsProxy>(std::move(scan_results),
                                                   task_runner_);

  uint32_t result_code = engine_delegate_->StartScan(
      enabled_uws, enabled_trace_locations, include_details,
      file_requests_proxy, engine_requests_proxy, scan_results_proxy);
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::StartCleanup(
    const std::vector<UwSId>& enabled_uws,
    mojo::PendingAssociatedRemote<mojom::EngineFileRequests> file_requests,
    mojo::PendingAssociatedRemote<mojom::EngineRequests>
        sandboxed_engine_requests,
    mojo::PendingAssociatedRemote<mojom::CleanerEngineRequests>
        sandboxed_cleaner_engine_requests,
    mojo::PendingAssociatedRemote<mojom::EngineCleanupResults> cleanup_results,
    StartCleanupCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);

  // Create proxies to pass requests to the broker process over Mojo.
  scoped_refptr<EngineFileRequestsProxy> file_requests_proxy =
      base::MakeRefCounted<EngineFileRequestsProxy>(std::move(file_requests),
                                                    task_runner_);

  scoped_refptr<EngineRequestsProxy> engine_requests_proxy =
      base::MakeRefCounted<EngineRequestsProxy>(
          std::move(sandboxed_engine_requests), task_runner_);

  scoped_refptr<CleanerEngineRequestsProxy> cleaner_engine_requests_proxy =
      base::MakeRefCounted<CleanerEngineRequestsProxy>(
          std::move(sandboxed_cleaner_engine_requests), task_runner_);

  // Create an EngineCleanupResults proxy to send results back over the
  // Mojo connection.
  scoped_refptr<EngineCleanupResultsProxy> cleanup_results_proxy =
      base::MakeRefCounted<EngineCleanupResultsProxy>(
          std::move(cleanup_results), task_runner_);

  uint32_t result_code = engine_delegate_->StartCleanup(
      enabled_uws, file_requests_proxy, engine_requests_proxy,
      cleaner_engine_requests_proxy, cleanup_results_proxy);
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::Finalize(FinalizeCallback callback) {
  ScopedCrashStageRecorder crash_stage(__func__);
  uint32_t result_code = engine_delegate_->Finalize();
  std::move(callback).Run(result_code);
}

void EngineCommandsImpl::PostInitializeCallback(
    mojom::EngineCommands::InitializeCallback callback,
    uint32_t result_code) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), result_code));
}

}  // namespace chrome_cleaner
