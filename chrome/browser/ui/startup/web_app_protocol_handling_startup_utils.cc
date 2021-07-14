// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_protocol_handling_startup_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/web_applications/components/app_registry_controller.h"
#include "chrome/browser/web_applications/components/os_integration_manager.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "url/gurl.h"

namespace {

// Tries to launch the web app when the `provider` is ready. `startup_callback`
// will run if there is no web app registered for `profile` that can handle
// `protocol_url`. If os_integration_manager finds a web app, then check if the
// web app has approval from the user to handle `protocol_url`. If the web app
// didn't have approval from a previous launch, show the permission dialog to
// ask for approval. The permission dialog will then launch the web app if the
// user accepts the dialog or close the dialog window if the user cancels it.
// If the web app did get permission in the past, the browser will directly
// launch the web app with the translated url. The passed in `keep_alive` and
// `profile_keep_alives` ensure the profiles and the browser are alive while
// `provider` is waiting for the signal for "on_registry_ready()".
void OnWebAppSystemReadyMaybeLaunchProtocolHandler(
    web_app::WebAppProvider* provider,
    const GURL& protocol_url,
    const web_app::AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile* last_used_profile,
    const std::vector<Profile*>& last_opened_profiles,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    const std::vector<std::unique_ptr<ScopedProfileKeepAlive>>
        profile_keep_alives,
    web_app::startup::FinalizeWebAppLaunchCallback finalize_callback,
    web_app::startup::StartupLaunchAfterProtocolCallback startup_callback) {
  web_app::OsIntegrationManager& os_integration_manager =
      provider->os_integration_manager();
  const std::vector<ProtocolHandler> handlers =
      os_integration_manager.GetHandlersForProtocol(protocol_url.scheme());

  if (!base::Contains(handlers, true, [](const auto& handler) {
        return handler.web_app_id().has_value();
      })) {
    std::move(startup_callback)
        .Run(command_line, cur_dir, profile, last_used_profile,
             last_opened_profiles);
    return;
  }

  auto launch_callback = base::BindOnce(
      [](const base::CommandLine& command_line, const base::FilePath& cur_dir,
         Profile* profile, const GURL& protocol_url,
         const web_app::AppId& app_id,
         web_app::startup::FinalizeWebAppLaunchCallback callback,
         bool accepted) {
        if (accepted) {
          web_app::WebAppProvider* provider =
              web_app::WebAppProvider::Get(profile);
          {
            web_app::ScopedRegistryUpdate update(
                provider->registry_controller().AsWebAppSyncBridge());
            web_app::WebApp* app_to_update = update->UpdateApp(app_id);
            std::vector<std::string> protocol_handlers(
                app_to_update->approved_launch_protocols());
            protocol_handlers.push_back(protocol_url.scheme());
            app_to_update->SetApprovedLaunchProtocols(
                std::move(protocol_handlers));
          }
          apps::AppServiceProxyFactory::GetForProfile(profile)
              ->BrowserAppLauncher()
              ->LaunchAppWithCallback(app_id, command_line, cur_dir,
                                      /*url_handler_launch_url=*/absl::nullopt,
                                      protocol_url, std::move(callback));
        }  // else allow the process to exit without opening a browser.
      },
      command_line, cur_dir, profile, protocol_url, app_id,
      std::move(finalize_callback));

  // Check if we have permission to launch the app directly.
  web_app::WebAppRegistrar& registrar = provider->registrar();
  if (registrar.IsApprovedLaunchProtocol(app_id, protocol_url.scheme())) {
    std::move(launch_callback).Run(true);
  } else {
    // ShowWebAppProtocolHandlerIntentPicker keeps the `profile` alive through
    // running of `launch_callback`.
    chrome::ShowWebAppProtocolHandlerIntentPicker(protocol_url, profile, app_id,
                                                  std::move(launch_callback));
  }
}

}  // namespace

namespace web_app {
namespace startup {

bool MaybeLaunchProtocolHandlerWebApp(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile* last_used_profile,
    const std::vector<Profile*>& last_opened_profiles,
    FinalizeWebAppLaunchCallback finalize_callback,
    StartupLaunchAfterProtocolCallback startup_callback) {
  std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
  // There must be a kAppId switch arg in the command line to launch.
  if (app_id.empty())
    return false;

  GURL protocol_url;
  base::CommandLine::StringVector args = command_line.GetArgs();
  for (const auto& arg : args) {
#if defined(OS_WIN)
    GURL potential_protocol(base::AsStringPiece16(arg));
#else
    GURL potential_protocol(arg);
#endif  // defined(OS_WIN)
    // protocol_url is checked for validity later with getting the provider and
    // consulting the os_integration_manager. However because that process has a
    // wait for "on_registry_ready()", `potential_protocol` checks for
    // blink::IsValidCustomHandlerScheme() here to avoid loading the
    // web_app::WebAppProvider with a false positive.
    bool unused_has_custom_scheme_prefix = false;
    if (potential_protocol.is_valid() &&
        blink::IsValidCustomHandlerScheme(potential_protocol.scheme(),
                                          /*allow_ext_prefix=*/false,
                                          unused_has_custom_scheme_prefix)) {
      protocol_url = std::move(potential_protocol);
      break;
    }
  }
  if (protocol_url.is_empty())
    return false;

  auto* provider = web_app::WebAppProvider::Get(profile);
  DCHECK(provider);
  // Create the keep_alives so the profiles and the browser stays alive as we
  // wait for the provider() to be ready.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::WEB_APP_PROTOCOL_HANDLER_LAUNCH,
      KeepAliveRestartOption::DISABLED);
  std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives;
  profile_keep_alives.push_back(std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kWebAppProtocolHandlerLaunch));
  profile_keep_alives.push_back(std::make_unique<ScopedProfileKeepAlive>(
      last_used_profile, ProfileKeepAliveOrigin::kWebAppProtocolHandlerLaunch));
  for (Profile* last_opened_profile : last_opened_profiles) {
    profile_keep_alives.push_back(std::make_unique<ScopedProfileKeepAlive>(
        last_opened_profile,
        ProfileKeepAliveOrigin::kWebAppProtocolHandlerLaunch));
  }

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(OnWebAppSystemReadyMaybeLaunchProtocolHandler, provider,
                     std::move(protocol_url), std::move(app_id), command_line,
                     cur_dir, profile, last_used_profile, last_opened_profiles,
                     std::move(keep_alive), std::move(profile_keep_alives),
                     std::move(finalize_callback),
                     std::move(startup_callback)));
  return true;
}

}  // namespace startup
}  // namespace web_app
