// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares utility functions for the installer. The original reason
// for putting these functions in installer\util library is so that we can
// separate out the critical logic and write unit tests for it.

#ifndef CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
#define CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_

#include <windows.h>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
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

  // Returns the toast activator registry path.
  static std::wstring GetToastActivatorRegistryPath();

  // Populates |path| with EULA sentinel file path. Returns false on error.
  static bool GetEulaSentinelFilePath(base::FilePath* path);

  // Deletes the registry key at path key_path under the key given by root_key.
  static bool DeleteRegistryKey(HKEY root_key,
                                const std::wstring& key_path,
                                REGSAM wow64_access);

  // Deletes the registry value named value_name at path key_path under the key
  // given by reg_root.
  static bool DeleteRegistryValue(HKEY reg_root,
                                  const std::wstring& key_path,
                                  REGSAM wow64_access,
                                  const std::wstring& value_name);

  // An interface to a predicate function for use by DeleteRegistryKeyIf and
  // DeleteRegistryValueIf.
  class RegistryValuePredicate {
   public:
    virtual ~RegistryValuePredicate() {}
    virtual bool Evaluate(const std::wstring& value) const = 0;
  };

  // The result of a conditional delete operation (i.e., DeleteFOOIf).
  enum ConditionalDeleteResult {
    NOT_FOUND,     // The condition was not satisfied.
    DELETED,       // The condition was satisfied and the delete succeeded.
    DELETE_FAILED  // The condition was satisfied but the delete failed.
  };

  // Deletes the key |key_to_delete_path| under |root_key| iff the value
  // |value_name| in the key |key_to_test_path| under |root_key| satisfies
  // |predicate|.  |value_name| may be either nullptr or an empty string to test
  // the key's default value.
  static ConditionalDeleteResult DeleteRegistryKeyIf(
      HKEY root_key,
      const std::wstring& key_to_delete_path,
      const std::wstring& key_to_test_path,
      REGSAM wow64_access,
      const wchar_t* value_name,
      const RegistryValuePredicate& predicate);

  // Deletes the value |value_name| in the key |key_path| under |root_key| iff
  // its current value satisfies |predicate|.  |value_name| may be either
  // nullptr or an empty string to test/delete the key's default value.
  static ConditionalDeleteResult DeleteRegistryValueIf(
      HKEY root_key,
      const wchar_t* key_path,
      REGSAM wow64_access,
      const wchar_t* value_name,
      const RegistryValuePredicate& predicate);

  // A predicate that performs a case-sensitive string comparison.
  class ValueEquals : public RegistryValuePredicate {
   public:
    explicit ValueEquals(const std::wstring& value_to_match)
        : value_to_match_(value_to_match) {}
    bool Evaluate(const std::wstring& value) const override;

   protected:
    std::wstring value_to_match_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ValueEquals);
  };

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
  static base::Optional<base::Version> GetDowngradeVersion();

  // Returns pairs of registry key paths and value names where the enrollment
  // token is stored for machine level user cloud policies. The locations are
  // returned in order of preference.
  static std::vector<std::pair<std::wstring, std::wstring>>
  GetCloudManagementEnrollmentTokenRegistryPaths();

  using ReadOnly = base::StrongAlias<class ReadOnlyTag, bool>;
  using BrowserLocation = base::StrongAlias<class BrowserLocationTag, bool>;

  // Returns the registry key and value name from/to which a cloud management DM
  // token may be read/written. |read_only| indicates whether they key is opened
  // for reading the value or writing it. |browser_location| indicates whether
  // the legacy browser-specific location is returned rather than the
  // app-neutral location. The returned key will be invalid if it could not be
  // opened/created.
  static std::pair<base::win::RegKey, std::wstring>
  GetCloudManagementDmTokenLocation(ReadOnly read_only,
                                    BrowserLocation browser_location);

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

  // A predicate that compares the program portion of a command line with a
  // given file path.  First, the file paths are compared directly.  If they do
  // not match, the filesystem is consulted to determine if the paths reference
  // the same file.
  class ProgramCompare : public RegistryValuePredicate {
   public:
    explicit ProgramCompare(const base::FilePath& path_to_match);
    ~ProgramCompare() override;
    bool Evaluate(const std::wstring& value) const override;
    bool EvaluatePath(const base::FilePath& path) const;

   protected:
    static bool OpenForInfo(const base::FilePath& path, base::File* file);
    static bool GetInfo(const base::File& file,
                        BY_HANDLE_FILE_INFORMATION* info);

    base::FilePath path_to_match_;
    base::File file_;
    BY_HANDLE_FILE_INFORMATION file_info_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProgramCompare);
  };  // class ProgramCompare

  // Converts a product GUID into a SQuished gUID that is used for MSI installer
  // registry entries.
  static std::wstring GuidToSquid(base::WStringPiece guid);

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallUtil);
};

#endif  // CHROME_INSTALLER_UTIL_INSTALL_UTIL_H_
