// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project. It also declares a
// few functions that the Chrome component updater uses for patching binary
// deltas.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/win/windows_types.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/util_constants.h"

class WorkItemList;

namespace base {
class CommandLine;
class FilePath;
class Version;
}  // namespace base

namespace installer {

class InstallationState;
class InstallerState;
class InitialPreferences;

extern const char kUnPackStatusMetricsName[];

// The name of consumers of UnPackArchive which is used to publish metrics.
enum UnPackConsumer {
  CHROME_ARCHIVE_PATCH,
  COMPRESSED_CHROME_ARCHIVE,
  SETUP_EXE_PATCH,
  UNCOMPRESSED_CHROME_ARCHIVE,
};

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or nullptr if no version is found.
base::Version* GetMaxVersionFromArchiveDir(const base::FilePath& chrome_path);

// Returns the uncompressed archive of the installed version that serves as the
// source for patching.  If |desired_version| is valid, only the path to that
// version will be returned, or empty if it doesn't exist.
base::FilePath FindArchiveToPatch(const InstallationState& original_state,
                                  const InstallerState& installer_state,
                                  const base::Version& desired_version);

// Spawns a new process that waits for a specified amount of time before
// attempting to delete |path|.  This is useful for setup to delete the
// currently running executable or a file that we cannot close right away but
// estimate that it will be possible after some period of time.
// Returns true if a new process was started, false otherwise.  Note that
// given the nature of this function, it is not possible to know if the
// delete operation itself succeeded.
bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32_t delay_before_delete_ms);

// Drops the thread down to background processing mode on supported OSes if it
// was launched below the normal process priority. Returns true when background
// processing mode is entered.
bool AdjustThreadPriority();

// Returns true if |install_status| represents a successful uninstall code.
bool IsUninstallSuccess(InstallStatus install_status);

// Returns true if |cmd_line| contains unsupported (legacy) switches.
bool ContainsUnsupportedSwitch(const base::CommandLine& cmd_line);

// Returns true if the processor is supported by chrome.
bool IsProcessorSupported();

// Deletes all values and subkeys of the key |path| under |root|, preserving
// the keys named in |keys_to_preserve| (each of which must be an ASCII string).
// The key itself is deleted if no subkeys are preserved.
void DeleteRegistryKeyPartial(
    HKEY root,
    const std::wstring& path,
    const std::vector<std::wstring>& keys_to_preserve);

// Returns true if downgrade is allowed by installer data.
bool IsDowngradeAllowed(const InitialPreferences& prefs);

// Returns the age (in days) of the installation based on the creation time of
// its installation directory, or -1 in case of error.
int GetInstallAge(const InstallerState& installer_state);

// Records UMA metrics for unpack result.
void RecordUnPackMetrics(UnPackStatus unpack_status, UnPackConsumer consumer);

// Register Chrome's EventLog message provider dll.
void RegisterEventLogProvider(const base::FilePath& install_directory,
                              const base::Version& version);

// De-register Chrome's EventLog message provider dll.
void DeRegisterEventLogProvider();

// Removes leftover bits from features that have been removed from the product.
void DoLegacyCleanups(const InstallerState& installer_state,
                      InstallStatus install_status);

// Returns the time of the start of the console user's Windows logon session, or
// a null time in case of error.
base::Time GetConsoleSessionStartTime();

// Returns a DM token decoded from the base-64 `encoded_token`, or null in case
// of a decoding error.  The returned DM token is an opaque binary blob and
// should not be treated as an ASCII or UTF-8 string.
std::optional<std::string> DecodeDMTokenSwitchValue(
    const std::wstring& encoded_token);

// Returns a nonce decoded from the base-64 `encoded_nonce`, or null in case
// of a decoding error.  The returned nonce is an opaque binary blob and
// should not be treated as an ASCII or UTF-8 string.
std::optional<std::string> DecodeNonceSwitchValue(
    const std::string& encoded_nonce);

// Saves a DM token to a global location on the machine accessible to all
// install modes of the browser (i.e., stable and all three side-by-side modes).
bool StoreDMToken(const std::string& token);

// Deletes any existing DMToken from the global location on the machine.
bool DeleteDMToken();

// Returns the file path to notification_helper.exe (in |version| directory).
base::FilePath GetNotificationHelperPath(const base::FilePath& target_path,
                                         const base::Version& version);

// Returns the file path to chrome_wer.dll (in `version` directory).
base::FilePath GetWerHelperPath(const base::FilePath& target_path,
                                const base::Version& version);

// Returns the WER runtime exception helper module registry path.
std::wstring GetWerHelperRegistryPath();

// Returns the file path to elevation_service.exe (in |version| directory).
base::FilePath GetElevationServicePath(const base::FilePath& target_path,
                                       const base::Version& version);

// Returns the file path to elevated_tracing_service.exe (in `version`
// directory).
base::FilePath GetTracingServicePath(const base::FilePath& target_path,
                                     const base::Version& version);

// Adds or removes downgrade version registry value.
void AddUpdateDowngradeVersionItem(HKEY root,
                                   const base::Version& current_version,
                                   const base::Version& new_version,
                                   WorkItemList* list);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
