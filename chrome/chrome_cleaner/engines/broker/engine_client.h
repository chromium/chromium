// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_H_
#define CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_H_

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/broker/cleaner_engine_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_cleanup_results_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_file_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_requests_impl.h"
#include "chrome/chrome_cleaner/engines/broker/engine_scan_results_impl.h"
#include "chrome/chrome_cleaner/engines/broker/interface_metadata_observer.h"
#include "chrome/chrome_cleaner/engines/common/engine_result_codes.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/mojom/engine_sandbox.mojom.h"
#include "chrome/chrome_cleaner/pup_data/pup_data.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace chrome_cleaner {

// Singleton interface to interact with the sandboxed engine.
class EngineClient : public base::RefCountedThreadSafe<EngineClient> {
 public:
  // An engine call. Values from this enum are used for histograms through the
  // registry logger. Do not re-use existing values.
  enum class Operation {
    UNKNOWN = 0,
    Initialize = 1,
    DEPRECATED_RegisterTargetProcess = 2,
    DEPRECATED_TargetProcessStarted = 3,
    DEPRECATED_Shutdown = 4,
    DEPRECATED_InitializeSandboxTarget = 5,
    DEPRECATED_StartSandboxTarget = 6,
    DEPRECATED_GetVersion = 7,
    DEPRECATED_GetSupportedUwS = 8,
    StartScan = 9,
    ScanDoneCallback = 10,
    StartCleanup = 11,
    CleanupDoneCallback = 12,
    DEPRECATED_CancelScan = 13,
    Finalize = 14,
    DEPRECATED_RebootAndRerun = 15,
    DEPRECATED_StartPostRebootCleanup = 16,
    DEPRECATED_PostRebootCleanupDoneCallback = 17,
    DEPRECATED_EndPostRebootCleanup = 18,
    // Values for EngineOperations go here.
    // Keep in sync with EngineOperationStatus.Operation enum in proto.

    // The SoftwareReporter.EngineErrorCode histogram logic requires that the
    // EngineOperation occupies at most 16 bits.
    MAX_OPERATION = 2 ^ 16 - 1,
  };

  typedef base::RepeatingCallback<void(int)> ResultCodeLoggingCallback;

  // Creates an EngineClient which will call the given |logging_callback| to
  // log result codes after every operation. IPC tasks will be posted with
  // |mojo_task_runner|, which must not be null. If
  // |interface_metadata_observer| is not nullptr its ObserveCall method will
  // be called with information about all engine calls.
  static scoped_refptr<EngineClient> CreateEngineClient(
      const Engine::Name engine,
      const ResultCodeLoggingCallback& logging_callback,
      const SandboxConnectionErrorCallback& connection_error_callback,
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      std::unique_ptr<InterfaceMetadataObserver> interface_metadata_observer =
          nullptr);

  // Return the watchdog timeout that should be used for scanning using this
  // client.
  uint32_t ScanningWatchdogTimeoutInSeconds() const;

  mojo::Remote<mojom::EngineCommands>* engine_commands_remote() const {
    return engine_commands_.get();
  }

  // Posts a task to the mojo thread to bind an EngineCommands remote to |pipe|.
  // |error_handler| will be called for errors on this connection.
  //
  // TODO(joenotcharles): When the EngineClient interface is updated to be
  // asynchronous, rename Initialize, StartScan, StartCleanup, and Finalize to
  // match this naming scheme.
  //
  // CAREFUL: some methods (eg. StartScan), have callback parameters and the
  // caller (eg. ScannerImpl::Start) binds tasks to the callback using
  // base::Unretained. This is safe right now because StartScan is synchronous
  // so the callbacks are called while it is still executing, so ScannerImpl
  // definitely still exists. But if StartScan is made asynchronous, we need to
  // make sure that all callbacks are still valid before calling them. (This
  // could happen during shutdown when ScannerImpl has been deleted but
  // EngineClient is still being kept alive because a StartScanAsync task
  // that's still queued has a reference to it.)
  virtual void PostBindEngineCommandsRemote(mojo::ScopedMessagePipeHandle pipe);

  using FoundUwSCallback = EngineScanResultsImpl::FoundUwSCallback;
  using DoneCallback = EngineScanResultsImpl::DoneCallback;

  // Return the list of UwS enabled for this engine.
  virtual std::vector<UwSId> GetEnabledUwS() const;

  virtual uint32_t Initialize();

  // Note that |DoneCallback| should call MaybeLogResultCode().
  virtual uint32_t StartScan(
      const std::vector<UwSId>& enabled_uws,
      const std::vector<UwS::TraceLocation>& enabled_locations,
      bool include_details,
      FoundUwSCallback found_callback,
      DoneCallback done_callback);

  // Note that |DoneCallback| should call MaybeLogResultCode().
  virtual uint32_t StartCleanup(const std::vector<UwSId>& enabled_uws,
                                DoneCallback done_callback);

  virtual uint32_t Finalize();

  // Logs operation and result_code to the registry so Chrome later uploads them
  // as one UMA histogram. Successive calls to this method will clobber the
  // registry value until |result_code| is not SUCCESS.
  //
  // TODO(joenotcharles): Make this method private. StartScan(), StartCleanup()
  // and StartPostRebootCleanup() can wrap |DoneCallback| around another
  // callback which calls this method.
  virtual void MaybeLogResultCode(Operation operation, uint32_t result_code);

  virtual bool needs_reboot() const;

 protected:
  friend class base::RefCountedThreadSafe<EngineClient>;

  EngineClient(
      Engine::Name engine,
      const ResultCodeLoggingCallback& logging_callback,
      const SandboxConnectionErrorCallback& connection_error_callback,
      scoped_refptr<MojoTaskRunner> mojo_task_runner,
      std::unique_ptr<InterfaceMetadataObserver> metadata_observer = nullptr);

  virtual ~EngineClient();

 private:
  friend class ExtensionCleanupTest;
  using InitializeCallback = mojom::EngineCommands::InitializeCallback;
  using StartScanCallback = mojom::EngineCommands::StartScanCallback;
  using StartCleanupCallback = mojom::EngineCommands::StartCleanupCallback;
  using FinalizeCallback = mojom::EngineCommands::FinalizeCallback;

  void BindEngineCommandsRemote(mojo::ScopedMessagePipeHandle pipe,
                                base::OnceClosure error_handler);

  void InitializeReadOnlyCallbacks();
  bool InitializeCleaningCallbacks();
  bool InitializeQuarantine(std::unique_ptr<ZipArchiver>* archiver);

  // TODO(joenotcharles): When the synchronous Initialize method is removed,
  // rename this to Initialize and name the public accessor PostInitialize.
  // Same for StartScan and similar methods.
  void InitializeAsync(InitializeCallback result_callback);

  void StartScanAsync(const std::vector<UwSId>& enabled_uws,
                      const std::vector<UwS::TraceLocation>& enabled_locations,
                      bool include_details,
                      FoundUwSCallback found_callback,
                      DoneCallback done_callback,
                      StartScanCallback result_callback);

  void StartCleanupAsync(const std::vector<UwSId>& enabled_uws,
                         DoneCallback done_callback,
                         StartCleanupCallback result_callback);

  void FinalizeAsync(FinalizeCallback result_callback);

  void SetRebootRequired();

  void ResetCreatedInstanceCheckForTesting();

  Engine::Name engine_;

  // Keeps track of whether an operation was started (with a call to a Start*
  // method) but not ended (with a matching call to the End* method). Ensures
  // that we never shutdown while an operation is in progress. Also serves for
  // some debug checks.
  bool operation_in_progress_ = false;

  // Keeps track of the last result code stored in the registry.
  uint32_t cached_result_code_ = EngineResultCode::kSuccess;

  // The callback to call to report the experimental engine result code.
  ResultCodeLoggingCallback registry_logging_callback_;

  // The callback to call to report the sandbox connection error.
  SandboxConnectionErrorCallback connection_error_callback_;

  // Task runner used to send IPC commands to the sandbox target process.
  scoped_refptr<MojoTaskRunner> mojo_task_runner_;

  // Proxy object that implements the EngineCommands interface by sending the
  // commands over IPC to the sandbox target process.
  std::unique_ptr<mojo::Remote<mojom::EngineCommands>> engine_commands_;

  // Handler for scan results returned over the Mojo pipe.
  std::unique_ptr<EngineScanResultsImpl> scan_results_impl_;

  // Handler for cleanup results returned over the Mojo pipe.
  std::unique_ptr<EngineCleanupResultsImpl> cleanup_results_impl_;

  // Handler for file reading requests from the sandbox that have to run
  // outside the sandbox.
  std::unique_ptr<EngineFileRequestsImpl> sandbox_file_requests_;

  // Handler for requests from the sandbox that have to run outside the sandbox.
  std::unique_ptr<EngineRequestsImpl> sandbox_requests_;

  // Handler for cleaning requests from the sandbox that have to run outside the
  // sandbox.
  std::unique_ptr<CleanerEngineRequestsImpl> sandbox_cleaner_requests_;

  // Allow tests overwrite the archiver used.
  std::unique_ptr<ZipArchiver> archiver_for_testing_;

  // Keep track of if this cleaning requires a reboot to be fully completed.
  bool needs_reboot_ = false;

  // Keeps track of the calls of both EngineRequestsImpl and
  // CleanerEngineRequestsImpl.
  std::unique_ptr<InterfaceMetadataObserver> interface_metadata_observer_;

  DISALLOW_COPY_AND_ASSIGN(EngineClient);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_BROKER_ENGINE_CLIENT_H_
