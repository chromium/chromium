// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/os_integration/web_app_protocol_handler_registration.h"

#include <string>
#include <utility>

#include <shlobj.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/browser/web_applications/os_integration/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_win.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/shell_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

void RegisterProtocolHandlersWithOSInBackground(
    const web_app::AppId& app_id,
    const std::wstring& app_name,
    Profile* profile,
    const base::FilePath& profile_path,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    const std::wstring& app_name_extension) {
  base::AssertLongCPUWorkAllowed();

  if (web_app::GetShortcutOverrideForTesting()) {  // IN-TEST
    // Instead of modifying the registry, add them to the testing data.
    std::vector<std::string> protocols_registered;
    for (apps::ProtocolHandlerInfo& info : protocol_handlers) {
      protocols_registered.push_back(info.protocol);
    }
    web_app::GetShortcutOverrideForTesting()
        ->protocol_scheme_registrations.emplace_back(
            app_id, std::move(protocols_registered));
    return;
  }

  base::FilePath web_app_path =
      web_app::GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id,
                                                        GURL());

  absl::optional<base::FilePath> app_specific_launcher_path =
      web_app::CreateAppLauncherFile(app_name, app_name_extension,
                                     web_app_path);
  if (!app_specific_launcher_path.has_value())
    return;

  base::CommandLine app_specific_launcher_command =
      web_app::GetAppLauncherCommand(app_id, app_specific_launcher_path.value(),
                                     profile_path);

  std::wstring user_visible_app_name(app_name);
  user_visible_app_name.append(app_name_extension);

  base::FilePath icon_path = web_app::internals::GetIconFilePath(
      web_app_path, base::AsString16(app_name));

  std::wstring prog_id = web_app::GetProgIdForApp(profile_path, app_id);
  ShellUtil::AddApplicationClass(prog_id, app_specific_launcher_command,
                                 user_visible_app_name, user_visible_app_name,
                                 icon_path);

  std::vector<std::wstring> wstring_protocols;
  wstring_protocols.reserve(protocol_handlers.size());

  for (const auto& protocol_handler : protocol_handlers) {
    wstring_protocols.push_back(base::UTF8ToWide(protocol_handler.protocol));
  }

  // Add protocol associations to the Windows registry.
  ShellUtil::AddAppProtocolAssociations(wstring_protocols, prog_id);
  ::SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void UnregisterProtocolHandlersWithOsInBackground(
    const web_app::AppId& app_id,
    const base::FilePath& profile_path) {
  base::AssertLongCPUWorkAllowed();

  if (web_app::GetShortcutOverrideForTesting()) {  // IN-TEST
    // The unregistration is not tested due to complication in the
    // implementation of other OS's. Instead, we check if the updated
    // registrations are empty / don't have the offending protocol.
    return;
  }

  // Need to delete the app-specific-launcher file, since uninstall may not
  // remove the web application directory. This must be done before cleaning up
  // the registry, since the app-specific-launcher path is retrieved from the
  // registry.
  std::wstring prog_id = web_app::GetProgIdForApp(profile_path, app_id);
  base::FilePath app_specific_launcher_path =
      ShellUtil::GetApplicationPathForProgId(prog_id);

  // Need to delete the hardlink file as well, since extension uninstall
  // by default doesn't remove the web application directory.
  base::DeleteFile(app_specific_launcher_path);

  // Remove application class registry key.
  ShellUtil::DeleteApplicationClass(prog_id);

  // Remove protocol associations from the Windows registry.
  ShellUtil::RemoveAppProtocolAssociations(
      web_app::GetProgIdForApp(profile_path, app_id));
}

}  // namespace

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    ResultCallback callback) {
  if (protocol_handlers.empty()) {
    if (web_app::GetShortcutOverrideForTesting()) {  // IN-TEST
      web_app::GetShortcutOverrideForTesting()
          ->protocol_scheme_registrations.emplace_back(
              app_id, std::vector<std::string>());
    }
    std::move(callback).Run(Result::kOk);
    return;
  }

  std::wstring app_name_extension =
      GetAppNameExtensionForNextInstall(app_id, profile->GetPath());

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterProtocolHandlersWithOSInBackground, app_id,
                     base::UTF8ToWide(app_name), profile, profile->GetPath(),
                     std::move(protocol_handlers), app_name_extension),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile->GetPath(),
                     app_id, std::move(callback)));
}

void UnregisterProtocolHandlersWithOs(const AppId& app_id,
                                      Profile* profile,
                                      ResultCallback callback) {
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UnregisterProtocolHandlersWithOsInBackground, app_id,
                     profile->GetPath()),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile->GetPath(),
                     app_id, std::move(callback)));
}

}  // namespace web_app
