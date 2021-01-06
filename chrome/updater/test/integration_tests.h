// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_

#include <string>

#include "build/build_config.h"

namespace base {
class CommandLine;
class FilePath;
class Version;
}  // namespace base

class GURL;

namespace updater {
namespace test {

// Prints the updater.log file to stdout.
void PrintLog();

// Removes traces of the updater from the system. It is best to run this at the
// start of each test in case a previous crash or timeout on the machine running
// the test left the updater in an installed or partially installed state.
void Clean();

// Expects that the system is in a clean state, i.e. no updater is installed and
// no traces of an updater exist. Should be run at the start and end of each
// test.
void ExpectClean();

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
base::FilePath GetDataDirPath();

// Expects that the updater is installed on the system.
void ExpectInstalled();

// Installs the updater.
void Install();

// Expects that the updater is installed on the system and the launchd tasks
// are updated correctly.
void ExpectActive();

// Uninstalls the updater. If the updater was installed during the test, it
// should be uninstalled before the end of the test to avoid having an actual
// live updater on the machine that ran the test.
void Uninstall();

// Runs the wake client and wait for it to exit. Assert that it exits with
// `exit_code`. The server should exit a few seconds after.
void RunWake(int exit_code);

// Registers the test app. As a result, the bundled updater is installed,
// promoted and registered.
void RegisterTestApp();

// Runs the command and waits for it to exit or time out.
bool Run(base::CommandLine command_line, int* exit_code);

// Returns the path of the Updater executable.
base::FilePath GetInstalledExecutablePath();

// Returns the folder path under which the executable for the fake updater
// should reside.
base::FilePath GetFakeUpdaterInstallFolderPath(const base::Version& version);

// Creates Prefs with the fake updater version set as active.
void SetupFakeUpdaterPrefs(const base::Version& version);

// Creates an install folder on the system with the fake updater version.
void SetupFakeUpdaterInstallFolder(const base::Version& version);

// Sets up a fake updater on the system at a version lower than the test.
void SetupFakeUpdaterLowerVersion();

// Sets up a fake updater on the system at a version higher than the test.
void SetupFakeUpdaterHigherVersion();

// Expects that this version of updater is uninstalled from the system.
void ExpectCandidateUninstalled();

// Sets the active bit for `app_id`.
void SetActive(const std::string& app_id);

// Expects that the active bit for `app_id` is set.
void ExpectActive(const std::string& app_id);

// Expects that the active bit for `app_id` is unset.
void ExpectNotActive(const std::string& app_id);

#if defined(OS_WIN)
void ExpectInterfacesRegistered();
#endif

}  // namespace test
}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_
