// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_startup_utils.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/web_applications/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "url/gurl.h"

namespace web_app {
namespace startup {

namespace {

using content::ProtocolHandler;

void OnAppLaunched(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    Browser* browser,
    apps::mojom::LaunchContainer container) {
  std::move(app_launched_callback).Run(browser, container);
  // The KeepAlives can now go out of scope, because the app is launched which
  // will keep the process alive.
}

void OnPersistProtocolHandlersUserChoiceCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    const GURL& protocol_url,
    const AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    bool allowed) {
  if (!allowed)
    return;  // Allow the process to exit without opening a browser.

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(
          app_id, command_line, cur_dir,
          /*url_handler_launch_url=*/absl::nullopt, protocol_url,
          /*launch_files=*/{},
          base::BindOnce(&OnAppLaunched, std::move(keep_alive),
                         std::move(profile_keep_alives),
                         std::move(app_launched_callback)));
}

void OnProtocolHandlerDialogCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    const GURL& protocol_url,
    const AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    bool allowed,
    bool remember_user_choice) {
  auto launch_callback =
      base::BindOnce(&OnPersistProtocolHandlersUserChoiceCompleted,
                     command_line, cur_dir, profile, protocol_url, app_id,
                     std::move(keep_alive), std::move(profile_keep_alives),
                     std::move(app_launched_callback), allowed);

  if (remember_user_choice) {
    PersistProtocolHandlersUserChoice(profile, app_id, protocol_url, allowed,
                                      std::move(launch_callback));
  } else {
    std::move(launch_callback).Run();
  }
}

void OnFileHandlerDialogCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::vector<base::FilePath> launch_files,
    const AppId& app_id,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    bool allowed,
    bool remember_user_choice) {
  if (!allowed)
    return;

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(
          app_id, command_line, cur_dir,
          /*url_handler_launch_url=*/absl::nullopt,
          /*protocol_handler_launch_url=*/absl::nullopt, launch_files,
          base::BindOnce(&OnAppLaunched, std::move(keep_alive),
                         std::move(profile_keep_alives),
                         std::move(app_launched_callback)));
}

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
    WebAppProvider* provider,
    const GURL& protocol_url,
    const AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    ContinueStartupCallback startup_callback) {
  // Check if the user has already disallowed this app to launch the protocol.
  // This check takes priority over checking if the protocol is handled
  // by the application. Do not run |startup_callback|, as that will launch the
  // app to its normal start page.
  WebAppRegistrar& registrar = provider->registrar();
  if (registrar.IsDisallowedLaunchProtocol(app_id, protocol_url.scheme()))
    return;

  OsIntegrationManager& os_integration_manager =
      provider->os_integration_manager();
  const std::vector<ProtocolHandler> handlers =
      os_integration_manager.GetHandlersForProtocol(protocol_url.scheme());

  // TODO(https://crbug.com/1249907): This code should be simplified such that
  // it only checks if the protocol is associated with the app_id.
  // |GetHandlersForProtocol| will return a list of all apps that can handle
  // the protocol, which is unnecessary here.
  if (!base::Contains(handlers, true, [](const auto& handler) {
        return handler.web_app_id().has_value();
      })) {
    std::move(startup_callback).Run();
    return;
  }

  // Check if we have permission to launch the app directly.
  if (registrar.IsAllowedLaunchProtocol(app_id, protocol_url.scheme())) {
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->BrowserAppLauncher()
        ->LaunchAppWithCallback(
            app_id, command_line, cur_dir,
            /*url_handler_launch_url=*/absl::nullopt, protocol_url,
            /*launch_files=*/{},
            base::BindOnce(&OnAppLaunched, std::move(keep_alive),
                           std::move(profile_keep_alives),
                           std::move(app_launched_callback)));
  } else {
    auto launch_callback = base::BindOnce(
        &OnProtocolHandlerDialogCompleted, command_line, cur_dir, profile,
        protocol_url, app_id, std::move(keep_alive),
        std::move(profile_keep_alives), std::move(app_launched_callback));

    // ShowWebAppProtocolHandlerIntentPicker keeps the `profile` alive through
    // running of `launch_callback`.
    chrome::ShowWebAppProtocolHandlerIntentPicker(protocol_url, profile, app_id,
                                                  std::move(launch_callback));
  }
}

void OnWebAppSystemReadyMaybeLaunchFileHandler(
    WebAppProvider* provider,
    std::vector<base::FilePath> launch_files,
    const AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::vector<std::unique_ptr<ScopedProfileKeepAlive>> profile_keep_alives,
    FinalizeWebAppLaunchCallback app_launched_callback,
    ContinueStartupCallback startup_callback) {
  absl::optional<GURL> file_handler_url =
      provider->os_integration_manager().GetMatchingFileHandlerURL(
          app_id, launch_files);
  if (!file_handler_url) {
    std::move(startup_callback).Run();
    return;
  }

  auto launch_callback = base::BindOnce(
      &OnFileHandlerDialogCompleted, command_line, cur_dir, profile,
      std::move(launch_files), app_id, std::move(keep_alive),
      std::move(profile_keep_alives), std::move(app_launched_callback));

  // ShowWebAppProtocolHandlerIntentPicker keeps the `profile` alive through
  // running of `launch_callback`.
  // TODO(estade): this should use a file handling dialog, but until that's
  // implemented, this reuses the PH dialog as a stand-in.
  chrome::ShowWebAppProtocolHandlerIntentPicker(
      *file_handler_url, profile, app_id, std::move(launch_callback));
}

}  // namespace

bool MaybeHandleEarlyWebAppLaunch(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    Profile* last_used_profile,
    const std::vector<Profile*>& last_opened_profiles,
    FinalizeWebAppLaunchCallback finalize_callback,
    ContinueStartupCallback startup_callback) {
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
    // WebAppProvider with a false positive.
    bool unused_has_custom_scheme_prefix = false;
    if (potential_protocol.is_valid() &&
        blink::IsValidCustomHandlerScheme(potential_protocol.scheme(),
                                          /*allow_ext_prefix=*/false,
                                          unused_has_custom_scheme_prefix)) {
      protocol_url = std::move(potential_protocol);
      break;
    }
  }
  std::vector<base::FilePath> launch_files;
  if (protocol_url.is_empty()) {
    // This path is only used when file handling is gated on settings.
    if (base::FeatureList::IsEnabled(
            features::kDesktopPWAsFileHandlingSettingsGated)) {
      launch_files = apps::GetLaunchFilesFromCommandLine(command_line);
    }
    if (launch_files.empty())
      return false;
  }

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
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

  if (!protocol_url.is_empty()) {
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(
            OnWebAppSystemReadyMaybeLaunchProtocolHandler, provider,
            std::move(protocol_url), std::move(app_id), command_line, cur_dir,
            profile, std::move(keep_alive), std::move(profile_keep_alives),
            std::move(finalize_callback), std::move(startup_callback)));
  } else {
    DCHECK(!launch_files.empty());
    provider->on_registry_ready().Post(
        FROM_HERE,
        base::BindOnce(
            OnWebAppSystemReadyMaybeLaunchFileHandler, provider,
            std::move(launch_files), std::move(app_id), command_line, cur_dir,
            profile, std::move(keep_alive), std::move(profile_keep_alives),
            std::move(finalize_callback), std::move(startup_callback)));
  }

  return true;
}

}  // namespace startup
}  // namespace web_app
