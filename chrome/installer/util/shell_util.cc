// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines functions that integrate Chrome in Windows shell. These
// functions can be used by Chrome as well as Chrome installer. All of the
// work is done by the local functions defined in anonymous namespace in
// this class.

#include "chrome/installer/util/shell_util.h"

#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <windows.h>
#include <wrl/client.h>

#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/hash/md5.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/atomic_flag.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_constants.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/beacons.h"
#include "chrome/installer/util/helper.h"
#include "chrome/installer/util/initial_preferences.h"
#include "chrome/installer/util/initial_preferences_constants.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/registry_entry.h"
#include "chrome/installer/util/scoped_user_protocol_entry.h"
#include "chrome/installer/util/util_constants.h"
#include "chrome/installer/util/work_item.h"
#include "components/base32/base32.h"

using base::win::RegKey;

namespace {

// An enum used to tell QuickIsChromeRegistered() which level of registration
// the caller wants to confirm.
enum RegistrationConfirmationLevel {
  // Only look for Chrome's ProgIds.
  // This is sufficient when we are trying to determine the suffix of the
  // currently running Chrome as shell integration registrations might not be
  // present.
  CONFIRM_PROGID_REGISTRATION = 0,
  // Confirm that Chrome is fully integrated with Windows (i.e. registered with
  // Default Programs). These registrations can be in HKCU as of Windows 8.
  // Note: Shell registration implies ProgId registration.
  CONFIRM_SHELL_REGISTRATION,
  // Same as CONFIRM_SHELL_REGISTRATION, but only look in HKLM (used when
  // uninstalling to know whether elevation is required to clean up the
  // registry).
  CONFIRM_SHELL_REGISTRATION_IN_HKLM,
};

const wchar_t kReinstallCommand[] = L"ReinstallCommand";

// Returns the current (or installed) browser's ProgId (e.g.
// "ChromeHTML|suffix|").
// |suffix| can be the empty string.
base::string16 GetBrowserProgId(const base::string16& suffix) {
  base::string16 chrome_html(install_static::GetProgIdPrefix());
  chrome_html.append(suffix);

  // ProgIds cannot be longer than 39 characters.
  // Ref: http://msdn.microsoft.com/en-us/library/aa911706.aspx.
  // Make all new registrations comply with this requirement (existing
  // registrations must be preserved).
  base::string16 new_style_suffix;
  if (ShellUtil::GetUserSpecificRegistrySuffix(&new_style_suffix) &&
      suffix == new_style_suffix && chrome_html.length() > 39) {
    NOTREACHED();
    chrome_html.erase(39);
  }
  return chrome_html;
}

// Returns the browser's application name. This application name will be
// suffixed as is appropriate for the current install. This is the name that is
// registered with Default Programs on Windows and that should thus be used to
// "make chrome default" and such.
base::string16 GetApplicationName(const base::FilePath& chrome_exe) {
  return install_static::GetBaseAppName().append(
      ShellUtil::GetCurrentInstallationSuffix(chrome_exe));
}

// This class is used to initialize and cache a base 32 encoding of the md5 hash
// of this user's sid preceded by a dot.
// This is guaranteed to be unique on the machine and 27 characters long
// (including the '.').
// This is then meant to be used as a suffix on all registrations that may
// conflict with another user-level Chrome install.
class UserSpecificRegistrySuffix {
 public:
  // All the initialization is done in the constructor to be able to build the
  // suffix in a thread-safe manner when used in conjunction with a
  // LazyInstance.
  UserSpecificRegistrySuffix();

  // Sets |suffix| to the pre-computed suffix cached in this object.
  // Returns true unless the initialization originally failed.
  bool GetSuffix(base::string16* suffix);

 private:
  base::string16 suffix_;

  DISALLOW_COPY_AND_ASSIGN(UserSpecificRegistrySuffix);
};  // class UserSpecificRegistrySuffix

UserSpecificRegistrySuffix::UserSpecificRegistrySuffix() {
  base::string16 user_sid;
  if (!base::win::GetUserSidString(&user_sid)) {
    NOTREACHED();
    return;
  }
  static_assert(sizeof(base::MD5Digest) == 16, "size of MD5 not as expected");
  base::MD5Digest md5_digest;
  std::string user_sid_ascii(base::UTF16ToASCII(user_sid));
  base::MD5Sum(user_sid_ascii.c_str(), user_sid_ascii.length(), &md5_digest);
  std::string base32_md5 = base32::Base32Encode(
      base::StringPiece(reinterpret_cast<char*>(md5_digest.a),
                        base::size(md5_digest.a)),
      base32::Base32EncodePolicy::OMIT_PADDING);
  // The value returned by the base32 algorithm above must never change.
  DCHECK_EQ(base32_md5.length(), 26U);
  suffix_.reserve(base32_md5.length() + 1);
  suffix_.assign(1, L'.');
  suffix_.append(base::ASCIIToUTF16(base32_md5));
}

bool UserSpecificRegistrySuffix::GetSuffix(base::string16* suffix) {
  if (suffix_.empty()) {
    NOTREACHED();
    return false;
  }
  suffix->assign(suffix_);
  return true;
}

// Details about a Windows application, to be entered into the registry for the
// purpose of file associations.
struct ApplicationInfo {
  ApplicationInfo() : file_type_icon_index(0), application_icon_index(0) {}

  // The ProgId used by Windows for file associations with this application.
  // Must not be empty or start with a '.'.
  base::string16 prog_id;
  // The friendly name, and the path of the icon that will be used for files of
  // these types when associated with this application by default. (They are NOT
  // the name/icon that will represent the application under the Open With
  // menu.)
  base::string16 file_type_name;
  base::FilePath file_type_icon_path;
  int file_type_icon_index;
  // The command to execute when opening a file via this association. It should
  // contain "%1" (to tell Windows to pass the filename as an argument).
  // TODO(mgiuca): |command_line| should be a base::CommandLine.
  base::string16 command_line;
  // The AppUserModelId used by Windows 8 for this application. Distinct from
  // |prog_id|.
  base::string16 app_id;

  // User-visible details about this application. Any of these may be empty.
  base::string16 application_name;
  base::FilePath application_icon_path;
  int application_icon_index;
  base::string16 application_description;
  base::string16 publisher_name;

  // The CLSID for the application's DelegateExecute handler. May be empty.
  base::string16 delegate_clsid;
};

// Returns the Windows browser client registration key for Chrome.  For example:
// "Software\Clients\StartMenuInternet\Chromium[.user]".  Strictly speaking, we
// should use the name of the executable (e.g., "chrome.exe"), but that ship has
// sailed.  The cost of switching now is re-prompting users to make Chrome their
// default browser, which isn't polite.  |suffix| is the user-specific
// registration suffix; see GetUserSpecificDefaultBrowserSuffix in shell_util.h
// for details.
base::string16 GetBrowserClientKey(const base::string16& suffix) {
  DCHECK(suffix.empty() || suffix[0] == L'.');
  return base::string16(ShellUtil::kRegStartMenuInternet)
      .append(1, L'\\')
      .append(install_static::GetBaseAppName())
      .append(suffix);
}

// Returns the Windows Default Programs capabilities key for Chrome.  For
// example:
// "Software\Clients\StartMenuInternet\Chromium[.user]\Capabilities".
base::string16 GetCapabilitiesKey(const base::string16& suffix) {
  return GetBrowserClientKey(suffix).append(L"\\Capabilities");
}

// DelegateExecute ProgId. Needed for Chrome Metro in Windows 8. This is only
// needed for registering a web browser, not for general associations.
std::vector<std::unique_ptr<RegistryEntry>> GetChromeDelegateExecuteEntries(
    const base::FilePath& chrome_exe,
    const ApplicationInfo& app_info) {
  std::vector<std::unique_ptr<RegistryEntry>> entries;

  base::string16 app_id_shell_key(ShellUtil::kRegClasses);
  app_id_shell_key.push_back(base::FilePath::kSeparators[0]);
  app_id_shell_key.append(app_info.app_id);
  app_id_shell_key.append(ShellUtil::kRegExePath);
  app_id_shell_key.append(ShellUtil::kRegShellPath);

  // <root hkey>\Software\Classes\<app_id>\.exe\shell @=open
  entries.push_back(std::make_unique<RegistryEntry>(app_id_shell_key,
                                                    ShellUtil::kRegVerbOpen));

  // The command to execute when opening this application via the Metro UI.
  const base::string16 delegate_command(
      ShellUtil::GetChromeDelegateCommand(chrome_exe));

  // Each of Chrome's shortcuts has an appid; which, as of Windows 8, is
  // registered to handle some verbs. This registration has the side-effect
  // that these verbs now show up in the shortcut's context menu. We
  // mitigate this side-effect by making the context menu entries
  // user readable/localized strings. See relevant MSDN article:
  // http://msdn.microsoft.com/en-US/library/windows/desktop/cc144171.aspx
  static const struct {
    const wchar_t* verb;
    int name_id;
  } verbs[] = {
      {ShellUtil::kRegVerbOpen, -1},
      {ShellUtil::kRegVerbOpenNewWindow, IDS_SHORTCUT_NEW_WINDOW_BASE},
  };
  for (const auto& verb_and_id : verbs) {
    base::string16 sub_path(app_id_shell_key);
    sub_path.push_back(base::FilePath::kSeparators[0]);
    sub_path.append(verb_and_id.verb);

    // <root hkey>\Software\Classes\<app_id>\.exe\shell\<verb>
    if (verb_and_id.name_id != -1) {
      // TODO(grt): http://crbug.com/75152 Write a reference to a localized
      // resource.
      const base::string16 verb_name(
          installer::GetLocalizedString(verb_and_id.name_id));
      entries.push_back(
          std::make_unique<RegistryEntry>(sub_path, verb_name.c_str()));
    }
    entries.push_back(std::make_unique<RegistryEntry>(sub_path, L"CommandId",
                                                      L"Browser.Launch"));

    sub_path.push_back(base::FilePath::kSeparators[0]);
    sub_path.append(ShellUtil::kRegCommand);

    // <root hkey>\Software\Classes\<app_id>\.exe\shell\<verb>\command
    entries.push_back(
        std::make_unique<RegistryEntry>(sub_path, delegate_command));
    entries.push_back(std::make_unique<RegistryEntry>(
        sub_path, ShellUtil::kRegDelegateExecute, app_info.delegate_clsid));
  }

  return entries;
}

// Gets the registry entries to register an application in the Windows registry.
// |app_info| provides all of the information needed.
void GetProgIdEntries(const ApplicationInfo& app_info,
                      std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  // Basic sanity checks.
  DCHECK(!app_info.prog_id.empty());
  DCHECK_NE(L'.', app_info.prog_id[0]);

  // File association ProgId
  base::string16 prog_id_path(ShellUtil::kRegClasses);
  prog_id_path.push_back(base::FilePath::kSeparators[0]);
  prog_id_path.append(app_info.prog_id);
  entries->push_back(
      std::make_unique<RegistryEntry>(prog_id_path, app_info.file_type_name));
  entries->push_back(std::make_unique<RegistryEntry>(
      prog_id_path + ShellUtil::kRegDefaultIcon,
      ShellUtil::FormatIconLocation(app_info.file_type_icon_path,
                                    app_info.file_type_icon_index)));
  entries->push_back(std::make_unique<RegistryEntry>(
      prog_id_path + ShellUtil::kRegShellOpen, app_info.command_line));
  if (!app_info.delegate_clsid.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        prog_id_path + ShellUtil::kRegShellOpen, ShellUtil::kRegDelegateExecute,
        app_info.delegate_clsid));
    // TODO(scottmg): Simplify after Metro removal. https://crbug.com/558054.
    entries->back()->set_removal_flag(RegistryEntry::RemovalFlag::VALUE);
  }

  // The following entries are required as of Windows 8, but do not
  // depend on the DelegateExecute verb handler being set.
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    if (!app_info.app_id.empty()) {
      entries->push_back(std::make_unique<RegistryEntry>(
          prog_id_path, ShellUtil::kRegAppUserModelId, app_info.app_id));
    }
  }

  // Add \Software\Classes\<prog_id>\Application entries
  base::string16 application_path(prog_id_path + ShellUtil::kRegApplication);
  if (!app_info.app_id.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        application_path, ShellUtil::kRegAppUserModelId, app_info.app_id));
  }
  if (!app_info.application_icon_path.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        application_path, ShellUtil::kRegApplicationIcon,
        ShellUtil::FormatIconLocation(app_info.application_icon_path,
                                      app_info.application_icon_index)));
  }
  if (!app_info.application_name.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        application_path, ShellUtil::kRegApplicationName,
        app_info.application_name));
  }
  if (!app_info.application_description.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        application_path, ShellUtil::kRegApplicationDescription,
        app_info.application_description));
  }
  if (!app_info.publisher_name.empty()) {
    entries->push_back(std::make_unique<RegistryEntry>(
        application_path, ShellUtil::kRegApplicationCompany,
        app_info.publisher_name));
  }
}

// This method returns a list of all the registry entries that are needed to
// register this installation's ProgId and AppId. These entries need to be
// registered in HKLM prior to Win8.
void GetChromeProgIdEntries(
    const base::FilePath& chrome_exe,
    const base::string16& suffix,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  int chrome_icon_index = install_static::GetIconResourceIndex();

  ApplicationInfo app_info;
  app_info.prog_id = GetBrowserProgId(suffix);
  app_info.file_type_name = install_static::GetProgIdDescription();
  // File types associated with Chrome are just given the Chrome icon.
  app_info.file_type_icon_path = chrome_exe;
  app_info.file_type_icon_index = chrome_icon_index;
  app_info.command_line = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
  // For user-level installs: entries for the app id will be in HKCU; thus we
  // do not need a suffix on those entries.
  app_info.app_id =
      ShellUtil::GetBrowserModelId(InstallUtil::IsPerUserInstall());

  // TODO(grt): http://crbug.com/75152 Write a reference to a localized
  // resource for name, description, and company.
  app_info.application_name = InstallUtil::GetDisplayName();
  app_info.application_icon_path = chrome_exe;
  app_info.application_icon_index = chrome_icon_index;
  app_info.application_description = InstallUtil::GetAppDescription();
  app_info.publisher_name = InstallUtil::GetPublisherName();
  app_info.delegate_clsid = install_static::GetLegacyCommandExecuteImplClsid();

  GetProgIdEntries(app_info, entries);

  if (!app_info.delegate_clsid.empty()) {
    auto delegate_execute_entries =
        GetChromeDelegateExecuteEntries(chrome_exe, app_info);
    // Remove the keys (not only their values) so that Windows will continue
    // to launch Chrome without a pesky association error.
    // TODO(scottmg): Simplify after Metro removal. https://crbug.com/558054.
    for (const auto& entry : delegate_execute_entries)
      entry->set_removal_flag(RegistryEntry::RemovalFlag::KEY);
    // Move |delegate_execute_entries| to |entries|.
    std::move(delegate_execute_entries.begin(), delegate_execute_entries.end(),
              std::back_inserter(*entries));
  }
}

// This method returns a list of the registry entries needed to declare a
// capability of handling a protocol on Windows.
void GetProtocolCapabilityEntries(
    const base::string16& suffix,
    const base::string16& protocol,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  entries->push_back(std::make_unique<RegistryEntry>(
      GetCapabilitiesKey(suffix).append(L"\\URLAssociations"), protocol,
      GetBrowserProgId(suffix)));
}

// This method returns a list of the registry entries required to register this
// installation in "RegisteredApplications" on Windows (to appear in Default
// Programs, StartMenuInternet, etc.). These entries need to be registered in
// HKLM prior to Win8. If |suffix| is not empty, these entries are guaranteed to
// be unique on this machine.
void GetShellIntegrationEntries(
    const base::FilePath& chrome_exe,
    const base::string16& suffix,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  const base::string16 icon_path(ShellUtil::FormatIconLocation(
      chrome_exe, install_static::GetIconResourceIndex()));
  const base::string16 quoted_exe_path(L"\"" + chrome_exe.value() + L"\"");

  // Register for the Start Menu "Internet" link (pre-Win7).
  const base::string16 start_menu_entry(GetBrowserClientKey(suffix));
  // Register Chrome's display name.
  // TODO(grt): http://crbug.com/75152 Also set LocalizedString; see
  // http://msdn.microsoft.com/en-us/library/windows/desktop/cc144109(v=VS.85).aspx#registering_the_display_name
  entries->push_back(std::make_unique<RegistryEntry>(
      start_menu_entry, InstallUtil::GetDisplayName()));
  // Register the "open" verb for launching Chrome via the "Internet" link.
  entries->push_back(std::make_unique<RegistryEntry>(
      start_menu_entry + ShellUtil::kRegShellOpen, quoted_exe_path));
  // Register Chrome's icon for the Start Menu "Internet" link.
  entries->push_back(std::make_unique<RegistryEntry>(
      start_menu_entry + ShellUtil::kRegDefaultIcon, icon_path));

  // Register installation information.
  base::string16 install_info(start_menu_entry + L"\\InstallInfo");
  // Note: not using CommandLine since it has ambiguous rules for quoting
  // strings.
  entries->push_back(std::make_unique<RegistryEntry>(
      install_info, kReinstallCommand,
      quoted_exe_path + L" --" +
          base::ASCIIToUTF16(switches::kMakeDefaultBrowser)));
  entries->push_back(std::make_unique<RegistryEntry>(
      install_info, L"HideIconsCommand",
      quoted_exe_path + L" --" + base::ASCIIToUTF16(switches::kHideIcons)));
  entries->push_back(std::make_unique<RegistryEntry>(
      install_info, L"ShowIconsCommand",
      quoted_exe_path + L" --" + base::ASCIIToUTF16(switches::kShowIcons)));
  entries->push_back(
      std::make_unique<RegistryEntry>(install_info, L"IconsVisible", 1));

  // Register with Default Programs.
  const base::string16 reg_app_name(
      install_static::GetBaseAppName().append(suffix));
  // Tell Windows where to find Chrome's Default Programs info.
  const base::string16 capabilities(GetCapabilitiesKey(suffix));
  entries->push_back(std::make_unique<RegistryEntry>(
      ShellUtil::kRegRegisteredApplications, reg_app_name, capabilities));
  // Write out Chrome's Default Programs info.
  // TODO(grt): http://crbug.com/75152 Write a reference to a localized
  // resource rather than this.
  entries->push_back(std::make_unique<RegistryEntry>(
      capabilities, ShellUtil::kRegApplicationDescription,
      InstallUtil::GetLongAppDescription()));
  entries->push_back(std::make_unique<RegistryEntry>(
      capabilities, ShellUtil::kRegApplicationIcon, icon_path));
  entries->push_back(std::make_unique<RegistryEntry>(
      capabilities, ShellUtil::kRegApplicationName,
      InstallUtil::GetDisplayName()));

  entries->push_back(std::make_unique<RegistryEntry>(
      capabilities + L"\\Startmenu", L"StartMenuInternet", reg_app_name));

  const base::string16 html_prog_id(GetBrowserProgId(suffix));
  for (int i = 0; ShellUtil::kPotentialFileAssociations[i] != nullptr; i++) {
    entries->push_back(std::make_unique<RegistryEntry>(
        capabilities + L"\\FileAssociations",
        ShellUtil::kPotentialFileAssociations[i], html_prog_id));
  }
  for (int i = 0; ShellUtil::kPotentialProtocolAssociations[i] != nullptr;
       i++) {
    entries->push_back(std::make_unique<RegistryEntry>(
        capabilities + L"\\URLAssociations",
        ShellUtil::kPotentialProtocolAssociations[i], html_prog_id));
  }
}

// Gets the registry entries to register an application as a handler for a
// particular file extension. |prog_id| is the ProgId used by Windows for the
// application. |ext| is the file extension, which must begin with a '.'.
void GetAppExtRegistrationEntries(
    const base::string16& prog_id,
    const base::string16& ext,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  // In HKEY_CURRENT_USER\Software\Classes\EXT\OpenWithProgids, create an
  // empty value with this class's ProgId.
  base::string16 key_name(ShellUtil::kRegClasses);
  key_name.push_back(base::FilePath::kSeparators[0]);
  key_name.append(ext);
  key_name.push_back(base::FilePath::kSeparators[0]);
  key_name.append(ShellUtil::kRegOpenWithProgids);
  entries->push_back(
      std::make_unique<RegistryEntry>(key_name, prog_id, base::string16()));
}

// This method returns a list of the registry entries required for this
// installation to be registered in the Windows shell.
// In particular:
//  - App Paths
//    http://msdn.microsoft.com/en-us/library/windows/desktop/ee872121
//  - File Associations
//    http://msdn.microsoft.com/en-us/library/bb166549
// These entries need to be registered in HKLM prior to Win8.
void GetChromeAppRegistrationEntries(
    const base::FilePath& chrome_exe,
    const base::string16& suffix,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  base::string16 app_path_key(ShellUtil::kAppPathsRegistryKey);
  app_path_key.push_back(base::FilePath::kSeparators[0]);
  app_path_key.append(chrome_exe.BaseName().value());
  entries->push_back(
      std::make_unique<RegistryEntry>(app_path_key, chrome_exe.value()));
  entries->push_back(std::make_unique<RegistryEntry>(
      app_path_key, ShellUtil::kAppPathsRegistryPathName,
      chrome_exe.DirName().value()));

  const base::string16 html_prog_id(GetBrowserProgId(suffix));
  for (int i = 0; ShellUtil::kPotentialFileAssociations[i] != nullptr; i++) {
    GetAppExtRegistrationEntries(
        html_prog_id, ShellUtil::kPotentialFileAssociations[i], entries);
  }
}

// Gets the registry entries to register an application as the default handler
// for a particular file extension. |prog_id| is the ProgId used by Windows for
// the application. |ext| is the file extension, which must begin with a '.'. If
// |overwrite_existing|, always sets the default handler; otherwise only sets if
// there is no existing default.
//
// This has no effect on Windows 8. Windows 8 ignores the default and lets the
// user choose. If there is only one handler for a file, it will automatically
// become the default. Otherwise, the first time the user opens a file, they are
// presented with the dialog to set the default handler. (This is roughly
// equivalent to being called with |overwrite_existing| false.)
void GetAppDefaultRegistrationEntries(
    const base::string16& prog_id,
    const base::string16& ext,
    bool overwrite_existing,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  // Set the default value of HKEY_CURRENT_USER\Software\Classes\EXT to this
  // class's name.
  base::string16 key_name(ShellUtil::kRegClasses);
  key_name.push_back(base::FilePath::kSeparators[0]);
  key_name.append(ext);
  auto default_association = std::make_unique<RegistryEntry>(key_name, prog_id);
  if (overwrite_existing ||
      !default_association->KeyExistsInRegistry(RegistryEntry::LOOK_IN_HKCU)) {
    entries->push_back(std::move(default_association));
  }
}

// This method returns a list of all the user level registry entries that are
// needed to make Chromium the default handler for a protocol on XP.
void GetXPStyleUserProtocolEntries(
    const base::string16& protocol,
    const base::string16& chrome_icon,
    const base::string16& chrome_open,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  // Protocols associations.
  base::string16 url_key(ShellUtil::kRegClasses);
  url_key.push_back(base::FilePath::kSeparators[0]);
  url_key.append(protocol);

  // This registry value tells Windows that this 'class' is a URL scheme
  // so IE, explorer and other apps will route it to our handler.
  // <root hkey>\Software\Classes\<protocol>\URL Protocol
  entries->push_back(std::make_unique<RegistryEntry>(
      url_key, ShellUtil::kRegUrlProtocol, base::string16()));

  // <root hkey>\Software\Classes\<protocol>\DefaultIcon
  base::string16 icon_key = url_key + ShellUtil::kRegDefaultIcon;
  entries->push_back(std::make_unique<RegistryEntry>(icon_key, chrome_icon));

  // <root hkey>\Software\Classes\<protocol>\shell\open\command
  base::string16 shell_key = url_key + ShellUtil::kRegShellOpen;
  entries->push_back(std::make_unique<RegistryEntry>(shell_key, chrome_open));

  // <root hkey>\Software\Classes\<protocol>\shell\open\ddeexec
  base::string16 dde_key = url_key + L"\\shell\\open\\ddeexec";
  entries->push_back(
      std::make_unique<RegistryEntry>(dde_key, base::string16()));

  // <root hkey>\Software\Classes\<protocol>\shell\@
  base::string16 protocol_shell_key = url_key + ShellUtil::kRegShellPath;
  entries->push_back(
      std::make_unique<RegistryEntry>(protocol_shell_key, L"open"));
}

// This method returns a list of all the user level registry entries that are
// needed to make Chromium default browser on XP. Some of these entries are
// irrelevant in recent versions of Windows, but we register them anyways as
// some legacy apps are hardcoded to lookup those values.
void GetXPStyleDefaultBrowserUserEntries(
    const base::FilePath& chrome_exe,
    const base::string16& suffix,
    std::vector<std::unique_ptr<RegistryEntry>>* entries) {
  // File extension associations.
  base::string16 html_prog_id(GetBrowserProgId(suffix));
  for (int i = 0; ShellUtil::kDefaultFileAssociations[i] != nullptr; i++) {
    GetAppDefaultRegistrationEntries(
        html_prog_id, ShellUtil::kDefaultFileAssociations[i], true, entries);
  }

  // Protocols associations.
  base::string16 chrome_open = ShellUtil::GetChromeShellOpenCmd(chrome_exe);
  base::string16 chrome_icon = ShellUtil::FormatIconLocation(
      chrome_exe, install_static::GetIconResourceIndex());
  for (int i = 0; ShellUtil::kBrowserProtocolAssociations[i] != nullptr; i++) {
    GetXPStyleUserProtocolEntries(ShellUtil::kBrowserProtocolAssociations[i],
                                  chrome_icon, chrome_open, entries);
  }

  // start->Internet shortcut.
  base::string16 start_menu(ShellUtil::kRegStartMenuInternet);
  base::string16 app_name = install_static::GetBaseAppName().append(suffix);
  entries->push_back(std::make_unique<RegistryEntry>(start_menu, app_name));
}

// Checks that all |entries| are present on this computer (or absent if their
// |removal_flag_| is set). |look_for_in| is passed to
// RegistryEntry::ExistsInRegistry(). Documentation for it can be found there.
bool AreEntriesAsDesired(
    const std::vector<std::unique_ptr<RegistryEntry>>& entries,
    uint32_t look_for_in) {
  for (const auto& entry : entries) {
    if (entry->ExistsInRegistry(look_for_in) != !entry->IsFlaggedForRemoval())
      return false;
  }
  return true;
}

// Checks that all required registry entries for Chrome are already present on
// this computer (or absent if their |removal_flag_| is set).
// See RegistryEntry::ExistsInRegistry for the behavior of |look_for_in|.
// Note: between r133333 and r154145 we were registering parts of Chrome in HKCU
// and parts in HKLM for user-level installs; we now always register everything
// under a single registry root. Not doing so caused http://crbug.com/144910 for
// users who first-installed Chrome in that revision range (those users are
// still impacted by http://crbug.com/144910). This method will keep returning
// true for affected users (i.e. who have all the registrations, but over both
// registry roots).
bool IsChromeRegistered(const base::FilePath& chrome_exe,
                        const base::string16& suffix,
                        uint32_t look_for_in) {
  std::vector<std::unique_ptr<RegistryEntry>> entries;
  GetChromeProgIdEntries(chrome_exe, suffix, &entries);
  GetShellIntegrationEntries(chrome_exe, suffix, &entries);
  GetChromeAppRegistrationEntries(chrome_exe, suffix, &entries);
  return AreEntriesAsDesired(entries, look_for_in);
}

// This method checks if Chrome is already registered on the local machine
// for the requested protocol. It just checks the one value required for this.
// See RegistryEntry::ExistsInRegistry for the behavior of |look_for_in|.
bool IsChromeRegisteredForProtocol(const base::string16& suffix,
                                   const base::string16& protocol,
                                   uint32_t look_for_in) {
  std::vector<std::unique_ptr<RegistryEntry>> entries;
  GetProtocolCapabilityEntries(suffix, protocol, &entries);
  return AreEntriesAsDesired(entries, look_for_in);
}

// This method registers Chrome by launching an elevated setup.exe. That will
// show the user the standard elevation prompt. If the user accepts it the new
// process will make the necessary changes and return SUCCESS that we capture
// and return. If protocol is non-empty we will also register Chrome as being
// capable of handling the protocol. This is used for general browser
// registration on Windows 7 for per-user installs where setup.exe did not have
// permission to register Chrome during install. It may also be used on Windows
// 7 for system-level installs to register Chrome for a specific protocol.
bool ElevateAndRegisterChrome(const base::FilePath& chrome_exe,
                              const base::string16& suffix,
                              const base::string16& protocol) {
  // Check for setup.exe in the same directory as chrome.exe, as is the case
  // when running out of a build output directory.
  base::FilePath exe_path = chrome_exe.DirName().Append(installer::kSetupExe);

  // Failing that, read the path to setup.exe from Chrome's ClientState key,
  // which is the canonical location of the installer for all types of installs
  // (see AddUninstallShortcutWorkItems).
  const bool is_per_user = InstallUtil::IsPerUserInstall();
  if (!base::PathExists(exe_path)) {
    RegKey key(is_per_user ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE,
               install_static::GetClientStateKeyPath().c_str(),
               KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    base::string16 uninstall_string;
    if (key.ReadValue(installer::kUninstallStringField, &uninstall_string) ==
        ERROR_SUCCESS) {
      exe_path = base::FilePath(uninstall_string);
    }
  }

  if (base::PathExists(exe_path)) {
    base::CommandLine cmd(exe_path);
    InstallUtil::AppendModeAndChannelSwitches(&cmd);
    if (!is_per_user)
      cmd.AppendSwitch(installer::switches::kSystemLevel);
    cmd.AppendSwitchPath(installer::switches::kRegisterChromeBrowser,
                         chrome_exe);
    if (!suffix.empty()) {
      cmd.AppendSwitchNative(installer::switches::kRegisterChromeBrowserSuffix,
                             suffix);
    }

    if (!protocol.empty()) {
      cmd.AppendSwitchNative(installer::switches::kRegisterURLProtocol,
                             protocol);
    }

    DWORD ret_val = 0;
    InstallUtil::ExecuteExeAsAdmin(cmd, &ret_val);
    if (ret_val == 0)
      return true;
  }
  return false;
}

// Returns the target used as a activate parameter when opening the settings
// pointing to the page that is the most relevant to a user trying to change the
// default handler for |protocol|.
base::string16 GetTargetForDefaultAppsSettings(const wchar_t* protocol) {
  static const wchar_t kSystemSettingsDefaultAppsFormat[] =
      L"SystemSettings_DefaultApps_%ls";

  if (base::EqualsCaseInsensitiveASCII(protocol, L"http"))
    return base::StringPrintf(kSystemSettingsDefaultAppsFormat, L"Browser");
  if (base::EqualsCaseInsensitiveASCII(protocol, L"mailto"))
    return base::StringPrintf(kSystemSettingsDefaultAppsFormat, L"Email");
  return L"SettingsPageAppsDefaultsProtocolView";
}

// Launches the Windows 'settings' modern app with the 'default apps' view
// focused. This only works for Windows 8 and Windows 10. The appModelId
// looks arbitrary but it is the same in Win8 and Win10. There is no easy way to
// retrieve the appModelId from the registry.
bool LaunchDefaultAppsSettingsModernDialog(const wchar_t* protocol) {
  DCHECK(protocol);
  static const wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";

  static constexpr base::Feature kHighlightProtocolInWindowsSettings{
      "HighlightProtocolInWindowsSettings", base::FEATURE_ENABLED_BY_DEFAULT};

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&activator));
  if (SUCCEEDED(hr)) {
    DWORD pid = 0;
    CoAllowSetForegroundWindow(activator.Get(), nullptr);
    hr = activator->ActivateApplication(kControlPanelAppModelId,
                                        L"page=SettingsPageAppsDefaults",
                                        AO_NONE, &pid);
    if (SUCCEEDED(hr) &&
        base::FeatureList::IsEnabled(kHighlightProtocolInWindowsSettings)) {
      hr = activator->ActivateApplication(
          kControlPanelAppModelId,
          base::StringPrintf(L"page=SettingsPageAppsDefaults&target=%ls",
                             GetTargetForDefaultAppsSettings(protocol).c_str())
              .c_str(),
          AO_NONE, &pid);
    }
    if (SUCCEEDED(hr))
      return true;
    base::UmaHistogramSparse("DefaultBrowser.ActivateSettings.ErrorHresult",
                             hr);
  }
  return false;
}

// Launches the Windows 7 and Windows 8 dialog for picking the application to
// handle the given protocol. Most importantly, this is used to set the default
// handler for http (and, implicitly with it, https). In that case it is also
// known as the 'how do you want to open webpages' dialog.
// It is required that Chrome be already *registered* for the given protocol.
bool LaunchSelectDefaultProtocolHandlerDialog(const wchar_t* protocol) {
  DCHECK(protocol);
  OPENASINFO open_as_info = {};
  open_as_info.pcszFile = protocol;
  open_as_info.oaifInFlags =
      OAIF_URL_PROTOCOL | OAIF_FORCE_REGISTRATION | OAIF_REGISTER_EXT;
  HRESULT hr = SHOpenWithDialog(nullptr, &open_as_info);
  DLOG_IF(WARNING, FAILED(hr)) << "Failed to set as default " << protocol
                               << " handler; hr=0x" << std::hex << hr;
  if (FAILED(hr))
    return false;
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return true;
}

// Returns true if |chrome_exe| has been registered with |suffix| for |mode|.
// |confirmation_level| is the level of verification desired as described in
// the RegistrationConfirmationLevel enum above.
// |suffix| can be the empty string (this is used to support old installs
// where we used to not suffix user-level installs if they were the first to
// request the non-suffixed registry entries on the machine).
// NOTE: This a quick check that only validates that a single registry entry
// points to |chrome_exe|. This should only be used at run-time to determine
// how Chrome is registered, not to know whether the registration is complete
// at install-time (IsChromeRegistered() can be used for that).
bool QuickIsChromeRegisteredForMode(
    const base::FilePath& chrome_exe,
    const base::string16& suffix,
    const install_static::InstallConstants& mode,
    RegistrationConfirmationLevel confirmation_level) {
  // Get the appropriate key to look for based on the level desired.
  base::string16 reg_key;
  switch (confirmation_level) {
    case CONFIRM_PROGID_REGISTRATION:
      // Software\Classes\ChromeHTML|suffix|
      reg_key = ShellUtil::kRegClasses;
      reg_key.push_back(base::FilePath::kSeparators[0]);
      reg_key.append(mode.prog_id_prefix);
      reg_key.append(suffix);
      break;
    case CONFIRM_SHELL_REGISTRATION:
    case CONFIRM_SHELL_REGISTRATION_IN_HKLM:
      // Software\Clients\StartMenuInternet\Google Chrome|suffix|
      reg_key = GetBrowserClientKey(suffix);
      break;
    default:
      NOTREACHED();
      break;
  }
  reg_key.append(ShellUtil::kRegShellOpen);

  // ProgId registrations are allowed to reside in HKCU for user-level installs
  // (and values there have priority over values in HKLM). The same is true for
  // shell integration entries as of Windows 8.
  if (confirmation_level == CONFIRM_PROGID_REGISTRATION ||
      (confirmation_level == CONFIRM_SHELL_REGISTRATION &&
       base::win::GetVersion() >= base::win::Version::WIN8)) {
    const RegKey key_hkcu(HKEY_CURRENT_USER, reg_key.c_str(), KEY_QUERY_VALUE);
    base::string16 hkcu_value;
    // If |reg_key| is present in HKCU, assert that it points to |chrome_exe|.
    // Otherwise, fall back on an HKLM lookup below.
    if (key_hkcu.ReadValue(L"", &hkcu_value) == ERROR_SUCCESS)
      return InstallUtil::ProgramCompare(chrome_exe).Evaluate(hkcu_value);
  }

  // Assert that |reg_key| points to |chrome_exe| in HKLM.
  const RegKey key_hklm(HKEY_LOCAL_MACHINE, reg_key.c_str(), KEY_QUERY_VALUE);
  base::string16 hklm_value;
  if (key_hklm.ReadValue(L"", &hklm_value) == ERROR_SUCCESS)
    return InstallUtil::ProgramCompare(chrome_exe).Evaluate(hklm_value);
  return false;
}

// Returns the installation suffix for |mode| at the system or user level based
// on |system_install|.
base::string16 GetInstallationSuffixForModeAtLevel(
    const install_static::InstallConstants& mode,
    bool system_install) {
  // Search based on the existing install location. If no existing install
  // found, uses the default install location for the mode.
  const base::FilePath chrome_exe =
      installer::GetChromeInstallPath(system_install)
          .Append(installer::kChromeExe);

  // See the comment in ShellUtil::GetCurrentInstallationSuffix for details on
  // what's going on here.
  base::string16 tested_suffix;
  if (!system_install &&
      (!ShellUtil::GetUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegisteredForMode(chrome_exe, tested_suffix, mode,
                                       CONFIRM_PROGID_REGISTRATION)) &&
      (!ShellUtil::GetOldUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegisteredForMode(chrome_exe, tested_suffix, mode,
                                       CONFIRM_PROGID_REGISTRATION)) &&
      !QuickIsChromeRegisteredForMode(chrome_exe, tested_suffix.erase(), mode,
                                      CONFIRM_PROGID_REGISTRATION)) {
    // If Chrome is not registered under any of the possible suffixes (e.g.
    // tests, Canary, etc.): use the new-style suffix at run-time.
    if (!ShellUtil::GetUserSpecificRegistrySuffix(&tested_suffix))
      NOTREACHED();
  }
  return tested_suffix;
}

// Returns |mode|'s application name at the system or user level based on
// |system_install|. This application name will be suffixed as is appropriate
// for the install. This is the name that is registered with Default Programs on
// Windows and that should thus be used to "make chrome default" and such.
base::string16 GetApplicationNameForModeAtLevel(
    const install_static::InstallConstants& mode,
    bool system_install) {
  return base::string16(mode.base_app_name)
      .append(GetInstallationSuffixForModeAtLevel(mode, system_install));
}

// Returns true if the current install's |chrome_exe| has been registered with
// |suffix|.
// |confirmation_level| is the level of verification desired as described in
// the RegistrationConfirmationLevel enum above.
// |suffix| can be the empty string (this is used to support old installs
// where we used to not suffix user-level installs if they were the first to
// request the non-suffixed registry entries on the machine).
// NOTE: This a quick check that only validates that a single registry entry
// points to |chrome_exe|. This should only be used at run-time to determine
// how Chrome is registered, not to know whether the registration is complete
// at install-time (IsChromeRegistered() can be used for that).
bool QuickIsChromeRegistered(const base::FilePath& chrome_exe,
                             const base::string16& suffix,
                             RegistrationConfirmationLevel confirmation_level) {
  return QuickIsChromeRegisteredForMode(
      chrome_exe, suffix, install_static::InstallDetails::Get().mode(),
      confirmation_level);
}

// Sets |suffix| to a 27 character string that is specific to this user on this
// machine (on user-level installs only).
// To support old-style user-level installs however, |suffix| is cleared if the
// user currently owns the non-suffixed HKLM registrations.
// |suffix| can also be set to the user's username if the current install is
// suffixed as per the old-style registrations.
// |suffix| is cleared on system-level installs.
// |suffix| should then be appended to all Chrome properties that may conflict
// with other Chrome user-level installs.
// Returns true unless one of the underlying calls fails.
bool GetInstallationSpecificSuffix(const base::FilePath& chrome_exe,
                                   base::string16* suffix) {
  if (!InstallUtil::IsPerUserInstall() ||
      QuickIsChromeRegistered(chrome_exe, base::string16(),
                              CONFIRM_SHELL_REGISTRATION)) {
    // No suffix on system-level installs and user-level installs already
    // registered with no suffix.
    suffix->clear();
    return true;
  }

  // Get the old suffix for the check below.
  if (!ShellUtil::GetOldUserSpecificRegistrySuffix(suffix)) {
    NOTREACHED();
    return false;
  }
  if (QuickIsChromeRegistered(chrome_exe, *suffix,
                              CONFIRM_SHELL_REGISTRATION)) {
    // Username suffix for installs that are suffixed as per the old-style.
    return true;
  }

  return ShellUtil::GetUserSpecificRegistrySuffix(suffix);
}

// Returns the root registry key (HKLM or HKCU) under which registrations must
// be placed for this install. As of Windows 8 everything can go in HKCU for
// per-user installs.
HKEY DetermineRegistrationRoot(bool is_per_user) {
  return is_per_user && base::win::GetVersion() >= base::win::Version::WIN8
             ? HKEY_CURRENT_USER
             : HKEY_LOCAL_MACHINE;
}

// Associates Chrome with supported protocols and file associations. This should
// not be required on Vista+ but since some applications still read
// Software\Classes\http key directly, we have to do this on Vista+ as well.
bool RegisterChromeAsDefaultXPStyle(int shell_change,
                                    const base::FilePath& chrome_exe) {
  bool ret = true;
  std::vector<std::unique_ptr<RegistryEntry>> entries;
  GetXPStyleDefaultBrowserUserEntries(
      chrome_exe, ShellUtil::GetCurrentInstallationSuffix(chrome_exe),
      &entries);

  // Change the default browser for current user.
  if ((shell_change & ShellUtil::CURRENT_USER) &&
      !ShellUtil::AddRegistryEntries(HKEY_CURRENT_USER, entries)) {
    ret = false;
    LOG(ERROR) << "Could not make Chrome default browser (XP/current user).";
  }

  // Chrome as default browser at system level.
  if ((shell_change & ShellUtil::SYSTEM_LEVEL) &&
      !ShellUtil::AddRegistryEntries(HKEY_LOCAL_MACHINE, entries)) {
    ret = false;
    LOG(ERROR) << "Could not make Chrome default browser (XP/system level).";
  }

  return ret;
}

// Associates Chrome with |protocol| in the registry. This should not be
// required on Vista+ but since some applications still read these registry
// keys directly, we have to do this on Vista+ as well.
// See http://msdn.microsoft.com/library/aa767914.aspx for more details.
bool RegisterChromeAsDefaultProtocolClientXPStyle(
    const base::FilePath& chrome_exe,
    const base::string16& protocol) {
  std::vector<std::unique_ptr<RegistryEntry>> entries;
  const base::string16 chrome_open(
      ShellUtil::GetChromeShellOpenCmd(chrome_exe));
  const base::string16 chrome_icon(ShellUtil::FormatIconLocation(
      chrome_exe, install_static::GetIconResourceIndex()));
  GetXPStyleUserProtocolEntries(protocol, chrome_icon, chrome_open, &entries);
  // Change the default protocol handler for current user.
  if (!ShellUtil::AddRegistryEntries(HKEY_CURRENT_USER, entries)) {
    LOG(ERROR) << "Could not make Chrome default protocol client (XP).";
    return false;
  }

  return true;
}

// Returns |properties.shortcut_name| if the property is set, otherwise it
// returns InstallUtil::GetShortcutName(). In any case, it makes sure the return
// value is suffixed with ".lnk".
base::string16 ExtractShortcutNameFromProperties(
    const ShellUtil::ShortcutProperties& properties) {
  base::string16 shortcut_name = properties.has_shortcut_name()
                                     ? properties.shortcut_name
                                     : InstallUtil::GetShortcutName();

  if (!base::EndsWith(shortcut_name, installer::kLnkExt,
                      base::CompareCase::INSENSITIVE_ASCII))
    shortcut_name.append(installer::kLnkExt);

  return shortcut_name;
}

// Converts ShellUtil::ShortcutOperation to the best-matching value in
// base::win::ShortcutOperation.
base::win::ShortcutOperation TranslateShortcutOperation(
    ShellUtil::ShortcutOperation operation) {
  switch (operation) {
    case ShellUtil::SHELL_SHORTCUT_CREATE_ALWAYS:  // Falls through.
    case ShellUtil::SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL:
      return base::win::SHORTCUT_CREATE_ALWAYS;

    case ShellUtil::SHELL_SHORTCUT_UPDATE_EXISTING:
      return base::win::SHORTCUT_UPDATE_EXISTING;

    case ShellUtil::SHELL_SHORTCUT_REPLACE_EXISTING:
      return base::win::SHORTCUT_REPLACE_EXISTING;

    default:
      NOTREACHED();
      return base::win::SHORTCUT_REPLACE_EXISTING;
  }
}

// Returns a base::win::ShortcutProperties struct containing the properties
// to set on the shortcut based on the provided ShellUtil::ShortcutProperties.
base::win::ShortcutProperties TranslateShortcutProperties(
    const ShellUtil::ShortcutProperties& properties) {
  base::win::ShortcutProperties shortcut_properties;

  if (properties.has_target()) {
    shortcut_properties.set_target(properties.target);
    DCHECK(!properties.target.DirName().empty());
    shortcut_properties.set_working_dir(properties.target.DirName());
  }

  if (properties.has_arguments())
    shortcut_properties.set_arguments(properties.arguments);

  if (properties.has_description())
    shortcut_properties.set_description(properties.description);

  if (properties.has_icon())
    shortcut_properties.set_icon(properties.icon, properties.icon_index);

  if (properties.has_app_id())
    shortcut_properties.set_app_id(properties.app_id);

  if (properties.has_toast_activator_clsid()) {
    shortcut_properties.set_toast_activator_clsid(
        properties.toast_activator_clsid);
  }

  return shortcut_properties;
}

// Cleans up an old verb (run) we used to register in
// <root>\Software\Classes\Chrome<.suffix>\.exe\shell\run on Windows 8.
void RemoveRunVerbOnWindows8() {
  if (base::win::GetVersion() >= base::win::Version::WIN8) {
    bool is_per_user_install = InstallUtil::IsPerUserInstall();
    HKEY root_key = DetermineRegistrationRoot(is_per_user_install);
    // There's no need to rollback, so forgo the usual work item lists and just
    // remove the key from the registry.
    base::string16 run_verb_key(ShellUtil::kRegClasses);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::GetBrowserModelId(is_per_user_install));
    run_verb_key.append(ShellUtil::kRegExePath);
    run_verb_key.append(ShellUtil::kRegShellPath);
    run_verb_key.push_back(base::FilePath::kSeparators[0]);
    run_verb_key.append(ShellUtil::kRegVerbRun);
    InstallUtil::DeleteRegistryKey(root_key, run_verb_key,
                                   WorkItem::kWow64Default);
  }
}

// Probe using IApplicationAssociationRegistration::QueryCurrentDefault
// (Windows 8); see ProbeProtocolHandlers.  This mechanism is not suitable for
// use on previous versions of Windows despite the presence of
// QueryCurrentDefault on them since versions of Windows prior to Windows 8
// did not perform validation on the ProgID registered as the current default.
// As a result, stale ProgIDs could be returned, leading to false positives.
ShellUtil::DefaultState ProbeCurrentDefaultHandlers(
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                         CLSCTX_INPROC, IID_PPV_ARGS(&registration));
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  // Get the ProgID for the current install mode.
  base::string16 prog_id(install_static::GetProgIdPrefix());
  prog_id += ShellUtil::GetCurrentInstallationSuffix(chrome_exe);

  const int current_install_mode_index =
      install_static::InstallDetails::Get().install_mode_index();
  bool other_mode_is_default = false;
  for (size_t i = 0; i < num_protocols; ++i) {
    base::win::ScopedCoMem<wchar_t> current_app;
    hr = registration->QueryCurrentDefault(protocols[i], AT_URLPROTOCOL,
                                           AL_EFFECTIVE, &current_app);
    if (FAILED(hr))
      return ShellUtil::NOT_DEFAULT;
    if (prog_id.compare(current_app) == 0)
      continue;

    // See if another mode is the default handler for this protocol.
    size_t current_app_len = std::char_traits<wchar_t>::length(current_app);
    const auto* it = std::find_if(
        &install_static::kInstallModes[0],
        &install_static::kInstallModes[install_static::NUM_INSTALL_MODES],
        [current_install_mode_index, &current_app,
         current_app_len](const install_static::InstallConstants& mode) {
          if (mode.index == current_install_mode_index)
            return false;
          const base::string16 mode_prog_id_prefix(mode.prog_id_prefix);
          // Does the current app either match this mode's ProgID or contain
          // this mode's ProgID as a prefix followed by the '.' separator for a
          // per-user install's suffix?
          return mode_prog_id_prefix.compare(current_app) == 0 ||
                 (InstallUtil::IsPerUserInstall() &&
                  current_app_len > mode_prog_id_prefix.length() &&
                  current_app[mode_prog_id_prefix.length()] == L'.' &&
                  std::char_traits<wchar_t>::compare(
                      mode_prog_id_prefix.c_str(), current_app,
                      mode_prog_id_prefix.length()) == 0);
        });
    if (it == &install_static::kInstallModes[install_static::NUM_INSTALL_MODES])
      return ShellUtil::NOT_DEFAULT;
    other_mode_is_default = true;
  }
  // This mode is default if it has all of the protocols.
  return other_mode_is_default ? ShellUtil::OTHER_MODE_IS_DEFAULT
                               : ShellUtil::IS_DEFAULT;
}

// Probe using IApplicationAssociationRegistration::QueryAppIsDefault (Vista and
// Windows 7); see ProbeProtocolHandlers.
ShellUtil::DefaultState ProbeAppIsDefaultHandlers(
    const base::FilePath& chrome_exe,
    const wchar_t* const* protocols,
    size_t num_protocols) {
  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> registration;
  HRESULT hr =
      ::CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr,
                         CLSCTX_INPROC, IID_PPV_ARGS(&registration));
  if (FAILED(hr))
    return ShellUtil::UNKNOWN_DEFAULT;

  base::string16 app_name(GetApplicationName(chrome_exe));

  // Generate the app names for this brand's other install modes at both user
  // and system levels.
  const int current_install_mode_index =
      install_static::InstallDetails::Get().install_mode_index();
  base::string16 other_app_names[install_static::NUM_INSTALL_MODES * 2];
  for (int mode_index = 0; mode_index < install_static::NUM_INSTALL_MODES;
       ++mode_index) {
    if (mode_index == current_install_mode_index)
      continue;  // Leave the entry for the current mode empty.
    other_app_names[mode_index * 2] = GetApplicationNameForModeAtLevel(
        install_static::kInstallModes[mode_index], false);
    other_app_names[mode_index * 2 + 1] = GetApplicationNameForModeAtLevel(
        install_static::kInstallModes[mode_index], true);
  }

  // Now check each protocol to see if this brand is default for all. This loop
  // terminates when this brand is the default handler for the protocols.
  bool other_mode_is_default = false;
  for (size_t i = 0; i < num_protocols; ++i) {
    const wchar_t* protocol = protocols[i];
    BOOL result = TRUE;
    // Check the current app name. This will fail (e.g., ERROR_FILE_NOT_FOUND)
    // if |app_name| isn't registered.
    hr = registration->QueryAppIsDefault(protocol, AT_URLPROTOCOL, AL_EFFECTIVE,
                                         app_name.c_str(), &result);
    if (SUCCEEDED(hr) && result)
      continue;

    // Search for a different install mode that is the default handler.
    const auto* it =
        std::find_if(std::cbegin(other_app_names), std::cend(other_app_names),
                     [&registration, protocol](const base::string16& app_name) {
                       if (app_name.empty())
                         return false;
                       BOOL result = TRUE;
                       HRESULT hr = registration->QueryAppIsDefault(
                           protocol, AT_URLPROTOCOL, AL_EFFECTIVE,
                           app_name.c_str(), &result);
                       return SUCCEEDED(hr) && result;
                     });
    if (it == std::end(other_app_names))
      return ShellUtil::NOT_DEFAULT;
    other_mode_is_default = true;
  }

  return other_mode_is_default ? ShellUtil::OTHER_MODE_IS_DEFAULT
                               : ShellUtil::IS_DEFAULT;
}

// A helper function that probes default protocol handler registration (in a
// manner appropriate for the current version of Windows) to determine if
// Chrome is the default handler for |protocols|.  Returns IS_DEFAULT
// only if Chrome is the default for all specified protocols.
ShellUtil::DefaultState ProbeProtocolHandlers(const base::FilePath& chrome_exe,
                                              const wchar_t* const* protocols,
                                              size_t num_protocols) {
#if DCHECK_IS_ON()
  DCHECK(!num_protocols || protocols);
  for (size_t i = 0; i < num_protocols; ++i)
    DCHECK(protocols[i] && *protocols[i]);
#endif

  const base::win::Version windows_version = base::win::GetVersion();

  if (windows_version >= base::win::Version::WIN8)
    return ProbeCurrentDefaultHandlers(chrome_exe, protocols, num_protocols);

  return ProbeAppIsDefaultHandlers(chrome_exe, protocols, num_protocols);
}

// (Windows 8+) Finds and stores an app shortcuts folder path in *|path|.
// Returns true on success.
bool GetAppShortcutsFolder(ShellUtil::ShellChange level, base::FilePath* path) {
  DCHECK(path);
  DCHECK_GE(base::win::GetVersion(), base::win::Version::WIN8);

  base::FilePath folder;
  if (!base::PathService::Get(base::DIR_APP_SHORTCUTS, &folder)) {
    LOG(ERROR) << "Could not get application shortcuts location.";
    return false;
  }

  folder = folder.Append(
      ShellUtil::GetBrowserModelId(level == ShellUtil::CURRENT_USER));
  if (!base::DirectoryExists(folder)) {
    VLOG(1) << "No start screen shortcuts.";
    return false;
  }

  *path = folder;
  return true;
}

// Shortcut filters for BatchShortcutAction().

using ShortcutFilterCallback =
    base::RepeatingCallback<bool(const base::FilePath& shortcut_path,
                                 const base::string16& args)>;

// FilterTargetEq is a shortcut filter that matches only shortcuts that have a
// specific target, and optionally matches shortcuts that have non-empty
// arguments.
class FilterTargetEq {
 public:
  FilterTargetEq(const base::FilePath& desired_target_exe, bool require_args);

  // Returns true if filter rules are satisfied, i.e.:
  // - |target_path|'s target == |desired_target_compare_|, and
  // - |args| is non-empty (if |require_args_| == true).
  bool Match(const base::FilePath& target_path,
             const base::string16& args) const;

  // A convenience routine to create a callback to call Match().
  // The callback is only valid during the lifetime of the FilterTargetEq
  // instance.
  ShortcutFilterCallback AsShortcutFilterCallback();

 private:
  InstallUtil::ProgramCompare desired_target_compare_;

  bool require_args_;
};

FilterTargetEq::FilterTargetEq(const base::FilePath& desired_target_exe,
                               bool require_args)
    : desired_target_compare_(desired_target_exe),
      require_args_(require_args) {}

bool FilterTargetEq::Match(const base::FilePath& target_path,
                           const base::string16& args) const {
  if (!desired_target_compare_.EvaluatePath(target_path))
    return false;
  if (require_args_ && args.empty())
    return false;
  return true;
}

ShortcutFilterCallback FilterTargetEq::AsShortcutFilterCallback() {
  return base::BindRepeating(&FilterTargetEq::Match, base::Unretained(this));
}

// Shortcut operations for BatchShortcutAction().

using ShortcutOperationCallback =
    base::RepeatingCallback<bool(const base::FilePath& shortcut_path)>;

bool ShortcutOpUnpinFromTaskbar(const base::FilePath& shortcut_path) {
  VLOG(1) << "Trying to unpin from taskbar " << shortcut_path.value();
  if (!base::win::UnpinShortcutFromTaskbar(shortcut_path)) {
    VLOG(1) << shortcut_path.value()
            << " wasn't pinned to taskbar (or the unpin failed).";
    // No error, since shortcut might not be pinned.
  }
  return true;
}

bool ShortcutOpDelete(const base::FilePath& shortcut_path) {
  bool ret = base::DeleteFile(shortcut_path);
  LOG_IF(ERROR, !ret) << "Failed to remove " << shortcut_path.value();
  return ret;
}

bool ShortcutOpRetarget(const base::FilePath& old_target,
                        const base::FilePath& new_target,
                        const base::FilePath& shortcut_path) {
  base::win::ShortcutProperties new_prop;
  new_prop.set_target(new_target);

  // If the old icon matches old target, then update icon while keeping the old
  // icon index. Non-fatal if we fail to get the old icon.
  base::win::ShortcutProperties old_prop;
  if (base::win::ResolveShortcutProperties(
          shortcut_path, base::win::ShortcutProperties::PROPERTIES_ICON,
          &old_prop)) {
    if (InstallUtil::ProgramCompare(old_target).EvaluatePath(old_prop.icon))
      new_prop.set_icon(new_target, old_prop.icon_index);
  } else {
    LOG(ERROR) << "Failed to resolve " << shortcut_path.value();
  }

  bool result = base::win::CreateOrUpdateShortcutLink(
      shortcut_path, new_prop, base::win::SHORTCUT_UPDATE_EXISTING);
  LOG_IF(ERROR, !result) << "Failed to retarget " << shortcut_path.value();
  return result;
}

bool ShortcutOpListOrRemoveUnknownArgs(
    bool do_removal,
    std::vector<std::pair<base::FilePath, base::string16>>* shortcuts,
    const base::FilePath& shortcut_path) {
  base::string16 args;
  if (!base::win::ResolveShortcut(shortcut_path, nullptr, &args))
    return false;

  base::CommandLine current_args(base::CommandLine::FromString(
      base::StringPrintf(L"unused_program %ls", args.c_str())));
  const char* const kept_switches[] = {
      switches::kApp,
      switches::kAppId,
      switches::kProfileDirectory,
  };
  base::CommandLine desired_args(base::CommandLine::NO_PROGRAM);
  desired_args.CopySwitchesFrom(current_args, kept_switches,
                                base::size(kept_switches));
  if (desired_args.argv().size() == current_args.argv().size())
    return true;
  if (shortcuts)
    shortcuts->push_back(std::make_pair(shortcut_path, args));
  if (!do_removal)
    return true;
  base::win::ShortcutProperties updated_properties;
  updated_properties.set_arguments(desired_args.GetArgumentsString());
  return base::win::CreateOrUpdateShortcutLink(
      shortcut_path, updated_properties, base::win::SHORTCUT_UPDATE_EXISTING);
}

bool ShortcutOpResetAttributes(const base::FilePath& file_path) {
  const DWORD kAllowedAttributes =
      FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_REPARSE_POINT;
  DWORD attributes = ::GetFileAttributes(file_path.value().c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES)
    return false;
  if ((attributes & (~kAllowedAttributes)) == 0)
    return true;
  return ::SetFileAttributes(file_path.value().c_str(),
                             attributes & kAllowedAttributes);
}

// {|location|, |level|} determine |shortcut_folder|.
// For each shortcut in |shortcut_folder| that match |shortcut_filter|, apply
// |shortcut_operation|. Returns true if all operations are successful.
// All intended operations are attempted, even if failures occur.
// This method will abort and return false if |cancel| is non-nullptr and gets
// set at any point during this call.
bool BatchShortcutAction(
    const ShortcutFilterCallback& shortcut_filter,
    const ShortcutOperationCallback& shortcut_operation,
    ShellUtil::ShortcutLocation location,
    ShellUtil::ShellChange level,
    const scoped_refptr<ShellUtil::SharedCancellationFlag>& cancel) {
  DCHECK(!shortcut_operation.is_null());

  // There is no system-level Quick Launch shortcut folder.
  if (level == ShellUtil::SYSTEM_LEVEL &&
      location == ShellUtil::SHORTCUT_LOCATION_QUICK_LAUNCH) {
    return true;
  }

  base::FilePath shortcut_folder;
  if (!ShellUtil::GetShortcutPath(location, level, &shortcut_folder)) {
    LOG(WARNING) << "Cannot find path at location " << location;
    return false;
  }

  bool success = true;
  base::FileEnumerator enumerator(shortcut_folder, false,
                                  base::FileEnumerator::FILES,
                                  base::string16(L"*") + installer::kLnkExt);
  base::FilePath target_path;
  base::string16 args;
  for (base::FilePath shortcut_path = enumerator.Next(); !shortcut_path.empty();
       shortcut_path = enumerator.Next()) {
    if (cancel.get() && cancel->data.IsSet())
      return false;
    if (base::win::ResolveShortcut(shortcut_path, &target_path, &args)) {
      if (shortcut_filter.Run(target_path, args) &&
          !shortcut_operation.Run(shortcut_path)) {
        success = false;
      }
    } else {
      LOG(ERROR) << "Cannot resolve shortcut at " << shortcut_path.value();
      success = false;
    }
  }
  return success;
}

// If the folder specified by {|location|, |level|} is empty, remove it.
// Otherwise do nothing. Returns true on success, including the vacuous case
// where no deletion occurred because directory is non-empty.
bool RemoveShortcutFolderIfEmpty(ShellUtil::ShortcutLocation location,
                                 ShellUtil::ShellChange level) {
  // Explicitly whitelist locations, since accidental calls can be very harmful.
  if (location !=
          ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED &&
      location != ShellUtil::SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR &&
      location != ShellUtil::SHORTCUT_LOCATION_APP_SHORTCUTS) {
    NOTREACHED();
    return false;
  }

  base::FilePath shortcut_folder;
  if (!ShellUtil::GetShortcutPath(location, level, &shortcut_folder)) {
    LOG(WARNING) << "Cannot find path at location " << location;
    return false;
  }
  if (base::IsDirectoryEmpty(shortcut_folder) &&
      !base::DeletePathRecursively(shortcut_folder)) {
    LOG(ERROR) << "Cannot remove folder " << shortcut_folder.value();
    return false;
  }
  return true;
}

// Return a shortened version of |component|. Cut in the middle to try
// to avoid losing the unique parts of |component| (which are usually
// at the beginning or end for things like usernames and paths).
base::string16 ShortenAppModelIdComponent(const base::string16& component,
                                          int desired_length) {
  return component.substr(0, desired_length / 2) +
         component.substr(component.length() - ((desired_length + 1) / 2));
}

}  // namespace

const wchar_t* ShellUtil::kRegDefaultIcon = L"\\DefaultIcon";
const wchar_t* ShellUtil::kRegShellPath = L"\\shell";
const wchar_t* ShellUtil::kRegShellOpen = L"\\shell\\open\\command";
const wchar_t* ShellUtil::kRegStartMenuInternet =
    L"Software\\Clients\\StartMenuInternet";
const wchar_t* ShellUtil::kRegClasses = L"Software\\Classes";
const wchar_t* ShellUtil::kRegRegisteredApplications =
    L"Software\\RegisteredApplications";
const wchar_t* ShellUtil::kRegVistaUrlPrefs =
    L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\"
    L"http\\UserChoice";
const wchar_t* ShellUtil::kAppPathsRegistryKey =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const wchar_t* ShellUtil::kAppPathsRegistryPathName = L"Path";

const wchar_t* ShellUtil::kDefaultFileAssociations[] = {
    L".htm", L".html", L".shtml", L".xht", L".xhtml", nullptr};
const wchar_t* ShellUtil::kPotentialFileAssociations[] = {
    L".htm", L".html",  L".pdf",  L".shtml", L".svg",
    L".xht", L".xhtml", L".webp", nullptr};
const wchar_t* ShellUtil::kBrowserProtocolAssociations[] = {L"ftp", L"http",
                                                            L"https", nullptr};
const wchar_t* ShellUtil::kPotentialProtocolAssociations[] = {
    L"ftp", L"http",  L"https", L"irc", L"mailto", L"mms",    L"news", L"nntp",
    L"sms", L"smsto", L"snews", L"tel", L"urn",    L"webcal", nullptr};
const wchar_t* ShellUtil::kRegUrlProtocol = L"URL Protocol";
const wchar_t* ShellUtil::kRegApplication = L"\\Application";
const wchar_t* ShellUtil::kRegAppUserModelId = L"AppUserModelId";
const wchar_t* ShellUtil::kRegApplicationDescription =
    L"ApplicationDescription";
const wchar_t* ShellUtil::kRegApplicationName = L"ApplicationName";
const wchar_t* ShellUtil::kRegApplicationIcon = L"ApplicationIcon";
const wchar_t* ShellUtil::kRegApplicationCompany = L"ApplicationCompany";
const wchar_t* ShellUtil::kRegExePath = L"\\.exe";
const wchar_t* ShellUtil::kRegVerbOpen = L"open";
const wchar_t* ShellUtil::kRegVerbOpenNewWindow = L"opennewwindow";
const wchar_t* ShellUtil::kRegVerbRun = L"run";
const wchar_t* ShellUtil::kRegCommand = L"command";
const wchar_t* ShellUtil::kRegDelegateExecute = L"DelegateExecute";
const wchar_t* ShellUtil::kRegOpenWithProgids = L"OpenWithProgids";

ShellUtil::ShortcutProperties::ShortcutProperties(ShellChange level_in)
    : level(level_in), icon_index(0), pin_to_taskbar(false), options(0U) {}

ShellUtil::ShortcutProperties::ShortcutProperties(
    const ShortcutProperties& other) = default;

ShellUtil::ShortcutProperties::~ShortcutProperties() {}

ShellUtil::FileAssociationsAndAppName::FileAssociationsAndAppName() = default;

ShellUtil::FileAssociationsAndAppName::FileAssociationsAndAppName(
    FileAssociationsAndAppName&& other) = default;

ShellUtil::FileAssociationsAndAppName::~FileAssociationsAndAppName() = default;

bool ShellUtil::QuickIsChromeRegisteredInHKLM(const base::FilePath& chrome_exe,
                                              const base::string16& suffix) {
  return QuickIsChromeRegistered(chrome_exe, suffix,
                                 CONFIRM_SHELL_REGISTRATION_IN_HKLM);
}

bool ShellUtil::ShortcutLocationIsSupported(ShortcutLocation location) {
  switch (location) {
    case SHORTCUT_LOCATION_DESKTOP:                           // Falls through.
    case SHORTCUT_LOCATION_QUICK_LAUNCH:                      // Falls through.
    case SHORTCUT_LOCATION_START_MENU_ROOT:                   // Falls through.
    case SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED:  // Falls through.
    case SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR:        // Falls through.
    case SHORTCUT_LOCATION_STARTUP:
      return true;
    case SHORTCUT_LOCATION_TASKBAR_PINS:
      return base::win::GetVersion() >= base::win::Version::WIN7;
    case SHORTCUT_LOCATION_APP_SHORTCUTS:
      return base::win::GetVersion() >= base::win::Version::WIN8;
    default:
      NOTREACHED();
      return false;
  }
}

bool ShellUtil::GetShortcutPath(ShortcutLocation location,
                                ShellChange level,
                                base::FilePath* path) {
  DCHECK(path);
  int dir_key = -1;
  base::string16 folder_to_append;
  switch (location) {
    case SHORTCUT_LOCATION_DESKTOP:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_DESKTOP
                                        : base::DIR_COMMON_DESKTOP;
      break;
    case SHORTCUT_LOCATION_QUICK_LAUNCH:
      // There is no support for a system-level Quick Launch shortcut.
      DCHECK_EQ(level, CURRENT_USER);
      dir_key = base::DIR_USER_QUICK_LAUNCH;
      break;
    case SHORTCUT_LOCATION_START_MENU_ROOT:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU
                                        : base::DIR_COMMON_START_MENU;
      break;
    case SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU
                                        : base::DIR_COMMON_START_MENU;
      folder_to_append = InstallUtil::GetChromeShortcutDirNameDeprecated();
      break;
    case SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR:
      dir_key = (level == CURRENT_USER) ? base::DIR_START_MENU
                                        : base::DIR_COMMON_START_MENU;
      folder_to_append = InstallUtil::GetChromeAppsShortcutDirName();
      break;
    case SHORTCUT_LOCATION_TASKBAR_PINS:
      dir_key = base::DIR_TASKBAR_PINS;
      break;
    case SHORTCUT_LOCATION_APP_SHORTCUTS:
      // TODO(huangs): Move GetAppShortcutsFolder() logic into base_paths_win.
      return GetAppShortcutsFolder(level, path);
    case SHORTCUT_LOCATION_STARTUP:
      dir_key = (level == CURRENT_USER) ? base::DIR_USER_STARTUP
                                        : base::DIR_COMMON_STARTUP;
      break;

    default:
      NOTREACHED();
      return false;
  }

  if (!base::PathService::Get(dir_key, path) || path->empty()) {
    NOTREACHED() << dir_key;
    return false;
  }

  if (!folder_to_append.empty())
    *path = path->Append(folder_to_append);

  return true;
}

// Modifies a ShortcutProperties object by adding default values to
// uninitialized members. Tries to assign:
// - target: |target_exe|.
// - icon: from |target_exe|.
// - icon_index: the browser's icon index
// - app_id: the browser model id for the current install.
// - description: the browser's app description.
// static
void ShellUtil::AddDefaultShortcutProperties(const base::FilePath& target_exe,
                                             ShortcutProperties* properties) {
  if (!properties->has_target())
    properties->set_target(target_exe);

  if (!properties->has_icon())
    properties->set_icon(target_exe, install_static::GetIconResourceIndex());

  if (!properties->has_app_id()) {
    properties->set_app_id(
        GetBrowserModelId(!install_static::IsSystemInstall()));
  }

  if (!properties->has_description())
    properties->set_description(InstallUtil::GetAppDescription());
}

bool ShellUtil::MoveExistingShortcut(ShortcutLocation old_location,
                                     ShortcutLocation new_location,
                                     const ShortcutProperties& properties) {
  // Explicitly whitelist locations to which this is applicable.
  if (old_location != SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED ||
      new_location != SHORTCUT_LOCATION_START_MENU_ROOT) {
    NOTREACHED();
    return false;
  }

  base::string16 shortcut_name(ExtractShortcutNameFromProperties(properties));

  base::FilePath old_shortcut_path;
  base::FilePath new_shortcut_path;
  GetShortcutPath(old_location, properties.level, &old_shortcut_path);
  GetShortcutPath(new_location, properties.level, &new_shortcut_path);
  old_shortcut_path = old_shortcut_path.Append(shortcut_name);
  new_shortcut_path = new_shortcut_path.Append(shortcut_name);

  bool result = base::Move(old_shortcut_path, new_shortcut_path);
  RemoveShortcutFolderIfEmpty(old_location, properties.level);
  return result;
}

bool ShellUtil::CreateOrUpdateShortcut(ShortcutLocation location,
                                       const ShortcutProperties& properties,
                                       ShortcutOperation operation) {
  // Explicitly whitelist locations to which this is applicable.
  if (location != SHORTCUT_LOCATION_DESKTOP &&
      location != SHORTCUT_LOCATION_QUICK_LAUNCH &&
      location != SHORTCUT_LOCATION_START_MENU_ROOT &&
      location != SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED &&
      location != SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR) {
    NOTREACHED();
    return false;
  }

  // |pin_to_taskbar| is only acknowledged when first creating the shortcut.
  DCHECK(!properties.pin_to_taskbar ||
         operation == SHELL_SHORTCUT_CREATE_ALWAYS ||
         operation == SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL);

  base::FilePath user_shortcut_path;
  base::FilePath system_shortcut_path;
  if (location == SHORTCUT_LOCATION_QUICK_LAUNCH) {
    // There is no system-level shortcut for Quick Launch.
    DCHECK_EQ(properties.level, CURRENT_USER);
  } else if (!GetShortcutPath(location, SYSTEM_LEVEL, &system_shortcut_path)) {
    NOTREACHED();
    return false;
  }

  base::string16 shortcut_name(ExtractShortcutNameFromProperties(properties));
  system_shortcut_path = system_shortcut_path.Append(shortcut_name);

  base::FilePath* chosen_path;
  bool should_install_shortcut = true;
  if (properties.level == SYSTEM_LEVEL) {
    // Install the system-level shortcut if requested.
    chosen_path = &system_shortcut_path;
  } else if (operation != SHELL_SHORTCUT_CREATE_IF_NO_SYSTEM_LEVEL ||
             system_shortcut_path.empty() ||
             !base::PathExists(system_shortcut_path)) {
    // Otherwise install the user-level shortcut, unless the system-level
    // variant of this shortcut is present on the machine and |operation| states
    // not to create a user-level shortcut in that case.
    if (!GetShortcutPath(location, CURRENT_USER, &user_shortcut_path)) {
      NOTREACHED();
      return false;
    }
    user_shortcut_path = user_shortcut_path.Append(shortcut_name);
    chosen_path = &user_shortcut_path;
  } else {
    // Do not install any shortcut if we are told to install a user-level
    // shortcut, but the system-level variant of that shortcut is present.
    // Other actions (e.g., pinning) can still happen with respect to the
    // existing system-level shortcut however.
    chosen_path = &system_shortcut_path;
    should_install_shortcut = false;
  }

  if (chosen_path == nullptr || chosen_path->empty()) {
    NOTREACHED();
    return false;
  }

  base::win::ShortcutOperation shortcut_operation =
      TranslateShortcutOperation(operation);
  bool success = true;
  if (should_install_shortcut) {
    // Make sure the parent directories exist when creating the shortcut.
    if (shortcut_operation == base::win::SHORTCUT_CREATE_ALWAYS &&
        !base::CreateDirectory(chosen_path->DirName())) {
      NOTREACHED();
      return false;
    }

    base::win::ShortcutProperties shortcut_properties(
        TranslateShortcutProperties(properties));
    success = base::win::CreateOrUpdateShortcutLink(
        *chosen_path, shortcut_properties, shortcut_operation);
  }

  if (success && shortcut_operation == base::win::SHORTCUT_CREATE_ALWAYS &&
      properties.pin_to_taskbar && base::win::CanPinShortcutToTaskbar()) {
    bool pinned = base::win::PinShortcutToTaskbar(*chosen_path);
    LOG_IF(ERROR, !pinned) << "Failed to pin to taskbar "
                           << chosen_path->value();
  }

  return success;
}

base::string16 ShellUtil::FormatIconLocation(const base::FilePath& icon_path,
                                             int icon_index) {
  base::string16 icon_string(icon_path.value());
  icon_string.append(L",");
  icon_string.append(base::NumberToString16(icon_index));
  return icon_string;
}

base::string16 ShellUtil::GetChromeShellOpenCmd(
    const base::FilePath& chrome_exe) {
  return base::CommandLine(chrome_exe).GetCommandLineStringForShell();
}

base::string16 ShellUtil::GetChromeDelegateCommand(
    const base::FilePath& chrome_exe) {
  return L"\"" + chrome_exe.value() + L"\" -- %*";
}

void ShellUtil::GetRegisteredBrowsers(
    std::map<base::string16, base::string16>* browsers) {
  DCHECK(browsers);

  const base::string16 base_key(kRegStartMenuInternet);
  base::string16 client_path;
  RegKey key;
  base::string16 name;
  base::string16 command;

  // HKCU has precedence over HKLM for these registrations: http://goo.gl/xjczJ.
  // Look in HKCU second to override any identical values found in HKLM.
  const HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
  for (const HKEY root : roots) {
    for (base::win::RegistryKeyIterator iter(root, base_key.c_str());
         iter.Valid(); ++iter) {
      client_path.assign(base_key).append(1, L'\\').append(iter.Name());
      // Read the browser's name (localized according to install language).
      if (key.Open(root, client_path.c_str(), KEY_QUERY_VALUE) !=
              ERROR_SUCCESS ||
          key.ReadValue(nullptr, &name) != ERROR_SUCCESS || name.empty() ||
          name.find(install_static::GetBaseAppName()) != base::string16::npos) {
        continue;
      }
      // Read the browser's reinstall command.
      if (key.Open(root, (client_path + L"\\InstallInfo").c_str(),
                   KEY_QUERY_VALUE) == ERROR_SUCCESS &&
          key.ReadValue(kReinstallCommand, &command) == ERROR_SUCCESS &&
          !command.empty()) {
        (*browsers)[name] = command;
      }
    }
  }
}

base::string16 ShellUtil::GetCurrentInstallationSuffix(
    const base::FilePath& chrome_exe) {
  // This method is somewhat the opposite of GetInstallationSpecificSuffix().
  // In this case we are not trying to determine the current suffix for the
  // upcoming installation (i.e. not trying to stick to a currently bad
  // registration style if one is present).
  // Here we want to determine which suffix we should use at run-time.
  // In order of preference, we prefer (for user-level installs):
  //   1) Base 32 encoding of the md5 hash of the user's sid (new-style).
  //   2) Username (old-style).
  //   3) Unsuffixed (even worse).
  base::string16 tested_suffix;
  if (InstallUtil::IsPerUserInstall() &&
      (!GetUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegistered(chrome_exe, tested_suffix,
                                CONFIRM_PROGID_REGISTRATION)) &&
      (!GetOldUserSpecificRegistrySuffix(&tested_suffix) ||
       !QuickIsChromeRegistered(chrome_exe, tested_suffix,
                                CONFIRM_PROGID_REGISTRATION)) &&
      !QuickIsChromeRegistered(chrome_exe, tested_suffix.erase(),
                               CONFIRM_PROGID_REGISTRATION)) {
    // If Chrome is not registered under any of the possible suffixes (e.g.
    // tests, Canary, etc.): use the new-style suffix at run-time.
    if (!GetUserSpecificRegistrySuffix(&tested_suffix))
      NOTREACHED();
  }
  return tested_suffix;
}

base::string16 ShellUtil::GetBrowserModelId(bool is_per_user_install) {
  base::string16 app_id(install_static::GetBaseAppId());
  base::string16 suffix;

  // TODO(robertshield): Temporary hack to make the kRegisterChromeBrowserSuffix
  // apply to all registry values computed down in these murky depths.
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          installer::switches::kRegisterChromeBrowserSuffix)) {
    suffix = command_line.GetSwitchValueNative(
        installer::switches::kRegisterChromeBrowserSuffix);
  } else if (is_per_user_install && !GetUserSpecificRegistrySuffix(&suffix)) {
    NOTREACHED();
  }
  app_id.append(suffix);
  if (app_id.length() <= installer::kMaxAppModelIdLength)
    return app_id;
  return ShortenAppModelIdComponent(app_id, installer::kMaxAppModelIdLength);
}

base::string16 ShellUtil::BuildAppUserModelId(
    const std::vector<base::string16>& components) {
  DCHECK_GT(components.size(), 0U);

  // Find the maximum numbers of characters allowed in each component
  // (accounting for the dots added between each component).
  const size_t available_chars =
      installer::kMaxAppModelIdLength - (components.size() - 1);
  const size_t max_component_length = available_chars / components.size();

  // |max_component_length| should be at least 2; otherwise the truncation logic
  // below breaks.
  if (max_component_length < 2U) {
    NOTREACHED();
    return (*components.begin()).substr(0, installer::kMaxAppModelIdLength);
  }

  base::string16 app_id;
  app_id.reserve(installer::kMaxAppModelIdLength);
  for (std::vector<base::string16>::const_iterator it = components.begin();
       it != components.end(); ++it) {
    if (it != components.begin())
      app_id.push_back(L'.');

    const base::string16& component = *it;
    DCHECK(!component.empty());
    if (component.length() > max_component_length) {
      app_id.append(
          ShortenAppModelIdComponent(component, max_component_length));
    } else {
      app_id.append(component);
    }
  }
  // No spaces are allowed in the AppUserModelId according to MSDN.
  base::ReplaceChars(app_id, base::ASCIIToUTF16(" "), base::ASCIIToUTF16("_"),
                     &app_id);
  return app_id;
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultState() {
  base::FilePath app_path;
  if (!base::PathService::Get(base::FILE_EXE, &app_path)) {
    NOTREACHED();
    return UNKNOWN_DEFAULT;
  }

  return GetChromeDefaultStateFromPath(app_path);
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultStateFromPath(
    const base::FilePath& chrome_exe) {
  // When we check for default browser we don't necessarily want to count file
  // type handlers and icons as having changed the default browser status,
  // since the user may have changed their shell settings to cause HTML files
  // to open with a text editor for example. We also don't want to aggressively
  // claim FTP, since the user may have a separate FTP client. It is an open
  // question as to how to "heal" these settings. Perhaps the user should just
  // re-run the installer or run with the --set-default-browser command line
  // flag. There is doubtless some other key we can hook into to cause "Repair"
  // to show up in Add/Remove programs for us.
  static const wchar_t* const kChromeProtocols[] = {L"http", L"https"};
  DefaultState default_state = ProbeProtocolHandlers(
      chrome_exe, kChromeProtocols, base::size(kChromeProtocols));
  UpdateDefaultBrowserBeaconWithState(default_state);
  return default_state;
}

ShellUtil::DefaultState ShellUtil::GetChromeDefaultProtocolClientState(
    const base::string16& protocol) {
  if (protocol.empty())
    return UNKNOWN_DEFAULT;

  base::FilePath chrome_exe;
  if (!base::PathService::Get(base::FILE_EXE, &chrome_exe)) {
    NOTREACHED();
    return UNKNOWN_DEFAULT;
  }

  const wchar_t* const protocols[] = {protocol.c_str()};
  return ProbeProtocolHandlers(chrome_exe, protocols, base::size(protocols));
}

// static
bool ShellUtil::CanMakeChromeDefaultUnattended() {
  return base::win::GetVersion() < base::win::Version::WIN8;
}

// static
ShellUtil::InteractiveSetDefaultMode ShellUtil::GetInteractiveSetDefaultMode() {
  DCHECK(!CanMakeChromeDefaultUnattended());

  if (base::win::GetVersion() >= base::win::Version::WIN10)
    return InteractiveSetDefaultMode::SYSTEM_SETTINGS;

  return InteractiveSetDefaultMode::INTENT_PICKER;
}

bool ShellUtil::MakeChromeDefault(int shell_change,
                                  const base::FilePath& chrome_exe,
                                  bool elevate_if_not_admin) {
  DCHECK(!(shell_change & SYSTEM_LEVEL) || IsUserAnAdmin());

  if (!install_static::SupportsSetAsDefaultBrowser())
    return false;

  // Windows 8 does not permit making a browser default just like that.
  // This process needs to be routed through the system's UI. Use
  // ShowMakeChromeDefaultSystemUI instead (below).
  if (!CanMakeChromeDefaultUnattended()) {
    return false;
  }

  if (!RegisterChromeBrowser(chrome_exe, base::string16(),
                             elevate_if_not_admin)) {
    return false;
  }

  bool ret = true;
  // First use the new "recommended" way on Vista to make Chrome default
  // browser.
  base::string16 app_name = GetApplicationName(chrome_exe);

  // On Windows 7 we still can set ourselves via the the
  // IApplicationAssociationRegistration interface.
  VLOG(1) << "Registering Chrome as default browser on Windows 7.";
  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                  nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&pAAR));
  if (SUCCEEDED(hr)) {
    for (int i = 0; kBrowserProtocolAssociations[i] != nullptr; i++) {
      hr = pAAR->SetAppAsDefault(
          app_name.c_str(), kBrowserProtocolAssociations[i], AT_URLPROTOCOL);
      if (!SUCCEEDED(hr)) {
        ret = false;
        LOG(ERROR) << "Failed to register as default for protocol "
                   << kBrowserProtocolAssociations[i] << " (" << hr << ")";
      }
    }

    for (int i = 0; kDefaultFileAssociations[i] != nullptr; i++) {
      hr = pAAR->SetAppAsDefault(app_name.c_str(), kDefaultFileAssociations[i],
                                 AT_FILEEXTENSION);
      if (!SUCCEEDED(hr)) {
        ret = false;
        LOG(ERROR) << "Failed to register as default for file extension "
                   << kDefaultFileAssociations[i] << " (" << hr << ")";
      }
    }
  }

  if (!RegisterChromeAsDefaultXPStyle(shell_change, chrome_exe))
    ret = false;

  // Send Windows notification event so that it can update icons for
  // file associations.
  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  return ret;
}

// static
bool ShellUtil::LaunchUninstallAppsSettings() {
  DCHECK_GE(base::win::GetVersion(), base::win::Version::WIN10);

  static constexpr wchar_t kControlPanelAppModelId[] =
      L"windows.immersivecontrolpanel_cw5n1h2txyewy"
      L"!microsoft.windows.immersivecontrolpanel";

  Microsoft::WRL::ComPtr<IApplicationActivationManager> activator;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationActivationManager, nullptr,
                                  CLSCTX_ALL, IID_PPV_ARGS(&activator));
  if (FAILED(hr))
    return false;

  DWORD pid = 0;
  CoAllowSetForegroundWindow(activator.Get(), nullptr);
  hr = activator->ActivateApplication(
      kControlPanelAppModelId, L"page=SettingsPageAppsSizes", AO_NONE, &pid);
  return SUCCEEDED(hr);
}

bool ShellUtil::ShowMakeChromeDefaultSystemUI(
    const base::FilePath& chrome_exe) {
  DCHECK(!CanMakeChromeDefaultUnattended());

  if (!install_static::SupportsSetAsDefaultBrowser())
    return false;

  if (!RegisterChromeBrowser(chrome_exe, base::string16(), true))
    return false;

  bool succeeded = true;
  bool is_default = (GetChromeDefaultState() == IS_DEFAULT);
  if (!is_default) {
    switch (GetInteractiveSetDefaultMode()) {
      case INTENT_PICKER: {
        // On Windows 8, you can't set yourself as the default handler
        // programmatically. In other words IApplicationAssociationRegistration
        // has been rendered useless. What you can do is to launch
        // "Set Program Associations" section of the "Default Programs"
        // control panel, which is a mess, or pop the concise "How you want to
        // open webpages?" dialog.  We choose the latter.
        ScopedUserProtocolEntry user_protocol_entry(L"http");
        succeeded = LaunchSelectDefaultProtocolHandlerDialog(L"http");
      } break;
      case SYSTEM_SETTINGS:
        // On Windows 10, you can't even launch the associations dialog.
        // So we launch the settings dialog. Quoting from MSDN: "The Open With
        // dialog box can no longer be used to change the default program used
        // to open a file extension. You can only use SHOpenWithDialog to open
        // a single file."
        succeeded = LaunchDefaultAppsSettingsModernDialog(L"http");
        break;
    }
    is_default = (succeeded && GetChromeDefaultState() == IS_DEFAULT);
  }
  if (succeeded && is_default)
    RegisterChromeAsDefaultXPStyle(CURRENT_USER, chrome_exe);
  return succeeded;
}

bool ShellUtil::MakeChromeDefaultProtocolClient(
    const base::FilePath& chrome_exe,
    const base::string16& protocol) {
  if (!install_static::SupportsSetAsDefaultBrowser())
    return false;

  if (!RegisterChromeForProtocol(chrome_exe, base::string16(), protocol, true))
    return false;

  // Windows 8 does not permit making a browser default just like that.
  // This process needs to be routed through the system's UI. Use
  // ShowMakeChromeDefaultProtocolClientSystemUI instead (below).
  if (!CanMakeChromeDefaultUnattended())
    return false;

  bool ret = true;
  // First use the "recommended" way introduced in Vista to make Chrome default
  // protocol handler.
  VLOG(1) << "Registering Chrome as default handler for " << protocol
          << " on Windows 7.";
  Microsoft::WRL::ComPtr<IApplicationAssociationRegistration> pAAR;
  HRESULT hr = ::CoCreateInstance(CLSID_ApplicationAssociationRegistration,
                                  nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&pAAR));
  if (SUCCEEDED(hr)) {
    base::string16 app_name = GetApplicationName(chrome_exe);
    hr = pAAR->SetAppAsDefault(app_name.c_str(), protocol.c_str(),
                               AT_URLPROTOCOL);
  }
  if (!SUCCEEDED(hr)) {
    ret = false;
    LOG(ERROR) << "Could not make Chrome default protocol client (Windows 7):"
               << " HRESULT=" << hr << ".";
  }

  // Now use the old way to associate Chrome with the desired protocol. This
  // should not be required on Vista+, but since some applications still read
  // Software\Classes\<protocol> key directly, do this on Vista+ also.
  if (!RegisterChromeAsDefaultProtocolClientXPStyle(chrome_exe, protocol))
    ret = false;

  return ret;
}

bool ShellUtil::ShowMakeChromeDefaultProtocolClientSystemUI(
    const base::FilePath& chrome_exe,
    const base::string16& protocol) {
  DCHECK(!CanMakeChromeDefaultUnattended());

  if (!install_static::SupportsSetAsDefaultBrowser())
    return false;

  if (!RegisterChromeForProtocol(chrome_exe, base::string16(), protocol, true))
    return false;

  bool succeeded = true;
  bool is_default =
      (GetChromeDefaultProtocolClientState(protocol) == IS_DEFAULT);
  if (!is_default) {
    switch (GetInteractiveSetDefaultMode()) {
      case INTENT_PICKER: {
        // On Windows 8, you can't set yourself as the default handler
        // programmatically. In other words IApplicationAssociationRegistration
        // has been rendered useless. What you can do is to launch
        // "Set Program Associations" section of the "Default Programs"
        // control panel, which is a mess, or pop the concise "How you want to
        // open
        // links of this type (protocol)?" dialog.  We choose the latter.
        ScopedUserProtocolEntry user_protocol_entry(protocol.c_str());
        succeeded = LaunchSelectDefaultProtocolHandlerDialog(protocol.c_str());
      } break;
      case SYSTEM_SETTINGS:
        // On Windows 10, you can't even launch the associations dialog.
        // So we launch the settings dialog.
        succeeded = LaunchDefaultAppsSettingsModernDialog(protocol.c_str());
        break;
    }
    is_default = (succeeded &&
                  GetChromeDefaultProtocolClientState(protocol) == IS_DEFAULT);
  }
  if (succeeded && is_default)
    RegisterChromeAsDefaultProtocolClientXPStyle(chrome_exe, protocol);
  return succeeded;
}

bool ShellUtil::RegisterChromeBrowser(const base::FilePath& chrome_exe,
                                      const base::string16& unique_suffix,
                                      bool elevate_if_not_admin) {
  base::CommandLine& command_line = *base::CommandLine::ForCurrentProcess();

  base::string16 suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (command_line.HasSwitch(
                 installer::switches::kRegisterChromeBrowserSuffix)) {
    suffix = command_line.GetSwitchValueNative(
        installer::switches::kRegisterChromeBrowserSuffix);
  } else if (!GetInstallationSpecificSuffix(chrome_exe, &suffix)) {
    return false;
  }

  RemoveRunVerbOnWindows8();

  bool user_level = InstallUtil::IsPerUserInstall();
  HKEY root = DetermineRegistrationRoot(user_level);

  // Look only in HKLM for system-level installs (otherwise, if a user-level
  // install is also present, it will lead IsChromeRegistered() to think this
  // system-level install isn't registered properly as it is shadowed by the
  // user-level install's registrations).
  uint32_t look_for_in = user_level ? RegistryEntry::LOOK_IN_HKCU_THEN_HKLM
                                    : RegistryEntry::LOOK_IN_HKLM;

  // Check if chrome is already registered with this suffix.
  if (IsChromeRegistered(chrome_exe, suffix, look_for_in))
    return true;

  // Ensure that the shell is notified of the mutations below. Specific exit
  // points may disable this if no mutations are made.
  base::ScopedClosureRunner notify_on_exit(base::BindOnce([] {
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
  }));

  // Do the full registration at user-level or if the user is an admin.
  if (root == HKEY_CURRENT_USER || IsUserAnAdmin()) {
    std::vector<std::unique_ptr<RegistryEntry>> progid_and_appreg_entries;
    std::vector<std::unique_ptr<RegistryEntry>> shell_entries;
    GetChromeProgIdEntries(chrome_exe, suffix, &progid_and_appreg_entries);
    GetChromeAppRegistrationEntries(chrome_exe, suffix,
                                    &progid_and_appreg_entries);
    GetShellIntegrationEntries(chrome_exe, suffix, &shell_entries);
    return AddRegistryEntries(root, progid_and_appreg_entries) &&
           AddRegistryEntries(root, shell_entries);
  }
  // The installer is responsible for registration for system-level installs, so
  // never try to do it here. Getting to this point for a system-level install
  // likely means that IsChromeRegistered thinks registration is broken due to
  // localization issues (see https://crbug.com/717913#c18). It likely is not,
  // so return success to allow Chrome to be made default.
  if (!user_level) {
    notify_on_exit.Release().Reset();
    return true;
  }
  // Try to elevate and register if requested for per-user installs if the user
  // is not an admin.
  if (elevate_if_not_admin &&
      ElevateAndRegisterChrome(chrome_exe, suffix, base::string16())) {
    return true;
  }
  // If we got to this point then all we can do is create ProgId and basic app
  // registrations under HKCU.
  std::vector<std::unique_ptr<RegistryEntry>> entries;
  GetChromeProgIdEntries(chrome_exe, base::string16(), &entries);
  // Prefer to use |suffix|; unless Chrome's ProgIds are already registered with
  // no suffix (as per the old registration style): in which case some other
  // registry entries could refer to them and since we were not able to set our
  // HKLM entries above, we are better off not altering these here.
  if (!AreEntriesAsDesired(entries, RegistryEntry::LOOK_IN_HKCU)) {
    if (!suffix.empty()) {
      entries.clear();
      GetChromeProgIdEntries(chrome_exe, suffix, &entries);
      GetChromeAppRegistrationEntries(chrome_exe, suffix, &entries);
    }
    return AddRegistryEntries(HKEY_CURRENT_USER, entries);
  }
  // The ProgId is registered unsuffixed in HKCU, also register the app with
  // Windows in HKCU (this was not done in the old registration style and thus
  // needs to be done after the above check for the unsuffixed registration).
  entries.clear();
  GetChromeAppRegistrationEntries(chrome_exe, base::string16(), &entries);
  return AddRegistryEntries(HKEY_CURRENT_USER, entries);
}

bool ShellUtil::RegisterChromeForProtocol(const base::FilePath& chrome_exe,
                                          const base::string16& unique_suffix,
                                          const base::string16& protocol,
                                          bool elevate_if_not_admin) {
  base::string16 suffix;
  if (!unique_suffix.empty()) {
    suffix = unique_suffix;
  } else if (!GetInstallationSpecificSuffix(chrome_exe, &suffix)) {
    return false;
  }

  bool user_level = InstallUtil::IsPerUserInstall();
  HKEY root = DetermineRegistrationRoot(user_level);

  // Look only in HKLM for system-level installs (otherwise, if a user-level
  // install is also present, it could lead IsChromeRegisteredForProtocol() to
  // think this system-level install isn't registered properly as it may be
  // shadowed by the user-level install's registrations).
  uint32_t look_for_in = user_level ? RegistryEntry::LOOK_IN_HKCU_THEN_HKLM
                                    : RegistryEntry::LOOK_IN_HKLM;

  // Check if chrome is already registered with this suffix.
  if (IsChromeRegisteredForProtocol(suffix, protocol, look_for_in))
    return true;

  if (root == HKEY_CURRENT_USER || IsUserAnAdmin()) {
    // We can do this operation directly.
    // First, make sure Chrome is fully registered on this machine.
    if (!RegisterChromeBrowser(chrome_exe, suffix, false))
      return false;

    // Write in the capability for the protocol.
    std::vector<std::unique_ptr<RegistryEntry>> entries;
    GetProtocolCapabilityEntries(suffix, protocol, &entries);
    return AddRegistryEntries(root, entries);
  } else if (elevate_if_not_admin) {
    // Elevate to do the whole job
    return ElevateAndRegisterChrome(chrome_exe, suffix, protocol);
  } else {
    // Admin rights are required to register capabilities before Windows 8.
    return false;
  }
}

// static
bool ShellUtil::RemoveShortcuts(ShortcutLocation location,
                                ShellChange level,
                                const base::FilePath& target_exe) {
  if (!ShortcutLocationIsSupported(location))
    return true;  // Vacuous success.

  FilterTargetEq shortcut_filter(target_exe, false);
  // Main operation to apply to each shortcut in the directory specified.
  ShortcutOperationCallback shortcut_operation =
      location == SHORTCUT_LOCATION_TASKBAR_PINS
          ? base::BindRepeating(&ShortcutOpUnpinFromTaskbar)
          : base::BindRepeating(&ShortcutOpDelete);
  bool success =
      BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                          shortcut_operation, location, level, nullptr);
  // Remove chrome-specific shortcut folders if they are now empty.
  if (success &&
      (location == SHORTCUT_LOCATION_START_MENU_CHROME_DIR_DEPRECATED ||
       location == SHORTCUT_LOCATION_START_MENU_CHROME_APPS_DIR ||
       location == SHORTCUT_LOCATION_APP_SHORTCUTS)) {
    success = RemoveShortcutFolderIfEmpty(location, level);
  }
  return success;
}

// static
bool ShellUtil::RetargetShortcutsWithArgs(
    ShortcutLocation location,
    ShellChange level,
    const base::FilePath& old_target_exe,
    const base::FilePath& new_target_exe) {
  if (!ShortcutLocationIsSupported(location))
    return true;  // Vacuous success.

  FilterTargetEq shortcut_filter(old_target_exe, true);
  ShortcutOperationCallback shortcut_operation =
      base::BindRepeating(&ShortcutOpRetarget, old_target_exe, new_target_exe);
  return BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                             shortcut_operation, location, level, nullptr);
}

// static
bool ShellUtil::ShortcutListMaybeRemoveUnknownArgs(
    ShortcutLocation location,
    ShellChange level,
    const base::FilePath& chrome_exe,
    bool do_removal,
    const scoped_refptr<SharedCancellationFlag>& cancel,
    std::vector<std::pair<base::FilePath, base::string16>>* shortcuts) {
  if (!ShortcutLocationIsSupported(location))
    return false;
  FilterTargetEq shortcut_filter(chrome_exe, true);
  ShortcutOperationCallback shortcut_operation = base::BindRepeating(
      &ShortcutOpListOrRemoveUnknownArgs, do_removal, shortcuts);
  return BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                             shortcut_operation, location, level, cancel);
}

// static
bool ShellUtil::ResetShortcutFileAttributes(ShortcutLocation location,
                                            ShellChange level,
                                            const base::FilePath& chrome_exe) {
  if (!ShortcutLocationIsSupported(location))
    return false;
  FilterTargetEq shortcut_filter(chrome_exe, /*require_args=*/false);
  ShortcutOperationCallback shortcut_operation =
      base::BindRepeating(&ShortcutOpResetAttributes);
  return BatchShortcutAction(shortcut_filter.AsShortcutFilterCallback(),
                             shortcut_operation, location, level, nullptr);
}

bool ShellUtil::GetUserSpecificRegistrySuffix(base::string16* suffix) {
  // Use a thread-safe cache for the user's suffix.
  static base::LazyInstance<UserSpecificRegistrySuffix>::Leaky suffix_instance =
      LAZY_INSTANCE_INITIALIZER;
  return suffix_instance.Get().GetSuffix(suffix);
}

bool ShellUtil::GetOldUserSpecificRegistrySuffix(base::string16* suffix) {
  wchar_t user_name[256];
  DWORD size = base::size(user_name);
  if (::GetUserName(user_name, &size) == 0 || size < 1) {
    NOTREACHED();
    return false;
  }
  suffix->reserve(size);
  suffix->assign(1, L'.');
  suffix->append(user_name, size - 1);
  return true;
}

// static
bool ShellUtil::AddFileAssociations(
    const base::string16& prog_id,
    const base::CommandLine& command_line,
    const base::string16& application_name,
    const base::string16& file_type_name,
    const base::FilePath& icon_path,
    const std::set<base::string16>& file_extensions) {
  std::vector<std::unique_ptr<RegistryEntry>> entries;

  // Create a class for this app.
  ApplicationInfo app_info;
  app_info.prog_id = prog_id;
  app_info.application_name = application_name;
  app_info.application_icon_path = icon_path;
  app_info.application_icon_index = 0;
  app_info.file_type_name = file_type_name;
  app_info.file_type_icon_path = icon_path;
  app_info.file_type_icon_index = 0;
  app_info.command_line = command_line.GetCommandLineStringForShell();

  GetProgIdEntries(app_info, &entries);

  std::vector<base::string16> handled_file_extensions;

  // Associate each extension that the app can handle with the class. Set this
  // app as the default handler if and only if there is no existing default.
  for (const auto& file_extension : file_extensions) {
    // Do not allow empty file extensions, or extensions beginning with a '.'.
    DCHECK(!file_extension.empty());
    DCHECK_NE(L'.', file_extension[0]);
    base::string16 ext(1, L'.');
    ext.append(file_extension);
    GetAppExtRegistrationEntries(prog_id, ext, &entries);

    // Registering as the default will have no effect on Windows 8 (see
    // documentation for GetAppDefaultRegistrationEntries). However, if our app
    // is the only handler, it will automatically become the default, so the
    // same effect is achieved.
    GetAppDefaultRegistrationEntries(prog_id, ext, false, &entries);

    handled_file_extensions.push_back(std::move(ext));
  }

  // Save handled file extensions in the registry for use during uninstallation.
  base::string16 prog_id_path(ShellUtil::kRegClasses);
  prog_id_path.push_back(base::FilePath::kSeparators[0]);
  prog_id_path.append(prog_id);
  entries.push_back(std::make_unique<RegistryEntry>(
      prog_id_path, L"FileExtensions",
      base::JoinString(handled_file_extensions, L";")));

  return AddRegistryEntries(HKEY_CURRENT_USER, entries);
}

// static
bool ShellUtil::DeleteFileAssociations(const base::string16& prog_id) {
  base::string16 prog_id_path(kRegClasses);
  prog_id_path.push_back(base::FilePath::kSeparators[0]);
  prog_id_path.append(prog_id);

  // Get list of handled file extensions from value FileExtensions at
  // HKEY_CURRENT_USER\Software\Classes\|prog_id|.
  RegKey file_extensions_key(HKEY_CURRENT_USER, prog_id_path.c_str(),
                             KEY_QUERY_VALUE);
  base::string16 handled_file_extensions;
  if (file_extensions_key.ReadValue(
          L"FileExtensions", &handled_file_extensions) == ERROR_SUCCESS) {
    std::vector<base::string16> file_extensions =
        base::SplitString(handled_file_extensions, base::string16(L";"),
                          base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // Delete file-extension-handling registry entries for each file extension.
    for (const auto& file_extension : file_extensions) {
      base::string16 extension_path(kRegClasses);
      extension_path.push_back(base::FilePath::kSeparators[0]);
      extension_path.append(file_extension);

      // Delete the default value at
      // HKEY_CURRENT_USER\Software\Classes\.<extension> if set to |prog_id|;
      // this unregisters |prog_id| as the default handler for |file_extension|.
      InstallUtil::DeleteRegistryValueIf(
          HKEY_CURRENT_USER, extension_path.c_str(), WorkItem::kWow64Default,
          L"", InstallUtil::ValueEquals(prog_id));

      // Delete value |prog_id| at
      // HKEY_CURRENT_USER\Software\Classes\.<extension>\OpenWithProgids;
      // this removes |prog_id| from the list of handlers for |file_extension|.
      extension_path.push_back(base::FilePath::kSeparators[0]);
      extension_path.append(ShellUtil::kRegOpenWithProgids);
      InstallUtil::DeleteRegistryValue(HKEY_CURRENT_USER, extension_path,
                                       WorkItem::kWow64Default, prog_id);
    }
  }

  // Delete the key HKEY_CURRENT_USER\Software\Classes\|prog_id|.
  return InstallUtil::DeleteRegistryKey(HKEY_CURRENT_USER, prog_id_path,
                                        WorkItem::kWow64Default);
}

// static
ShellUtil::FileAssociationsAndAppName ShellUtil::GetFileAssociationsAndAppName(
    const base::string16& prog_id) {
  FileAssociationsAndAppName file_associations_and_app_name;

  // Get list of handled file extensions from value FileExtensions at
  // HKEY_CURRENT_USER\Software\Classes\|prog_id|.
  base::string16 prog_id_path(kRegClasses);
  prog_id_path.push_back(base::FilePath::kSeparators[0]);
  prog_id_path.append(prog_id);
  RegKey file_extensions_key(HKEY_CURRENT_USER, prog_id_path.c_str(),
                             KEY_QUERY_VALUE);
  base::string16 handled_file_extensions;
  if (file_extensions_key.ReadValue(
          L"FileExtensions", &handled_file_extensions) != ERROR_SUCCESS) {
    return FileAssociationsAndAppName();
  }
  std::vector<base::StringPiece16> file_associations_vec =
      base::SplitStringPiece(base::StringPiece16(handled_file_extensions),
                             base::StringPiece16(L";"), base::TRIM_WHITESPACE,
                             base::SPLIT_WANT_NONEMPTY);
  for (const auto& file_extension : file_associations_vec) {
    // Skip over the leading '.' so that we return the same
    // extensions as were passed to AddFileAssociations.
    file_associations_and_app_name.file_associations.emplace(
        file_extension.substr(1));
  }
  prog_id_path.append(kRegApplication);
  RegKey prog_id_key(HKEY_CURRENT_USER, prog_id_path.c_str(), KEY_QUERY_VALUE);
  if (prog_id_key.ReadValue(kRegApplicationName,
                            &file_associations_and_app_name.app_name) !=
      ERROR_SUCCESS) {
    return FileAssociationsAndAppName();
  }
  return file_associations_and_app_name;
}

// static
base::FilePath ShellUtil::GetApplicationPathForProgId(
    const base::string16& prog_id) {
  base::string16 prog_id_path(kRegClasses);
  prog_id_path.push_back(base::FilePath::kSeparators[0]);
  prog_id_path.append(prog_id);
  prog_id_path.append(kRegShellOpen);
  base::string16 command_line;
  RegKey command_line_key(HKEY_CURRENT_USER, prog_id_path.c_str(),
                          KEY_QUERY_VALUE);
  if (command_line_key.ReadValue(L"", &command_line) == ERROR_SUCCESS)
    return base::CommandLine::FromString(command_line).GetProgram();

  return base::FilePath();
}

// static
bool ShellUtil::AddRegistryEntries(
    HKEY root,
    const std::vector<std::unique_ptr<RegistryEntry>>& entries) {
  std::unique_ptr<WorkItemList> items(WorkItem::CreateWorkItemList());

  for (const auto& entry : entries)
    entry->AddToWorkItemList(root, items.get());

  // Apply all the registry changes and if there is a problem, rollback
  if (!items->Do()) {
    items->Rollback();
    return false;
  }
  return true;
}
