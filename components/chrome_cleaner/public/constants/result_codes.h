// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_RESULT_CODES_H_
#define COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_RESULT_CODES_H_

#include <limits>

namespace chrome_cleaner {

// The exit code of a scanner or cleaner process, containing the result of a
// scanning or cleaning session.
typedef int ResultCode;

// A list of standard result codes.
enum ResultCodeValues : ResultCode {
  // Generic success.
  RESULT_CODE_SUCCESS = 0,

  // Generic failure.
  RESULT_CODE_FAILED = 1,

  // The scanner didn't find any UwS.
  RESULT_CODE_NO_PUPS_FOUND = 2,

  // The scanning or cleanup has been canceled.
  RESULT_CODE_CANCELED = 3,

  // DEPRECATED. Cleanup succeeded and a reboot is about to happen to complete
  // it.
  // This value should not be used in new code, but is still defined for use in
  // Chromium code that reads results from older versions of the cleaner.
  DEPRECATED_RESULT_CODE_ABOUT_TO_REBOOT = 4,

  // Could not register the application to be automatically rerun after a
  // reboot to confirm post-reboot success.
  RESULT_CODE_RUNONCE_REGISTRATION_FAILED = 5,

  // Shutdown could not be initiated, so a reboot won't automatically happen.
  RESULT_CODE_INITIATE_SHUTDOWN_FAILED = 6,

  // Failure to clean a registry entry.
  RESULT_CODE_REGISTRY_CLEAN_FAILED = 7,

  // Failure to remove a file.
  RESULT_CODE_DISK_CLEAN_FAILED = 8,

  // Failed to use the restart manager.
  RESULT_CODE_RESTART_MANAGER_FAILED = 9,

  // Found only UwS which has been fully examined for removal but not yet
  // enabled.  Older versions also used this for report-only, which is now
  // RESULT_CODE_REPORT_ONLY_PUPS_FOUND.
  RESULT_CODE_EXAMINED_FOR_REMOVAL_ONLY = 10,

  // Failure to check cleanup requirements.
  RESULT_CODE_CLEANUP_REQUIREMENTS_FAILED = 11,

  // To identify that we didn't exit yet, e.g., when we send intermediate logs
  // reports.
  RESULT_CODE_PENDING = 12,

  // To identify that a custom action failed.
  RESULT_CODE_CUSTOM_ACTION_FAILED = 13,

  // To identify a success that happened post-reboot.
  RESULT_CODE_POST_REBOOT_SUCCESS = 14,

  // A reboot is required to finish, but the user hasn't accepted it yet.
  RESULT_CODE_PENDING_REBOOT = 15,

  // To identify that the user refused to elevate the post-reboot run.
  RESULT_CODE_POST_REBOOT_ELEVATION_DENIED = 16,

  // To identify that the app is not running elevated, even though the command
  // line says that it should be. Note: this does not indicate that the user
  // declined the elevation prompt.
  RESULT_CODE_FAILED_TO_ELEVATE = 17,

  // To identify that we failed to read the upload logs file.
  RESULT_CODE_FAILED_TO_READ_UPLOAD_LOGS_FILE = 18,

  // To identify that we failed to upload pending logs.
  RESULT_CODE_FAILED_TO_UPLOAD_LOGS = 19,

  // To identify that we succeeded uploading pending logs.
  RESULT_CODE_UPLOADED_PENDING_LOGS = 20,

  // Could not register the application to be automatically rerun after a
  // reboot to confirm post-reboot success.
  RESULT_CODE_TASK_SCHEDULING_FAILED = 21,

  // Reports when another instance of Chrome Cleanup is already running.
  RESULT_CODE_ALREADY_RUNNING = 22,

  // This code is used when a scheduled task runs because a cleanup never got to
  // start.
  RESULT_CODE_FAILED_TO_START_CLEANUP = 23,

  // This code is used when a scheduled task runs because a cleanup wasn't
  // completed.
  RESULT_CODE_FAILED_TO_COMPLETE_CLEANUP = 24,

  // This code is used when the |kUploadLogFileSwitch| was specified but there
  // was no client identifier to use.
  RESULT_CODE_EMPTY_CLIENT_ID_UPLOAD_ATTEMPT = 25,

  // DEPRECATED. Report-inhibiting PUP found. This category of PUP is no longer
  // used.
  // 26

  // DEPRECATED. Removal-inhibiting PUP found. This category of PUP is no
  // longer used.
  // 27

  // This code is used when the reporter didn't find any UwS but the signature
  // matcher was invalid.
  RESULT_CODE_NO_PUPS_FOUND_NO_SIGNATURE_MATCHER = 28,

  // This code is used when the validation step detects that one or more PUP
  // files are remaining after a cleanup and fails to remove them.
  RESULT_CODE_POST_CLEANUP_VALIDATION_FAILED = 29,

  // DEPRECATED. The Kasko reporter failed to initialize.
  // 30

  // DEPRECATED. Replaced with
  // RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS or
  // RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS.
  // 31

  // Found PUPs which we currently only report.
  RESULT_CODE_REPORT_ONLY_PUPS_FOUND = 32,

  // Logs upload from the Crashpad crash handler.
  RESULT_CODE_CRASH_HANDLER = 33,

  // The process failed to complete in a reasonable amount of time and our
  // watchdog terminated the process. At that time no removable UwS had been
  // found.
  RESULT_CODE_WATCHDOG_TIMEOUT_WITHOUT_REMOVABLE_UWS = 34,

  // The process failed to complete in a reasonable amount of time and our
  // watchdog terminated the process. At that time removable UwS had been found.
  RESULT_CODE_WATCHDOG_TIMEOUT_WITH_REMOVABLE_UWS = 35,

  // Identifies when something has failed when initializing/setting up the
  // engine.
  RESULT_CODE_ENGINE_INITIALIZATION_FAILED = 36,

  // Identifies failures when starting the sandbox process.
  RESULT_CODE_FAILED_TO_START_SANDBOX_PROCESS = 37,

  // Identifies that the scanner reported a found UwS ID we don't support.
  RESULT_CODE_ENGINE_REPORTED_UNSUPPORTED_UWS = 38,

  // Identifies that an initialization error occurred.
  RESULT_CODE_INITIALIZATION_ERROR = 39,

  // Identifies when the scan did not complete because the engine returned an
  // error during the scan.
  RESULT_CODE_SCANNING_ENGINE_ERROR = 40,

  // Identifies that the verification of the signature on loaded libraries has
  // failed.
  RESULT_CODE_SIGNATURE_VERIFICATION_FAILED = 41,

  // Chrome IPC switches were passed to the command line but the execution mode
  // is not kScanning.
  RESULT_CODE_EXPECTED_SCANNING_EXECUTION_MODE = 42,

  // Chrome only sent one of the two expected switches corresponding to the
  // prompt IPC.
  RESULT_CODE_INVALID_IPC_SWITCHES = 43,

  // The parent process disconnected from the IPC while the pipe was still
  // needed by the cleaner process (scanner still running or waiting for a
  // response from Chrome).
  RESULT_CODE_CHROME_PROMPT_IPC_DISCONNECTED_TOO_SOON = 44,

  // DEPRECATED. The target process for the signature matcher disconnected from
  // the IPC while the pipe was still needed by the broker process.
  // 45

  // The user declined the elevation prompt or didn't have admin rights to
  // accept elevation.
  RESULT_CODE_ELEVATION_PROMPT_DECLINED = 46,

  // The target process for the ESET sandbox disconnected from the IPC
  // while the pipe was still needed by the broker process.
  RESULT_CODE_ESET_SANDBOX_DISCONNECTED_TOO_SOON = 47,

  // The user denied the cleanup prompt.
  RESULT_CODE_CLEANUP_PROMPT_DENIED = 48,

  // The cleaner process in scanning mode timed out while waiting for the user
  // the respond to the Chrome prompt.
  RESULT_CODE_WATCHDOG_TIMEOUT_WAITING_FOR_PROMPT_RESPONSE = 49,

  // The cleaner process in cleanup mode took too long to complete. Time out is
  // controlled by the cleaner process in scanning mode.
  RESULT_CODE_WATCHDOG_TIMEOUT_CLEANING = 50,

  // The cleaner process attempted to clean some UwS while using an engine that
  // only supports scanning.
  RESULT_CODE_CLEANUP_NOT_SUPPORTED_BY_ENGINE = 51,

  // The cleaner executable was run manually by a user without providing the
  // required execution mode flags. This used to launch the Chrome Cleaner's UI
  // which has been deprecated.
  RESULT_CODE_MANUAL_EXECUTION_BY_USER = 52,

  // Some of the scanning configuration switches have invalid values.
  RESULT_CODE_INVALID_SCANNING_SWITCHES = 53,

  // The target process for the JSON parser sandbox disconnected from the IPC
  // while the pipe was still needed by the broker process.
  RESULT_CODE_JSON_PARSER_SANDBOX_DISCONNECTED_TOO_SOON = 54,

  // The target process for the zip archiver sandbox disconnected from the IPC
  // while the pipe was still needed by the broker process.
  RESULT_CODE_ZIP_ARCHIVER_SANDBOX_DISCONNECTED_TOO_SOON = 55,

  // Used when attempting to run a 32-bit version of the tool on a 64-bit
  // machine, and vice versa. Since this can result in crashes and unexpected
  // result, we don't allow it.
  RESULT_CODE_WRONG_ARCHITECTURE = 56,

  // WHEN YOU ADD NEW EXIT CODES, DON'T FORGET TO UPDATE THE MONITORING RULES.
  // See http://go/chrome-cleaner-exit-codes. (Google internal only - external
  // contributors please ask one of the OWNERS to do the update.)

  // Used mainly for testing.
  RESULT_CODE_INVALID = std::numeric_limits<ResultCode>::max(),
};

}  // namespace chrome_cleaner

#endif  // COMPONENTS_CHROME_CLEANER_PUBLIC_CONSTANTS_RESULT_CODES_H_
