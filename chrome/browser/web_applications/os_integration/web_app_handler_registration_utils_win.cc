// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/hash/hash.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/filename_util.h"

namespace web_app {

namespace {

// UMA metric name for file handler registration result.
constexpr const char* kRegistrationResultMetric =
    "Apps.FileHandler.Registration.Win.Result";

// Returns true if the app with id |app_id| is currently installed in one or
// more profiles, excluding |cur_profile_path|, and has its web_app launcher
// registered with Windows as a handler for the associations it supports.
// Sets |only_profile_with_app_installed| to the path of profile that is the
// only profile with the app installed, an empty path otherwise. If the app is
// only installed in exactly one other profile, it will need its app name
// updated.
bool IsWebAppLauncherRegisteredWithWindows(
    const webapps::AppId& app_id,
    const base::FilePath& cur_profile_path,
    base::FilePath* only_profile_with_app_installed) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* storage = &profile_manager->GetProfileAttributesStorage();

  bool found_app = false;
  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (profile_path == cur_profile_path)
      continue;
    std::wstring profile_prog_id = GetProgIdForApp(profile_path, app_id);
    base::FilePath shim_app_path =
        ShellUtil::GetApplicationPathForProgId(profile_prog_id);
    if (shim_app_path.empty())
      continue;
    *only_profile_with_app_installed =
        found_app ? base::FilePath() : profile_path;
    found_app = true;
    if (only_profile_with_app_installed->empty())
      break;
  }
  return found_app;
}

// Construct a string that is used to specify which profile a web
// app is installed for. The string is of the form "( <profile name>)".
std::wstring GetAppNameExtensionForProfile(const base::FilePath& profile_path) {
  std::wstring app_name_extension;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile_path);
  if (entry) {
    app_name_extension.append(L" (");
    app_name_extension.append(base::AsWString(entry->GetLocalProfileName()));
    app_name_extension.append(L")");
  }
  return app_name_extension;
}

Result UpdateAppRegistration(const webapps::AppId& app_id,
                             const std::wstring& app_name,
                             const base::FilePath& profile_path,
                             const std::wstring& prog_id,
                             const std::wstring& app_name_extension) {
  if (!base::DeleteFile(ShellUtil::GetApplicationPathForProgId(prog_id))) {
    RecordRegistration(RegistrationResult::kFailToDeleteExistingRegistration);
    return Result::kError;
  }

  std::wstring user_visible_app_name(app_name);
  user_visible_app_name.append(app_name_extension);

  base::FilePath web_app_path(
      GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id, GURL()));
  std::optional<base::FilePath> app_launcher_path =
      CreateAppLauncherFile(app_name, app_name_extension, web_app_path);
  if (!app_launcher_path)
    return Result::kError;

  base::CommandLine app_launch_cmd =
      GetAppLauncherCommand(app_id, app_launcher_path.value(), profile_path);
  base::FilePath icon_path =
      internals::GetIconFilePath(web_app_path, base::AsString16(app_name));
  ShellUtil::AddApplicationClass(prog_id, app_launch_cmd, user_visible_app_name,
                                 app_name, icon_path);
  // Retrieve the file handler ProgIds and register the new app name for each of
  // them.
  const std::vector<std::wstring> file_handler_prog_ids =
      ShellUtil::GetFileHandlerProgIdsForAppId(prog_id);
  for (const auto& file_handler_prog_id : file_handler_prog_ids) {
    ShellUtil::AddApplicationClass(file_handler_prog_id, app_launch_cmd,
                                   user_visible_app_name, app_name, icon_path);
  }
  return Result::kOk;
}

bool AppNameHasProfileExtension(const std::wstring& app_name,
                                const base::FilePath& profile_path) {
  std::wstring app_name_extension = GetAppNameExtensionForProfile(profile_path);

  return base::EndsWith(app_name, app_name_extension,
                        base::CompareCase::SENSITIVE) &&
         app_name.size() > app_name_extension.size();
}

// NOTE: prog_ids generated by this function are written to users' Windows
// registries. Changes to how they are generated will be disruptive to
// previously written values, and should therefore be avoided if possible.
std::wstring GetProgId(const base::FilePath& profile_path,
                       const webapps::AppId& app_id,
                       const std::optional<std::set<std::string>>&
                           file_extensions = std::nullopt) {
  // On system-level Win7 installs of the browser we need a user-specific part
  // to differentiate HKLM entries from different Windows profiles.
  std::wstring user_specific_part;
  ShellUtil::GetUserSpecificRegistrySuffix(&user_specific_part);

  std::string string_to_hash =
      base::StrCat({base::WideToUTF8(profile_path.BaseName().value()), app_id,
                    base::WideToUTF8(user_specific_part)});
  if (file_extensions && !file_extensions->empty()) {
    auto iter = file_extensions->begin();
    // Provided file extensions must have a leading period. This is enforced
    // to ensure that calls to this function have consistent syntax (and
    // therefore get the same prog_id for a given set of inputs).
    DCHECK_EQ(iter->at(0), '.');
    string_to_hash += *iter;
    while (++iter != file_extensions->end()) {
      DCHECK_EQ(iter->at(0), '.');
      string_to_hash += ";";
      string_to_hash += *iter;
    }
  }
  const uint32_t hash = base::PersistentHash(string_to_hash);

  return base::UTF16ToWide(
      base::StrCat({base::AsStringPiece16(install_static::GetBaseAppId()), u".",
                    base::NumberToString16(hash)}));
}

}  // namespace

base::CommandLine GetAppLauncherCommand(const webapps::AppId& app_id,
                                        const base::FilePath& app_launcher_path,
                                        const base::FilePath& profile_path) {
  base::CommandLine app_launcher_command(app_launcher_path);
  shell_integration::AppendProfileArgs(profile_path, &app_launcher_command);
  app_launcher_command.AppendSwitchASCII(switches::kAppId, app_id);
  return app_launcher_command;
}

std::wstring GetAppNameExtensionForNextInstall(
    const webapps::AppId& app_id,
    const base::FilePath& profile_path) {
  // Return a profile-specific app name extension only if duplicate |app_id|
  // installations exist in other profiles.
  base::FilePath only_profile_with_app_installed;
  if (IsWebAppLauncherRegisteredWithWindows(app_id, profile_path,
                                            &only_profile_with_app_installed)) {
    return GetAppNameExtensionForProfile(profile_path);
  }

  return std::wstring();
}

base::FilePath GetAppSpecificLauncherFilename(const std::wstring& app_name) {
  // Remove any characters that are illegal in Windows filenames.
  base::FilePath::StringType sanitized_app_name =
      internals::GetSanitizedFileName(base::AsString16(app_name)).value();

  // If |sanitized_app_name| is a reserved filename, prepend '_' to allow its
  // use as the launcher filename (e.g. "nul" => "_nul"). Prepending is
  // preferred over appending in order to handle filenames containing '.', as
  // Windows' logic for checking reserved filenames views characters after '.'
  // as file extensions, and only the pre-file-extension portion is checked for
  // legitimacy (e.g. "nul_" is allowed, but "nul.a_" is not).
  if (net::IsReservedNameOnWindows(sanitized_app_name))
    sanitized_app_name.insert(0, 1, FILE_PATH_LITERAL('_'));

  // Add .exe extension.
  return base::FilePath(sanitized_app_name)
      .AddExtension(FILE_PATH_LITERAL("exe"));
}

// See https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for
// the allowed characters in a prog_id. Since the prog_id is stored in the
// Windows registry, the mapping between a given profile+app_id and a prog_id
// can not be changed.
std::wstring GetProgIdForApp(const base::FilePath& profile_path,
                             const webapps::AppId& app_id) {
  return GetProgId(profile_path, app_id);
}

std::wstring GetProgIdForAppFileHandler(
    const base::FilePath& profile_path,
    const webapps::AppId& app_id,
    const std::set<std::string>& file_extensions) {
  return GetProgId(profile_path, app_id, file_extensions);
}

std::optional<base::FilePath> CreateAppLauncherFile(
    const std::wstring& app_name,
    const std::wstring& app_name_extension,
    const base::FilePath& web_app_path) {
  if (!base::CreateDirectory(web_app_path)) {
    DPLOG(ERROR) << "Unable to create web app dir";
    RecordRegistration(RegistrationResult::kFailToCopyFromGenericLauncher);
    return std::nullopt;
  }

  base::FilePath icon_path =
      internals::GetIconFilePath(web_app_path, base::AsString16(app_name));
  base::FilePath pwa_launcher_path = GetChromePwaLauncherPath();

  std::wstring user_visible_app_name(app_name);
  user_visible_app_name.append(app_name_extension);

  base::FilePath app_specific_launcher_path = web_app_path.Append(
      GetAppSpecificLauncherFilename(user_visible_app_name));

  // Create a hard link to the chrome pwa launcher app. Delete any pre-existing
  // version of the file first.
  base::DeleteFile(app_specific_launcher_path);
  if (!base::CreateWinHardLink(app_specific_launcher_path, pwa_launcher_path) &&
      !base::CopyFile(pwa_launcher_path, app_specific_launcher_path)) {
    DPLOG(ERROR) << "Unable to copy the generic PWA launcher."
                 << " pwa_launcher_path: " << pwa_launcher_path
                 << " app_specific_launcher_path: "
                 << app_specific_launcher_path;
    RecordRegistration(RegistrationResult::kFailToCopyFromGenericLauncher);
    return std::nullopt;
  }

  return app_specific_launcher_path;
}

void CheckAndUpdateExternalInstallations(const base::FilePath& cur_profile_path,
                                         const webapps::AppId& app_id,
                                         ResultCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::wstring prog_id = GetProgIdForApp(cur_profile_path, app_id);
  bool cur_profile_has_installation =
      !ShellUtil::GetApplicationPathForProgId(prog_id).empty();

  base::FilePath external_installation_profile_path;
  IsWebAppLauncherRegisteredWithWindows(app_id, cur_profile_path,
                                        &external_installation_profile_path);

  // Naming updates are only required if a single external installation exists.
  if (external_installation_profile_path.empty()) {
    // This enumeration signals if there was not an error. Exiting early here is
    // WAI, so this is a success.
    std::move(callback).Run(Result::kOk);
    return;
  }

  std::wstring external_installation_prog_id =
      GetProgIdForApp(external_installation_profile_path, app_id);
  std::wstring external_installation_name =
      ShellUtil::GetAppName(external_installation_prog_id);

  // Determine the updated name and extension for the external installation
  // based on the state of the installation in |cur_profile_path|.
  std::wstring updated_name;
  std::wstring updated_extension;
  if (cur_profile_has_installation) {
    // The single installation in a different profile should have a
    // profile-specific name.
    if (AppNameHasProfileExtension(external_installation_name,
                                   external_installation_profile_path)) {
      // This enumeration signals if there was not an error. Exiting early here
      // is WAI, so this is a success.
      std::move(callback).Run(Result::kOk);
      return;
    }

    updated_name = external_installation_name;
    updated_extension =
        GetAppNameExtensionForProfile(external_installation_profile_path);
  } else {
    // The single installation in a different profile should not have a
    // profile-specific name.
    if (!AppNameHasProfileExtension(external_installation_name,
                                    external_installation_profile_path)) {
      // This enumeration signals if there was not an error. Exiting early here
      // is WAI, so this is a success.
      std::move(callback).Run(Result::kOk);
      return;
    }

    // Remove the profile-specific extension from the external installation.
    std::wstring external_installation_extension =
        GetAppNameExtensionForProfile(external_installation_profile_path);
    updated_name = std::wstring(
        std::wstring_view(external_installation_name.c_str(),
                          external_installation_name.size() -
                              external_installation_extension.size()));
    updated_extension = std::wstring();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&UpdateAppRegistration, app_id, updated_name,
                     external_installation_profile_path,
                     external_installation_prog_id, updated_extension),
      std::move(callback));
}

// Record UMA metric for the result of file handler registration.
void RecordRegistration(RegistrationResult result) {
  UMA_HISTOGRAM_ENUMERATION(kRegistrationResultMetric, result);
}

}  // namespace web_app
