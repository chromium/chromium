// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_UNIT_TEST_UTIL_H_
#define CHROME_UPDATER_TEST_UNIT_TEST_UTIL_H_

#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/functional/function_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process_iterator.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/updater/tag.h"

namespace base {
class TimeDelta;
class Version;
}  // namespace base

namespace updater {
class PolicyService;
enum class UpdaterScope;
}  // namespace updater

namespace updater::test {

extern const char kChromeAppId[];

// Returns true if a process based on the named executable and optional filter
// is running.
bool IsProcessRunning(const base::FilePath::StringType& executable_name,
                      const base::ProcessFilter* filter = nullptr);

// Returns true if all processes based on the named executable and optional
// filter have exited. Otherwise, returns false if the time delta has expired.
bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait,
                            const base::ProcessFilter* filter = nullptr);

// Terminates all the processes on the current machine that were launched
// from the given executable name and optional filter, ending them with the
// given exit code. Returns true if all processes were able to be killed off.
bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code,
                   const base::ProcessFilter* filter = nullptr);

// A policy service with default values.
scoped_refptr<PolicyService> CreateTestPolicyService();

// Returns the current test name in the format "TestSuiteName.TestName" or "?.?"
// if the test name is not available.
std::string GetTestName();

// Deletes the file and its parent directories, if the parent directories are
// empty. Returns true if:
// - the file and the directories are deleted.
// - the file does not exist.
// - the directory is not empty.
bool DeleteFileAndEmptyParentDirectories(
    const std::optional<base::FilePath>& file_path);

// Fetches the path to the ${ISOLATED_OUTDIR} env var.
// ResultDB reads logs and test artifacts info from there.
base::FilePath GetLogDestinationDir();

// Initializes the logging for the unit test and redirects the log output to
// ${ISOLATED_OUTDIR} if the directory is available. `log_base_path` is the base
// name of the log file in the above directory. The unit tests can't log into
// the updater directory because that directory is touched by the integration
// tests. This function must be called after the `base::TestSuite` instance is
// created, because `base::TestSuite` initializes logging too.
void InitLoggingForUnitTest(const base::FilePath& log_base_path);

#if BUILDFLAG(IS_WIN)
// Change Windows Defender settings to skip scanning the paths used by the
// updater if test runs with the flag `exclude-paths-from-win-defender`.
void MaybeExcludePathsFromWindowsDefender();

// Starts procmon logging if admin and procmon exists at
// `C:\\tools\\Procmon.exe`. Returns the path to the PML file if procmon could
// be successfully started.
base::FilePath StartProcmonLogging();

// Stops procmon logging and exports the PML file to a CSV file at the same
// location as `pml_file`. Caller needs to be admin, procmon needs to exist at
// `C:\\tools\\Procmon.exe`, and `pml_file` needs to be a valid path to a
// procmon PML file returned from `StartProcmonLogging`.
void StopProcmonLogging(const base::FilePath& pml_file);
#endif

// Returns a list of processes matching `executable_name` and optional `filter`.
const base::ProcessIterator::ProcessEntries FindProcesses(
    const base::FilePath::StringType& executable_name,
    const base::ProcessFilter* filter = nullptr);

// Returns a formatted string of processes matching `executable_name` and
// optional `filter`.
std::string PrintProcesses(const base::FilePath::StringType& executable_name,
                           const base::ProcessFilter* filter = nullptr);

// Waits for a given `predicate` to become true. Invokes `still_waiting`
// periodically to provide a indication of progress. Returns true if the
// predicate becomes true before a timeout, otherwise returns false.
[[nodiscard]] bool WaitFor(
    base::FunctionRef<bool()> predicate,
    base::FunctionRef<void()> still_waiting = [] {});

struct EventHolder {
  base::WaitableEvent event;
  std::wstring name;
};

// Creates a waitable event with default attributes for the current process,
// test, and test scope.
EventHolder CreateWaitableEventForTest();

// Returns the absolute path to a test file used by update client unit tests.
// These test files exist in the source tree and are available to tests in
// `//chrome/updater/test/data.` `file_name` is the relative name of the
// file in that directory.
[[nodiscard]] base::FilePath GetTestFilePath(const char* file_name);

// Sets up the official updater directory with global prefs, the versioned
// install folder (with a version of `base_version + major_version_offset`), and
// optionally, an empty updater executable in the versioned folder.
void SetupFakeUpdaterVersion(UpdaterScope scope,
                             const base::Version& base_version,
                             int major_version_offset,
                             bool should_create_updater_executable);

// Set up a mock `mock_updater_path`, and the following mock directories under
// `mock_updater_path.DirName()`: `Download`, `Install`, and a versioned
// `1.2.3.4` directory.
void SetupMockUpdater(const base::FilePath& mock_updater_path);

// Expect only a single file `mock_updater_path` and nothing else under
// `mock_updater_path.DirName()`.
void ExpectOnlyMockUpdater(const base::FilePath& mock_updater_path);

// Expects that all members of `actual` and `expected` are the same.
void ExpectTagArgsEqual(const updater::tagging::TagArgs& actual,
                        const updater::tagging::TagArgs& expected);

// Wait for the process to exit up to the action timeout and returns the exit
// code. The function expects the process to exit in time.
int WaitForProcess(base::Process& process);

}  // namespace updater::test

#endif  // CHROME_UPDATER_TEST_UNIT_TEST_UTIL_H_
