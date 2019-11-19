// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_
#define COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_

#include <time.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_mach_port.h"
#endif

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_LINUX)
#include <signal.h>
#endif

namespace base {
class Time;
}

namespace crashpad {
class CrashpadClient;
class CrashReportDatabase;
}  // namespace crashpad

namespace crash_reporter {

#if defined(OS_LINUX)
// TODO(jperaza): Remove kEnableCrashpad and IsCrashpadEnabled() when Crashpad
// is fully enabled on Linux.
extern const char kEnableCrashpad[];
bool IsCrashpadEnabled();
#endif

// Initializes Crashpad in a way that is appropriate for initial_client and
// process_type.
//
// If initial_client is true, this starts crashpad_handler and sets it as the
// exception handler. Child processes will inherit this exception handler, and
// should specify false for this parameter. Although they inherit the exception
// handler, child processes still need to call this function to perform
// additional initialization.
//
// If process_type is empty, initialization will be done for the browser
// process. The browser process performs additional initialization of the crash
// report database. The browser process is also the only process type that is
// eligible to have its crashes forwarded to the system crash report handler (in
// release mode only). Note that when process_type is empty, initial_client must
// be true.
//
// On Mac, process_type may be non-empty with initial_client set to true. This
// indicates that an exception handler has been inherited but should be
// discarded in favor of a new Crashpad handler. This configuration should be
// used infrequently. It is provided to allow an install-from-.dmg relauncher
// process to disassociate from an old Crashpad handler so that after performing
// an installation from a disk image, the relauncher process may unmount the
// disk image that contains its inherited crashpad_handler. This is only
// supported when initial_client is true and process_type is "relauncher".
//
// On Windows, use InitializeCrashpadWithEmbeddedHandler() when crashpad_handler
// is embedded into a binary that can be launched with --type=crashpad-handler.
// Otherwise, this function should be used and will launch an external
// crashpad_handler.exe which is generally used for test situations.
void InitializeCrashpad(bool initial_client, const std::string& process_type);

#if defined(OS_WIN)
// This is the same as InitializeCrashpad(), but rather than launching a
// crashpad_handler executable, relaunches the executable at |exe_path| or the
// current executable if |exe_path| is empty with a command line argument of
// --type=crashpad-handler. If |user_data_dir| is non-empty, it is added to the
// handler's command line for use by Chrome Crashpad extensions.
void InitializeCrashpadWithEmbeddedHandler(bool initial_client,
                                           const std::string& process_type,
                                           const std::string& user_data_dir,
                                           const base::FilePath& exe_path);

// This version of InitializeCrashpadWithEmbeddedHandler is used to call an
// embedded crash handler that comes from an entry point in a DLL. The command
// line for these kind of embedded handlers is usually:
// C:\Windows\System32\rundll.exe <path to dll>,<entrypoint> ...
// In this situation the exe_path is not sufficient to allow spawning a crash
// handler through the DLL so |initial_arguments| needs to be passed to
// specify the DLL entry point.
void InitializeCrashpadWithDllEmbeddedHandler(
    bool initial_client,
    const std::string& process_type,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments);
#endif  // OS_WIN

// Returns the CrashpadClient for this process. This will lazily create it if
// it does not already exist. This is called as part of InitializeCrashpad.
crashpad::CrashpadClient& GetCrashpadClient();

// Enables or disables crash report upload, taking the given consent to upload
// into account. Consent may be ignored, uploads may not be enabled even with
// consent, but will only be enabled without consent when policy enforces crash
// reporting. Whether reports upload is a property of the Crashpad database. In
// a newly-created database, uploads will be disabled. This function only has an
// effect when called in the browser process. Its effect is immediate and
// applies to all other process types, including processes that are already
// running.
void SetUploadConsent(bool consent);

// Determines whether uploads are enabled or disabled. This information is only
// available in the browser process.
bool GetUploadsEnabled();

enum class ReportUploadState {
  NotUploaded,
  Pending,
  Pending_UserRequested,
  Uploaded
};

struct Report {
  char local_id[64];
  time_t capture_time;
  char remote_id[64];
  time_t upload_time;
  ReportUploadState state;
};

// Obtains a list of reports uploaded to the collection server. This function
// only operates when called in the browser process. All reports in the Crashpad
// database that have been successfully uploaded will be included in this list.
// The list will be sorted in descending order by report creation time (newest
// reports first).
void GetReports(std::vector<Report>* reports);

// Requests a user triggered upload for a crash report with a given id.
void RequestSingleCrashUpload(const std::string& local_id);

void DumpWithoutCrashing();

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Logs message and immediately crashes the current process without triggering a
// crash dump.
void CrashWithoutDumping(const std::string& message);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// Returns the Crashpad database path, only valid in the browser.
base::FilePath GetCrashpadDatabasePath();

// Deletes any reports that were recorded or uploaded within the time range.
void ClearReportsBetween(const base::Time& begin, const base::Time& end);

// The implementation function for GetReports.
void GetReportsImpl(std::vector<Report>* reports);

// The implementation function for RequestSingleCrashUpload.
void RequestSingleCrashUploadImpl(const std::string& local_id);

// The implementation function for GetCrashpadDatabasePath.
base::FilePath::StringType::const_pointer GetCrashpadDatabasePathImpl();

// The implementation function for ClearReportsBetween.
void ClearReportsBetweenImpl(time_t begin, time_t end);

#if defined(OS_MACOSX)
// Captures a minidump for the process named by its |task_port| and stores it
// in the current crash report database.
void DumpProcessWithoutCrashing(task_t task_port);
#endif

#if defined(OS_ANDROID)
// This is used by WebView to generate a dump on behalf of the embedding app.
// This function can only be called from the browser process. Returns `true` on
// success.
class CrashReporterClient;
bool DumpWithoutCrashingForClient(CrashReporterClient* client);

// Used under WebView to whitelist a memory range so it's accessible using
// crashpad's ProcessMemory interface.
void WhitelistMemoryRange(void* begin, size_t size);
#endif  // OS_ANDROID

#if defined(OS_LINUX)
// Install a handler that gets a chance to handle faults before Crashpad. This
// is used by V8 for trap-based bounds checks.
void SetFirstChanceExceptionHandler(bool (*handler)(int, siginfo_t*, void*));

// Gets the socket and process ID of the Crashpad handler connected to this
// process, valid if this function returns `true`.
bool GetHandlerSocket(int* sock, pid_t* pid);
#endif  // OS_LINUX

namespace internal {

#if defined(OS_WIN)
// Returns platform specific annotations. This is broken out on Windows only so
// that it may be reused by GetCrashKeysForKasko.
void GetPlatformCrashpadAnnotations(
    std::map<std::string, std::string>* annotations);

// The thread functions that implement the InjectDumpForHungInput in the
// target process.
DWORD WINAPI DumpProcessForHungInputThread(void* param);

#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_ANDROID)
// Starts the handler process with an initial client connected on fd.
// Returns `true` on success.
bool StartHandlerForClient(int fd);
#endif  // OS_LINUX || OS_ANDROID

// The platform-specific portion of InitializeCrashpad(). On Windows, if
// |user_data_dir| is non-empty, the user data directory will be passed to the
// handler process for use by Chrome Crashpad extensions; if |exe_path| is
// non-empty, it specifies the path to the executable holding the embedded
// handler. Returns the database path, if initializing in the browser process.
base::FilePath PlatformCrashpadInitialization(
    bool initial_client,
    bool browser_process,
    bool embedded_handler,
    const std::string& user_data_dir,
    const base::FilePath& exe_path,
    const std::vector<std::string>& initial_arguments);

// Returns the current crash report database object, or null if it has not
// been initialized yet.
crashpad::CrashReportDatabase* GetCrashReportDatabase();

}  // namespace internal

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CONTENT_APP_CRASHPAD_H_
