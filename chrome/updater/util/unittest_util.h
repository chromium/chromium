// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_UNITTEST_UTIL_H_
#define CHROME_UPDATER_UTIL_UNITTEST_UTIL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
class FilePath;
}  // namespace base

namespace updater {
class PolicyService;
enum class UpdaterScope;
}  // namespace updater

namespace updater::test {

extern const char kChromeAppId[];

// Returns true if a process based on the named executable is running.
bool IsProcessRunning(const base::FilePath::StringType& executable_name);

// Returns true is all processes based on the named executable have exited.
// Otherwise, it returns false if the time delta has expired.
bool WaitForProcessesToExit(const base::FilePath::StringType& executable_name,
                            base::TimeDelta wait);

// Terminates all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.
// Returns true if all processes were able to be killed off.
bool KillProcesses(const base::FilePath::StringType& executable_name,
                   int exit_code);

// A policy service with default values.
scoped_refptr<PolicyService> CreateTestPolicyService();

// Returns the current test name in the format "TestSuiteName.TestName" or "?.?"
// if the test name is not available.
std::string GetTestName();

// Get the path for external constants override file. This is the JSON file in
// the updater data directory.
absl::optional<base::FilePath> GetOverrideFilePath(UpdaterScope scope);

// Deletes the file and its parent directories, if the parent directories are
// empty. Returns true if:
// - the file and the directories are deleted.
// - the file does not exist.
// - the directory is not empty.
bool DeleteFileAndEmptyParentDirectories(
    const absl::optional<base::FilePath>& file_path);

// Fetches the path to the ${ISOLATED_OUTDIR} env var.
// ResultDB reads logs and test artifacts info from there.
base::FilePath GetLogDestinationDir();

// Initializes the logging for the unit test and redirects the log output to
// ${ISOLATED_OUTDIR} if the directory is available. The unit tests can't log
// into the updater directory because that directory is touched by the
// integration tests. This function must be called after the `base::TestSuite`
// instance is created, because `base::TestSuite` initializes logging too.
void InitLoggingForUnitTest();

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

}  // namespace updater::test

#endif  // CHROME_UPDATER_UTIL_UNITTEST_UTIL_H_
