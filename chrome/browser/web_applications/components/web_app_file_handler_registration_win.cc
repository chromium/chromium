// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_file_handler_registration_win.h"

#include <iterator>
#include <set>
#include <string>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/browser/web_applications/components/web_app_file_handler_registration.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/shell_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "net/base/filename_util.h"

namespace {

// Returns true if the app with id |app_id| is currently installed in one or
// more profiles, excluding |curr_profile|, and has its web_app launcher
// registered with Windows as a handler for the file extensions it supports.
// Sets |only_profile_with_app_installed| to the path of profile that is the
// only profile with the app installed, an empty path otherwise. If the app is
// only installed in exactly one other profile, it will need its app name
// updated.
bool IsWebAppLauncherRegisteredWithWindows(
    const web_app::AppId& app_id,
    Profile* curr_profile,
    base::FilePath* only_profile_with_app_installed) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  auto* storage = &profile_manager->GetProfileAttributesStorage();

  bool found_app = false;
  std::vector<ProfileAttributesEntry*> entries =
      storage->GetAllProfilesAttributes();
  for (ProfileAttributesEntry* entry : entries) {
    base::FilePath profile_path = entry->GetPath();
    if (profile_path == curr_profile->GetPath())
      continue;
    base::string16 profile_prog_id =
        web_app::GetProgIdForApp(profile_path, app_id);
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

// UMA metric name for file handler registration result
constexpr const char* kRegistrationResultMetric =
    "Apps.FileHandler.Registration.Win.Result";

// Result of file handler registration process.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RegistrationResult {
  kSuccess = 0,
  kFailToCopyFromGenericLauncher = 1,
  kFailToAddFileAssociation = 2,
  kFailToDeleteExistingRegistration = 3,
  kFailToDeleteFileAssociationsForExistingRegistration = 4,
  kMaxValue = kFailToDeleteFileAssociationsForExistingRegistration
};

// Record UMA metric for the result of file handler registration.
void RecordRegistration(RegistrationResult result) {
  UMA_HISTOGRAM_ENUMERATION(kRegistrationResultMetric, result);
}

// Construct a string that is used to specify which profile a web
// app is installed for. The string is of the form "( <profile name>)".
base::string16 GetAppNameExtensionForProfile(
    const base::FilePath& profile_path) {
  base::string16 app_name_extension;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileAttributesStorage& storage =
      profile_manager->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry;
  bool has_entry = storage.GetProfileAttributesWithPath(profile_path, &entry);
  if (has_entry) {
    app_name_extension.append(STRING16_LITERAL(" ("));
    app_name_extension.append(entry->GetLocalProfileName());
    app_name_extension.append(STRING16_LITERAL(")"));
  }
  return app_name_extension;
}

}  // namespace

namespace web_app {

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

// Returns the app-specific-launcher filename to be used for |app_name|.
base::FilePath GetAppSpecificLauncherFilename(const base::string16& app_name) {
  // Remove any characters that are illegal in Windows filenames.
  base::FilePath::StringType sanitized_app_name =
      web_app::internals::GetSanitizedFileName(app_name).value();

  // On Windows 7, where the launcher has no file extension, replace any '.'
  // characters with '_' to prevent a portion of the filename from being
  // interpreted as its extension.
  const bool is_win_7 = base::win::GetVersion() == base::win::Version::WIN7;
  if (is_win_7) {
    base::ReplaceChars(sanitized_app_name, FILE_PATH_LITERAL("."),
                       FILE_PATH_LITERAL("_"), &sanitized_app_name);
  }

  // If |sanitized_app_name| is a reserved filename, prepend '_' to allow its
  // use as the launcher filename (e.g. "nul" => "_nul"). Prepending is
  // preferred over appending in order to handle filenames containing '.', as
  // Windows' logic for checking reserved filenames views characters after '.'
  // as file extensions, and only the pre-file-extension portion is checked for
  // legitimacy (e.g. "nul_" is allowed, but "nul.a_" is not).
  if (net::IsReservedNameOnWindows(sanitized_app_name))
    sanitized_app_name.insert(0, 1, FILE_PATH_LITERAL('_'));

  // On Windows 8+, add .exe extension. On Windows 7, where an app's display
  // name in the Open With menu can't be set programmatically, omit the
  // extension to use the launcher filename as the app's display name.
  if (!is_win_7) {
    return base::FilePath(sanitized_app_name)
        .AddExtension(FILE_PATH_LITERAL("exe"));
  }
  return base::FilePath(sanitized_app_name);
}

// See https://docs.microsoft.com/en-us/windows/win32/com/-progid--key for
// the allowed characters in a prog_id. Since the prog_id is stored in the
// Windows registry, the mapping between a given profile+app_id and a prog_id
// can not be changed.
base::string16 GetProgIdForApp(const base::FilePath& profile_path,
                               const AppId& app_id) {
  base::string16 prog_id = install_static::GetBaseAppId();
  std::string app_specific_part(
      base::UTF16ToUTF8(profile_path.BaseName().value()));
  app_specific_part.append(app_id);
  uint32_t hash = base::PersistentHash(app_specific_part);
  prog_id.push_back('.');
  prog_id.append(base::ASCIIToUTF16(base::NumberToString(hash)));
  return prog_id;
}

void RegisterFileHandlersWithOsTask(
    const AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path,
    const std::set<base::string16>& file_extensions,
    const base::string16& app_name_extension) {
  base::FilePath web_app_path =
      GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id, GURL());
  if (!base::CreateDirectory(web_app_path)) {
    DPLOG(ERROR) << "Unable to create web app dir";
    RecordRegistration(RegistrationResult::kFailToCopyFromGenericLauncher);
    return;
  }

  base::string16 utf16_app_name = base::UTF8ToUTF16(app_name);
  base::FilePath icon_path =
      internals::GetIconFilePath(web_app_path, utf16_app_name);
  base::FilePath pwa_launcher_path = GetChromePwaLauncherPath();
  base::string16 user_visible_app_name(utf16_app_name);
  // Specialize utf16_app_name to be per-profile, if there are other profiles
  // which have the web app installed. Also, migrate the registration
  // for the other profile(s), if needed.
  user_visible_app_name.append(app_name_extension);
  base::FilePath app_specific_launcher_path = web_app_path.Append(
      GetAppSpecificLauncherFilename(user_visible_app_name));

  // Create a hard link to the chrome pwa launcher app. Delete any pre-existing
  // version of the file first.
  base::DeleteFile(app_specific_launcher_path);
  if (!base::CreateWinHardLink(app_specific_launcher_path, pwa_launcher_path) &&
      !base::CopyFile(pwa_launcher_path, app_specific_launcher_path)) {
    DPLOG(ERROR) << "Unable to copy the generic PWA launcher";
    RecordRegistration(RegistrationResult::kFailToCopyFromGenericLauncher);
    return;
  }
  base::CommandLine app_specific_launcher_command(app_specific_launcher_path);
  app_specific_launcher_command.AppendSwitchPath(switches::kProfileDirectory,
                                                 profile_path.BaseName());
  app_specific_launcher_command.AppendSwitchASCII(switches::kAppId, app_id);
  bool result = ShellUtil::AddFileAssociations(
      GetProgIdForApp(profile_path, app_id), app_specific_launcher_command,
      user_visible_app_name, utf16_app_name + STRING16_LITERAL(" File"),
      icon_path, file_extensions);
  if (!result)
    RecordRegistration(RegistrationResult::kFailToAddFileAssociation);
  else
    RecordRegistration(RegistrationResult::kSuccess);
}

// Remove existing registration for an app, and reregister it. This is called
// when the app name changes.
void ReRegisterFileHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    const base::FilePath& profile_path,
    const std::set<base::string16>& file_extensions,
    const base::string16& prog_id,
    const base::string16& app_name_extension) {
  auto delete_file_callback = [](const base::FilePath& path) {
    if (!base::DeleteFile(path))
      RecordRegistration(RegistrationResult::kFailToDeleteExistingRegistration);
  };
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(delete_file_callback,
                     ShellUtil::GetApplicationPathForProgId(prog_id)));
  bool result = ShellUtil::DeleteFileAssociations(prog_id);
  if (!result) {
    RecordRegistration(
        RegistrationResult::
            kFailToDeleteFileAssociationsForExistingRegistration);
  }
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterFileHandlersWithOsTask, app_id, app_name,
                     profile_path, file_extensions, app_name_extension));
}

void RegisterFileHandlersWithOs(const AppId& app_id,
                                const std::string& app_name,
                                Profile* profile,
                                const apps::FileHandlers& file_handlers) {
  base::string16 app_name_extension;
  base::FilePath only_profile_with_app_installed;
  // Determine if there is exactly one other profile with the
  // app installed, before doing this new registration.
  if (IsWebAppLauncherRegisteredWithWindows(app_id, profile,
                                            &only_profile_with_app_installed))
    app_name_extension = GetAppNameExtensionForProfile(profile->GetPath());

  std::set<std::string> file_extensions =
      apps::GetFileExtensionsFromFileHandlers(file_handlers);
  std::set<base::string16> file_extensions16;
  for (const auto& file_extension : file_extensions) {
    // The file extensions in apps::FileHandler include a '.' prefix, which must
    // be removed.
    file_extensions16.insert(base::UTF8ToUTF16(file_extension.substr(1)));
  }

  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterFileHandlersWithOsTask, app_id, app_name,
                     profile->GetPath(), file_extensions16,
                     app_name_extension));
  // If there's another profile that needs its name changed, reregister it.
  if (!only_profile_with_app_installed.empty()) {
    ReRegisterFileHandlersWithOs(
        app_id, app_name, only_profile_with_app_installed, file_extensions16,
        GetProgIdForApp(only_profile_with_app_installed, app_id),
        GetAppNameExtensionForProfile(only_profile_with_app_installed));
  }
}

void UnregisterFileHandlersWithOs(const AppId& app_id, Profile* profile) {
  // Need to delete the app-specific-launcher file, since uninstall may not
  // remove the web application directory. This must be done before cleaning up
  // the registry, since the app-specific-launcher path is retrieved from the
  // registry.

  base::string16 prog_id = GetProgIdForApp(profile->GetPath(), app_id);
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(prog_id);
  ShellUtil::DeleteFileAssociations(prog_id);

  // Need to delete the hardlink file as well, since extension uninstall
  // by default doesn't remove the web application directory.
  if (!app_specific_launcher_path.empty()) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(base::GetDeleteFileCallback(),
                       app_specific_launcher_path));
  }
  base::FilePath only_profile_with_app_installed;
  if (IsWebAppLauncherRegisteredWithWindows(app_id, profile,
                                            &only_profile_with_app_installed) &&
      !only_profile_with_app_installed.empty()) {
    base::string16 remaining_prog_id =
        GetProgIdForApp(only_profile_with_app_installed, app_id);
    // Before deleting the file associations for the remaining app registration,
    // remember the app name and file extensions.
    ShellUtil::FileAssociationsAndAppName file_associations_and_app_name =
        ShellUtil::GetFileAssociationsAndAppName(remaining_prog_id);
    if (file_associations_and_app_name.app_name.empty()) {
      // If we can't get the file associations, just leave the remaining app
      // registration as is.
      return;
    }
    base::string16 app_name_extension =
        GetAppNameExtensionForProfile(only_profile_with_app_installed);
    // If |app_name| ends with " (<profile name>)", remove it.
    std::string new_app_name;
    if (base::EndsWith(file_associations_and_app_name.app_name,
                       app_name_extension, base::CompareCase::SENSITIVE) &&
        file_associations_and_app_name.app_name.size() >
            app_name_extension.size()) {
      new_app_name = base::UTF16ToUTF8(
          base::StringPiece16(file_associations_and_app_name.app_name.c_str(),
                              file_associations_and_app_name.app_name.size() -
                                  app_name_extension.size()));
    } else {
      // We probably don't have to reregister the app, but it's probably safer
      // to make sure the app name is correct.
      new_app_name = base::UTF16ToUTF8(file_associations_and_app_name.app_name);
    }
    ReRegisterFileHandlersWithOs(
        app_id, new_app_name, only_profile_with_app_installed,
        file_associations_and_app_name.file_associations, remaining_prog_id,
        base::string16());
  }
}

}  // namespace web_app
