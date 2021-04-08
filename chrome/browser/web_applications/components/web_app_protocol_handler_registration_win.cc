// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "chrome/browser/web_applications/components/web_app_protocol_handler_registration.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/chrome_pwa_launcher/chrome_pwa_launcher_util.h"
#include "chrome/browser/web_applications/components/web_app_handler_registration_utils_win.h"
#include "chrome/browser/web_applications/components/web_app_shortcut.h"
#include "chrome/browser/web_applications/components/web_app_shortcut_win.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/shell_util.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
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
  base::FilePath web_app_path =
      web_app::GetOsIntegrationResourcesDirectoryForApp(profile_path, app_id,
                                                        GURL());

  base::Optional<base::FilePath> app_specific_launcher_path =
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

  ShellUtil::AddApplicationClass(web_app::GetProgIdForApp(profile_path, app_id),
                                 app_specific_launcher_command,
                                 user_visible_app_name, user_visible_app_name,
                                 icon_path);

  // Post to UI thread to access ProtocolHandlerRegistry.
  // TODO(crbug.com/1174805): We should move this to ProtocolHandlerManager and
  // use a callback instead.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](Profile* profile, const web_app::AppId& app_id,
             std::vector<apps::ProtocolHandlerInfo> protocol_handlers) {
            ProtocolHandlerRegistry* registry =
                ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);

            registry->RegisterAppProtocolHandlers(app_id, protocol_handlers);
          },
          profile, app_id, std::move(protocol_handlers)));
}

void UnregisterProtocolHandlersWithOsInBackground(
    const web_app::AppId& app_id,
    const base::FilePath& profile_path) {
  base::AssertLongCPUWorkAllowed();
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

  // Clean up application class registry key.
  ShellUtil::DeleteApplicationClass(prog_id);
}

// TODO(crbug/1019239): Update CheckAndUpdateExternalInstallations
// to receive a callback that returns a bool. For now, return the call back
// below for test purposes (StartupBrowserWebAppProtocolHandlingTest).
void VerifyExternalInstallations(const base::FilePath& cur_profile_path,
                                 const web_app::AppId& app_id,
                                 base::OnceCallback<void(bool)> callback) {
  web_app::CheckAndUpdateExternalInstallations(cur_profile_path, app_id,
                                               base::DoNothing::Once());
  std::move(callback).Run(true);
}

}  // namespace

namespace web_app {

void RegisterProtocolHandlersWithOs(
    const AppId& app_id,
    const std::string& app_name,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers,
    base::OnceCallback<void(bool)> callback) {
  if (protocol_handlers.empty())
    return;

  std::wstring app_name_extension =
      GetAppNameExtensionForNextInstall(app_id, profile->GetPath());

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&RegisterProtocolHandlersWithOSInBackground, app_id,
                     base::UTF8ToWide(app_name), profile, profile->GetPath(),
                     std::move(protocol_handlers), app_name_extension),
      base::BindOnce(&VerifyExternalInstallations, profile->GetPath(), app_id,
                     std::move(callback)));
}

void UnregisterProtocolHandlersWithOs(
    const AppId& app_id,
    Profile* profile,
    std::vector<apps::ProtocolHandlerInfo> protocol_handlers) {
  ProtocolHandlerRegistry* registry =
      ProtocolHandlerRegistryFactory::GetForBrowserContext(profile);

  registry->DeregisterAppProtocolHandlers(app_id, protocol_handlers);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&UnregisterProtocolHandlersWithOsInBackground, app_id,
                     profile->GetPath()),
      base::BindOnce(&CheckAndUpdateExternalInstallations, profile->GetPath(),
                     app_id, base::DoNothing::Once()));
}

}  // namespace web_app
