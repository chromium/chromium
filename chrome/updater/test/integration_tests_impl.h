// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"

namespace base {
class CommandLine;
class Version;
}  // namespace base

class GURL;

namespace updater {
namespace test {

// Prints the updater.log file to stdout.
void PrintLog(UpdaterScope scope);

// Removes traces of the updater from the system. It is best to run this at the
// start of each test in case a previous crash or timeout on the machine running
// the test left the updater in an installed or partially installed state.
void Clean(UpdaterScope scope);

// Expects that the system is in a clean state, i.e. no updater is installed and
// no traces of an updater exist. Should be run at the start and end of each
// test.
void ExpectClean(UpdaterScope scope);

// Places the updater into test mode (use `url` as the update server and disable
// CUP).
void EnterTestMode(const GURL& url);

// Copies the logs to a location where they can be retrieved by ResultDB.
void CopyLog(const base::FilePath& src_dir);

// Sleeps for the given number of seconds. This should be avoided, but in some
// cases surrounding uninstall it is necessary since the processes can exit
// prior to completing the actual uninstallation.
void SleepFor(int seconds);

// Returns the path to the updater data dir.
base::Optional<base::FilePath> GetDataDirPath(UpdaterScope scope);

// Expects that the updater is installed on the system.
void ExpectInstalled(UpdaterScope scope);

// Installs the updater.
void Install(UpdaterScope scope);

// Expects that the updater is installed on the system and the launchd tasks
// are updated correctly.
void ExpectActiveUpdater(UpdaterScope scope);

void ExpectVersionActive(const std::string& version);
void ExpectVersionNotActive(const std::string& version);

// Uninstalls the updater. If the updater was installed during the test, it
// should be uninstalled before the end of the test to avoid having an actual
// live updater on the machine that ran the test.
void Uninstall(UpdaterScope scope);

// Runs the wake client and wait for it to exit. Assert that it exits with
// `exit_code`. The server should exit a few seconds after.
void RunWake(UpdaterScope scope, int exit_code);

// Registers the test app. As a result, the bundled updater is installed,
// promoted and registered.
void RegisterTestApp(UpdaterScope scope);

// Runs the command and waits for it to exit or time out.
bool Run(UpdaterScope scope, base::CommandLine command_line, int* exit_code);

// Returns the path of the Updater executable.
base::Optional<base::FilePath> GetInstalledExecutablePath(UpdaterScope scope);

// Returns the folder path under which the executable for the fake updater
// should reside.
base::Optional<base::FilePath> GetFakeUpdaterInstallFolderPath(
    UpdaterScope scope,
    const base::Version& version);

// Creates Prefs with the fake updater version set as active.
void SetupFakeUpdaterPrefs(const base::Version& version);

// Creates an install folder on the system with the fake updater version.
void SetupFakeUpdaterInstallFolder(UpdaterScope scope,
                                   const base::Version& version);

// Sets up a fake updater on the system at a version lower than the test.
void SetupFakeUpdaterLowerVersion(UpdaterScope scope);

// Sets up a fake updater on the system at a version higher than the test.
void SetupFakeUpdaterHigherVersion(UpdaterScope scope);

// Expects that this version of updater is uninstalled from the system.
void ExpectCandidateUninstalled(UpdaterScope scope);

// Sets the active bit for `app_id`.
void SetActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is set.
void ExpectActive(UpdaterScope scope, const std::string& app_id);

// Expects that the active bit for `app_id` is unset.
void ExpectNotActive(UpdaterScope scope, const std::string& app_id);

void SetFakeExistenceCheckerPath(const std::string& app_id);
void ExpectAppUnregisteredExistenceCheckerPath(const std::string& app_id);

void RegisterApp(const std::string& app_id);

#if defined(OS_WIN)
void ExpectInterfacesRegistered();
#endif

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_IMPL_H_
