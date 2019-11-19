// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/installer/util/lzma_util.h"
#include "chrome/installer/util/util_constants.h"

namespace base {
class CommandLine;
class FilePath;
class Version;
}

namespace installer {

class InstallationState;
class InstallerState;
class MasterPreferences;

extern const char kUnPackNTSTATUSMetricsName[];
extern const char kUnPackResultMetricsName[];
extern const char kUnPackStatusMetricsName[];

// The name of consumers of UnPackArchive which is used to publish metrics.
enum UnPackConsumer {
  CHROME_ARCHIVE_PATCH,
  COMPRESSED_CHROME_ARCHIVE,
  SETUP_EXE_PATCH,
  UNCOMPRESSED_CHROME_ARCHIVE,
};

// Applies a patch file to source file using Courgette. Returns 0 in case of
// success. In case of errors, it returns kCourgetteErrorOffset + a Courgette
// status code, as defined in courgette/courgette.h
int CourgettePatchFiles(const base::FilePath& src,
                        const base::FilePath& patch,
                        const base::FilePath& dest);

// Applies a patch file to source file using bsdiff. This function uses
// Courgette's flavor of bsdiff. Returns 0 in case of success, or
// kBsdiffErrorOffset + a bsdiff status code in case of errors.
// See courgette/third_party/bsdiff/bsdiff.h for details.
int BsdiffPatchFiles(const base::FilePath& src,
                     const base::FilePath& patch,
                     const base::FilePath& dest);

// Applies a patch file to source file using Zucchini. Returns 0 in case of
// success. In case of errors, it returns kZucchiniErrorOffset + a Zucchini
// status code, as defined in components/zucchini/zucchini.h
int ZucchiniPatchFiles(const base::FilePath& src,
                       const base::FilePath& patch,
                       const base::FilePath& dest);

// Find the version of Chrome from an install source directory.
// Chrome_path should contain at least one version folder.
// Returns the maximum version found or NULL if no version is found.
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

// Drops the process down to background processing mode on supported OSes if it
// was launched below the normal process priority. Returns true when background
// procesing mode is entered.
bool AdjustProcessPriority();

// Returns true if |install_status| represents a successful uninstall code.
bool IsUninstallSuccess(InstallStatus install_status);

// Returns true if |cmd_line| contains unsupported (legacy) switches.
bool ContainsUnsupportedSwitch(const base::CommandLine& cmd_line);

// Returns true if the processor is supported by chrome.
bool IsProcessorSupported();

// Returns the "...\\Commands\\|name|" registry key for a product's |reg_data|.
base::string16 GetCommandKey(const wchar_t* name);

// Deletes all values and subkeys of the key |path| under |root|, preserving
// the keys named in |keys_to_preserve| (each of which must be an ASCII string).
// The key itself is deleted if no subkeys are preserved.
void DeleteRegistryKeyPartial(
    HKEY root,
    const base::string16& path,
    const std::vector<base::string16>& keys_to_preserve);

// Returns true if downgrade is allowed by installer data.
bool IsDowngradeAllowed(const MasterPreferences& prefs);

// Returns the age (in days) of the installation based on the creation time of
// its installation directory, or -1 in case of error.
int GetInstallAge(const InstallerState& installer_state);

// Records UMA metrics for unpack result.
void RecordUnPackMetrics(UnPackStatus unpack_status,
                         base::Optional<int32_t> ntstatus,
                         base::Optional<DWORD> error_code,
                         UnPackConsumer consumer);

// Register Chrome's EventLog message provider dll.
void RegisterEventLogProvider(const base::FilePath& install_directory,
                              const base::Version& version);

// De-register Chrome's EventLog message provider dll.
void DeRegisterEventLogProvider();

// Returns true if the now-deprecated multi-install binaries are registered as
// an installed product with Google Update.
bool AreBinariesInstalled(const InstallerState& installer_state);

// Removes leftover bits from features that have been removed from the product.
void DoLegacyCleanups(const InstallerState& installer_state,
                      InstallStatus install_status);

// Returns the time of the start of the console user's Windows logon session, or
// a null time in case of error.
base::Time GetConsoleSessionStartTime();

// Returns a DM token decoded from the base-64 |encoded_token|, or null in case
// of a decoding error.  The returned DM token is an opaque binary blob and
// should not be treated as an ASCII or UTF-8 string.
base::Optional<std::string> DecodeDMTokenSwitchValue(
    const base::string16& encoded_token);

// Saves a DM token to a global location on the machine accessible to all
// install modes of the browser (i.e., stable and all three side-by-side modes).
bool StoreDMToken(const std::string& token);

// Returns the file path to notification_helper.exe (in |version| directory).
base::FilePath GetNotificationHelperPath(const base::FilePath& target_path,
                                         const base::Version& version);

// Returns the file path to elevation_service.exe (in |version| directory).
base::FilePath GetElevationServicePath(const base::FilePath& target_path,
                                       const base::Version& version);

// Returns the Elevation Service GUID prefixed with |prefix|.
base::string16 GetElevationServiceGuid(base::StringPiece16 prefix);

// Return the elevation service registry paths.
base::string16 GetElevationServiceClsidRegistryPath();
base::string16 GetElevationServiceAppidRegistryPath();
base::string16 GetElevationServiceIid(base::StringPiece16 prefix);
base::string16 GetElevationServiceIidRegistryPath();
base::string16 GetElevationServiceTypeLibRegistryPath();

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
