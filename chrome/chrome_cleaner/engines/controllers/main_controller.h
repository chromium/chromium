// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_MAIN_CONTROLLER_H_
#define CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_MAIN_CONTROLLER_H_

#include <windows.h>

#include <restartmanager.h>

#include <map>
#include <memory>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string16.h"
#include "base/synchronization/lock.h"
#include "base/threading/watchdog.h"
#include "chrome/chrome_cleaner/cleaner/cleaner.h"
#include "chrome/chrome_cleaner/components/component_api.h"
#include "chrome/chrome_cleaner/components/component_manager.h"
#include "chrome/chrome_cleaner/constants/uws_id.h"
#include "chrome/chrome_cleaner/engines/controllers/engine_facade_interface.h"
#include "chrome/chrome_cleaner/ipc/chrome_prompt_ipc.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/scanner/scanner.h"
#include "chrome/chrome_cleaner/settings/settings_types.h"
#include "chrome/chrome_cleaner/ui/main_dialog_api.h"
#include "components/chrome_cleaner/public/constants/result_codes.h"

namespace chrome_cleaner {

class ComponentAPI;
class RebooterAPI;
class RegistryLogger;

enum class TimedOutStage {
  kScanning,
  kWaitingForPrompt,
  kCleaning,
};

// The class used by the main entry point to connect the dots between the UI,
// the scanner, cleaner and other components. It works as a state machine in the
// following manner:
// - Starts with either a ScanAndClean() or a ValidateCleanup()
//   - ScanAndClean()
//     - Set the "main dialog" to scan mode. (Actually a legacy name, this
//       class now delegates UI actions to Chrome.)
//     - Call the Component Manager's PreScan method.
//     - Called back when PreScan is done for all components.
//       - Start the scanner and run the main message loop.
//       - Called back when scanning is done.
//         - Call the Component Manager's PostScan method.
//         - Called back when PostScan is done for all components.
//           - If no PUPs are found let the user know about it, wait for OnClose
//             to be called, and return proper exit code.
//           - If PUPs are found, get the cleanup requirements.
//             - If the requirements failed, do as if No PUPs was found.
//             - Once requirements are confirmed, the user is asked for confirm
//               the cleanup, wait for answer callback.
//               - If user refuses, exit with proper exit code.
//               - If user accepts, the pre-cleanup custom actions are executed.
//                 - If one of the actions fails, exit with proper exit code.
//                 - Call the Component Manager's PreCleanup method.
//                 - Called back when PreCleanup is done for all components.
//                   - Set the main dialog to cleanup mode.
//                   - Start Cleaner, and wait for done call back.
//                   - Once cleanup completes, call the Component Manager's
//                     PostCleanup method.
//                   - Called back when PostCleanup is done for all components.
//                     - Show the final UI to user, and wait for OnClose to be
//                       called.
//                     - Call the Component Manager's CloseAllComponents method.
//                     - Return proper exit code.
//   - ValidateCleanup()
//     - Start the scanner and run the main message loop.
//     - Called back when scanning is done.
//       - If nothing was found, exit with proper exit code.
//       - If something was found, make a last attempt at removing files that
//         would have been left over before rebooting.
//
// TODO(joenotcharles): Clean this chain of callbacks up a lot. This looks like
// a great case for futures!
class MainController : public ComponentManagerDelegate,
                       public MainDialogDelegate {
 public:
  // Provide conditional object at construction time for easier mocking and to
  // do the reporter / cleaner duality resolution outside the controller.
  // Ownership of |signature_matcher| and |registry_logger| are NOT transferred
  // to this class so the passed instances MUST outlive this MainController
  // instance. RegistryLogger has internal state, so cannot be const-reference,
  // cannot be null and must outlive the MainController.
  MainController(RebooterAPI* rebooter,
                 RegistryLogger* registry_logger,
                 ChromePromptIPC* chrome_prompt_ipc);
  ~MainController() override;

  // Add a new component to be called before the scan and after the cleanup. The
  // MainController takes ownership of the component.
  void AddComponent(std::unique_ptr<ComponentAPI> component);

  // Sets the engine that will be used for scanning and cleaning.
  void SetEngineFacade(EngineFacadeInterface* engine_facade);

  // Start a new scan.
  ResultCode ScanAndClean();

  // Start a post-reboot validation scan.
  ResultCode ValidateCleanup();

  // Returns a callback that should be called with the sandbox type when an IPC
  // connection to a sandbox closes and will terminate the process.
  SandboxConnectionErrorCallback GetSandboxConnectionErrorCallback();

  virtual MainDialogAPI* main_dialog();

 protected:
  // Allow tests to override watchdog timeouts.
  base::TimeDelta scanning_watchdog_timeout_;
  base::TimeDelta user_response_watchdog_timeout_;
  base::TimeDelta cleaning_watchdog_timeout_;

 private:
  class ChromePromptConnectionErrorHandler;
  friend class ChromePromptConnectionErrorHandler;

  static void CallSandboxConnectionClosed(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      base::WeakPtr<MainController> main_controller,
      SandboxType sandbox_type);

  void FoundUwSCallback(UwSId pup_id);

  // The callback for a regular/new scan. |found_pups| contains the
  // IDs of the found PUPs.
  void DoneScanning(ResultCode status, const std::vector<UwSId>& found_pups);

  // The callback when running a post-reboot validating scan.
  void DoneValidating(ResultCode status, const std::vector<UwSId>& found_pups);

  // The callback when running last-attempt cleaning after validating scan.
  void DoneCleanupValidation(ResultCode validation_code);

  // Interrupts scan on broken connection error. This will be posted by
  // ConnectionErrorHandler::OnConnectionClosed() on the thread used by main
  // controller.
  void OnChromePromptConnectionClosed();

  void OnSandboxConnectionClosed(SandboxType sandbox_type);

  // The callback for regular cleanup.
  void CleanupDone(ResultCode clean_result);

  // ComponentManagerDelegate.
  void PreScanDone() override;
  void PostScanDone() override;
  void PreCleanupDone() override;
  void PostCleanupDone() override;

  // MainDialogDelegate implementation.
  void AcceptedCleanup(bool confirmed) override;
  void OnClose() override;

  // Closes the main dialog.
  void Quit();

  // Uploads logs to SafeBrowsing. |tag| must be unique (UploadLogs cannot be
  // called with the same |tag| multiple times). If the raw logs are being
  // dumped to disk, |tag| will be appended to the filename. If |quit_when_done|
  // is true, then the current UI message loop will be told to QuitWhenIdle
  // after all the log uploads have completed.
  void UploadLogs(const base::string16& tag, bool quit_when_done);

  // Handles the completion of a logs upload.
  void LogsUploadComplete(const base::string16& tag, bool success);

  // Logs |exit_code| and any other metrics that should be saved on an early
  // exit, such as a watchdog timeout or sandbox error.
  void WriteEarlyExitMetrics(ResultCode exit_code);

  // Callback for the Watchdog timeout.
  int WatchdogTimeoutCallback(TimedOutStage timeout_stage);

  Scanner* scanner();
  Cleaner* cleaner();

  // The API to force a reboot.
  RebooterAPI* rebooter_;

  // The manager of components to be called before / after the scan / cleanup.
  ComponentManager component_manager_;

  // The objects that needed to be connected together by the controller.
  std::unique_ptr<MainDialogAPI> main_dialog_;
  EngineFacadeInterface* engine_facade_ = nullptr;

  // The PUPs that were found by the Scanner.
  std::vector<chrome_cleaner::UwSId> found_pups_;

  SEQUENCE_CHECKER(sequence_checker_);

  // The result to be returned to our caller.
  ResultCode result_code_;

  // Tracks the progress of multiple logs uploads.
  std::map<base::string16, bool> logs_upload_complete_;

  // Whether we should quit after all the logs uploads are done or not.
  bool quit_when_logs_upload_complete_ = false;

  // Pointer to the RegistryLogger used by this class to log upload results.
  RegistryLogger* registry_logger_;

  // Start times of the scanning and cleaning phases.
  base::Time scan_start_time_;
  base::Time clean_start_time_;
  // Indicate whether or not we have finished the scanning and cleaning phases.
  bool finished_scanning_ = false;
  bool finished_cleaning_ = false;
  // Indicate whether removable UwS were found.
  bool removable_uws_found_ = false;

  // Watchdog for terminating if scanning or cleaning takes too long.
  std::unique_ptr<base::Watchdog> watchdog_;

  // Quit closure to indicate the current scan & cleanup or validation is done.
  base::OnceClosure quit_closure_;

  // The connection error handlers gets a weak pointer for the main controller,
  // so that it doesn't try to access the object once it's destroyed.
  base::WeakPtrFactory<MainController> weak_factory_{this};
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ENGINES_CONTROLLERS_MAIN_CONTROLLER_H_
