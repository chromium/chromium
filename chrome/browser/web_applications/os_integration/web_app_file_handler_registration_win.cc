// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_file_handler_registration.h"

#include <iterator>
#include <set>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/installer/util/shell_util.h"
#include "components/services/app_service/public/cpp/file_handler.h"

namespace web_app {

bool ShouldRegisterFileHandlersWithOs() {
  return true;
}

bool FileHandlingIconsSupportedByOs() {
  // TODO(crbug.com/40185571): implement and flip this to true.
  return false;
}

void RegisterFileHandlersWithOsTask(const webapps::AppId& app_id,
                                    const std::wstring& app_name,
                                    const base::FilePath& profile_path,
                                    const apps::FileHandlers& file_handlers,
                                    const std::wstring& app_name_extension) {
  const base::FilePath web_app_path =
      GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id, GURL());
  std::optional<base::FilePath> app_specific_launcher_path =
      CreateAppLauncherFile(app_name, app_name_extension, web_app_path);
  if (!app_specific_launcher_path.has_value())
    return;

  const base::CommandLine app_specific_launcher_command = GetAppLauncherCommand(
      app_id, app_specific_launcher_path.value(), profile_path);

  std::wstring user_visible_app_name(app_name);
  user_visible_app_name.append(app_name_extension);

  const base::FilePath icon_path =
      internals::GetIconFilePath(web_app_path, base::AsString16(app_name));

  // Although this ProgId won't be used to launch web apps with file handlers,
  // it makes it easy to tell if the web app is installed in a profile, and is
  // consistent with the way web apps that handle protocols are registered.
  ShellUtil::AddApplicationClass(GetProgIdForApp(profile_path, app_id),
                                 app_specific_launcher_command,
                                 user_visible_app_name, app_name, icon_path);

  // Iterate over the file handlers and add file associations for each
  // of them, using the `display_name` in the file handler, not the ProgId for
  // the app, so that we can have separate display names in Windows Explorer and
  // separate icons for the file handler in the Open With context menu.
  std::vector<std::wstring> file_handler_progids;
  bool result = true;
  for (const auto& file_handler : file_handlers) {
    std::set<std::string> file_extensions =
        apps::GetFileExtensionsFromFileHandler(file_handler);
    std::set<std::wstring> file_extensions_wide;
    for (const auto& file_extension : file_extensions) {
      // The file extensions in apps::FileHandler include a '.' prefix, which
      // must be removed.
      file_extensions_wide.insert(base::UTF8ToWide(file_extension.substr(1)));
    }

    file_handler_progids.push_back(
        GetProgIdForAppFileHandler(profile_path, app_id, file_extensions));
    result &= ShellUtil::AddFileAssociations(
        file_handler_progids.back(), app_specific_launcher_command,
        user_visible_app_name,
        base::AsWString(std::u16string_view(file_handler.display_name)),
        icon_path, file_extensions_wide);
  }
  if (!result)
    RecordRegistration(RegistrationResult::kFailToAddFileAssociation);
  else
    RecordRegistration(RegistrationResult::kSuccess);
  // Store the app file handler ProgIds in the registry so that we can
  // unregister the app file handler ProgIds at uninstall time. At uninstall
  // time, all we have is the `app_id`.
  ShellUtil::RegisterFileHandlerProgIdsForAppId(
      GetProgIdForApp(profile_path, app_id), file_handler_progids);
}

void RegisterFileHandlersWithOs(const webapps::AppId& app_id,
                                const std::string& app_name,
                                const base::FilePath& profile_path,
                                const apps::FileHandlers& file_handlers,
                                ResultCallback callback) {
  DCHECK(!file_handlers.empty());

  const std::wstring app_name_extension =
      GetAppNameExtensionForNextInstall(app_id, profile_path);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterFileHandlersWithOsTask, app_id,
                     base::UTF8ToWide(app_name), profile_path, file_handlers,
                     app_name_extension),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile_path, app_id,
                     std::move(callback)));
}

void DeleteAppLauncher(const base::FilePath& launcher_path) {
  // Need to delete the app launcher file, since extension uninstall by default
  // doesn't remove the web application directory.
  base::DeleteFile(launcher_path);
}

void UnregisterFileHandlersWithOs(const webapps::AppId& app_id,
                                  const base::FilePath& profile_path,
                                  ResultCallback callback) {
  // The app-specific-launcher file name must be calculated before cleaning up
  // the registry, since the app-specific-launcher path is retrieved from the
  // registry.
  const std::wstring prog_id = GetProgIdForApp(profile_path, app_id);
  const base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(prog_id);
  // This needs to be done synchronously. If it's done via a task, protocol
  // unregistration will delete HKCU\Classes\<progid> before the task runs.
  // Information in that key is needed to unregister file associations.
  ShellUtil::DeleteFileAssociations(prog_id);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&DeleteAppLauncher, app_specific_launcher_path),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile_path, app_id,
                     std::move(callback)));
}

}  // namespace web_app
