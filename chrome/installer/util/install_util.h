// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for the installer. The original reason
// for putting these functions in installer\util library is so that we can
// separate out the critical logic and write unit tests for it.

#ifndef CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
#define CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/types/strong_alias.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/util/util_constants.h"

class WorkItemList;

// This is a utility class that provides common installation related
// utility methods that can be used by installer and also unit tested
// independently.
class InstallUtil {
 public:
  InstallUtil(const InstallUtil&) = delete;
  InstallUtil& operator=(const InstallUtil&) = delete;

  // Attempts to trigger the command that would be run by Active Setup for a
  // system-level Chrome. For use only when system-level Chrome is installed.
  static void TriggerActiveSetupCommand();

  // Launches given exe as admin on Vista.
  static bool ExecuteExeAsAdmin(const base::CommandLine& cmd, DWORD* exit_code);

  // Reads the uninstall command for Chromium from the Windows registry and
  // returns it. If |system_install| is true the command is read from HKLM,
  // otherwise from HKCU. Returns an empty CommandLine if Chrome is not
  // installed.
  static base::CommandLine GetChromeUninstallCmd(bool system_install);

  // Returns the version of Chrome registered with Google Update, or an invalid
  // Version in case no such value could be found. |system_install| indicates
  // whether HKLM (true) or HKCU (false) should be checked.
  static base::Version GetChromeVersion(bool system_install);

  // Returns the last critical update (version) of Chrome, or an invalid Version
  // in case no such value is found. A critical update is a specially flagged
  // version (by Google Update) that contains an important security fix.
  static base::Version GetCriticalUpdateVersion();

  // This function checks if the current OS is supported for Chromium.
  static bool IsOSSupported();

  // Adds work items to |install_list| to set installer error information in the
  // registry for consumption by Google Update. |install_list| must be best-
  // effort with rollback disabled. |state_key| must be the full path to an
  // app's ClientState key.  See InstallerState::WriteInstallerResult for more
  // details.
  static void AddInstallerResultItems(bool system_install,
                                      const std::wstring& state_key,
                                      installer::InstallStatus status,
                                      int string_resource_id,
                                      const std::wstring* const launch_cmd,
                                      WorkItemList* install_list);

  // Returns true if this installation path is per user, otherwise returns false
  // (per machine install, meaning: the exe_path contains the path to Program
  // Files).
  // TODO(grt): consider replacing all callers with direct use of
  // InstallDetails.
  static bool IsPerUserInstall();

  // Returns true if the sentinel file exists (or the path cannot be obtained).
  static bool IsFirstRunSentinelPresent();

  // Test to see if a Start menu shortcut exists with the right toast activator
  // CLSID registered.
  static bool IsStartMenuShortcutWithActivatorGuidInstalled();

  // Returns true if the current process has the interactive user token. False
  // otherwise.
  static bool IsRunningAsInteractiveUser();

  // Returns the toast activator registry path.
  static std::wstring GetToastActivatorRegistryPath();

  // Populates |path| with EULA sentinel file path. Returns false on error.
  static bool GetEulaSentinelFilePath(base::FilePath* path);

  // Returns zero on install success, or an InstallStatus value otherwise.
  static int GetInstallReturnCode(installer::InstallStatus install_status);

  // Composes |program| and |arguments| into |command_line|.
  static void ComposeCommandLine(const std::wstring& program,
                                 const std::wstring& arguments,
                                 base::CommandLine* command_line);

  // Appends the installer switch that selects the current install mode and
  // policy-specified channel (see install_static::InstallDetails).
  static void AppendModeAndChannelSwitches(base::CommandLine* command_line);

  // Returns a string in the form YYYYMMDD of the current date.
  static std::wstring GetCurrentDate();

  // Returns the highest Chrome version that was installed prior to a downgrade,
  // or no value if Chrome was not previously downgraded from a newer version.
  static std::optional<base::Version> GetDowngradeVersion();

  // Returns pairs of registry key paths and value names where the enrollment
  // token is stored for machine level user cloud policies. The locations are
  // returned in order of preference.
  static std::vector<std::pair<std::wstring, std::wstring>>
  GetCloudManagementEnrollmentTokenRegistryPaths();

  using ReadOnly = base::StrongAlias<class ReadOnlyTag, bool>;
  using BrowserLocation = base::StrongAlias<class BrowserLocationTag, bool>;

  // Returns the path where the cloud management DMToken should be read/written.
  // |browser_location| indicates whether the legacy browser-specific path is
  // returned rather than the app-neutral path.
  static std::pair<std::wstring, std::wstring> GetCloudManagementDmTokenPath(
      BrowserLocation browser_location);

  // Returns the registry key and value name from/to which a cloud management DM
  // token may be read/written. |read_only| indicates whether they key is opened
  // for reading the value or writing it. |browser_location| indicates whether
  // the legacy browser-specific location is returned rather than the
  // app-neutral location. The returned key will be invalid if it could not be
  // opened/created.
  static std::pair<base::win::RegKey, std::wstring>
  GetCloudManagementDmTokenLocation(ReadOnly read_only,
                                    BrowserLocation browser_location);

  // Returns the registry key and value names from/to which the device trust
  // signing key and trust level may be read/written. |read_only| indicates
  // whether they key is opened for reading the value or writing it. The
  // returned key will be invalid if it could not be opened/created.
  static std::tuple<base::win::RegKey, std::wstring, std::wstring>
  GetDeviceTrustSigningKeyLocation(ReadOnly read_only);

  // Returns the token used to enroll this chrome instance for machine level
  // user cloud policies.  Returns an empty string if this machine should not
  // be enrolled.
  static std::wstring GetCloudManagementEnrollmentToken();

  // Returns true if cloud management enrollment is mandatory.
  static bool ShouldCloudManagementBlockOnFailure();

  // Returns the localized name of the browser.
  static std::wstring GetDisplayName();

  // Returns the app description for shortcuts.
  static std::wstring GetAppDescription();

  // Returns the name of the browser's publisher.
  static std::wstring GetPublisherName();

  // Returns the name of Chrome's shortcut in the Start Menu (among other
  // places).
  static std::wstring GetShortcutName();

  // Returns the name of the subdirectory in which Chrome's Start Menu shortcut
  // was once placed. This remains purely to migrate old installs to the new
  // style.
  static std::wstring GetChromeShortcutDirNameDeprecated();

  // Returns the name of the subdirectory in the Start Menu in which Chrome
  // apps' shortcuts are placed.
  static std::wstring GetChromeAppsShortcutDirName();

  // Returns the long description of Chrome used when registering as a browser
  // with Windows.
  static std::wstring GetLongAppDescription();

  // Converts a product GUID into a SQuished gUID that is used for MSI installer
  // registry entries.
  static std::wstring GuidToSquid(std::wstring_view guid);
};

#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
