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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/startup/launch_mode_recorder.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
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

// Encapsulates a browser process keep alive and a profile keep alive. This is
// mainly useful for shutting down the process after the user denies a launch
// permission prompt.
struct WebAppLaunchKeepAlive {
  explicit WebAppLaunchKeepAlive(Profile* profile)
      : profile_keep_alive_(
            profile,
            ProfileKeepAliveOrigin::kWebAppPermissionDialogWindow),
        keep_alive_(KeepAliveOrigin::WEB_APP_INTENT_PICKER,
                    KeepAliveRestartOption::DISABLED) {}
  ~WebAppLaunchKeepAlive() = default;

  ScopedProfileKeepAlive profile_keep_alive_;
  ScopedKeepAlive keep_alive_;
};

void OnAppLaunched(std::unique_ptr<WebAppLaunchKeepAlive> keep_alive,
                   Browser* browser,
                   apps::mojom::LaunchContainer container) {
  FinalizeWebAppLaunch(absl::nullopt, browser, container);
  // The WebAppLaunchKeepAlive can now go out of scope, because the app is
  // launched which will keep the process alive.
}

void OnPersistProtocolHandlersUserChoiceCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    const GURL& protocol_url,
    const AppId& app_id,
    std::unique_ptr<WebAppLaunchKeepAlive> keep_alive,
    bool allowed) {
  if (!allowed) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    keep_alive.release());
    return;
  }

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(
          app_id, command_line, cur_dir,
          /*url_handler_launch_url=*/absl::nullopt, protocol_url,
          /*launch_files=*/{},
          base::BindOnce(&OnAppLaunched, std::move(keep_alive)));
}

void OnPersistFileHandlersUserChoiceCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    std::vector<base::FilePath> launch_files,
    const AppId& app_id,
    std::unique_ptr<WebAppLaunchKeepAlive> keep_alive,
    bool allowed) {
  if (!allowed) {
    base::ThreadTaskRunnerHandle::Get()->DeleteSoon(FROM_HERE,
                                                    keep_alive.release());
    return;
  }

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(
          app_id, command_line, cur_dir,
          /*url_handler_launch_url=*/absl::nullopt,
          /*protocol_handler_launch_url=*/absl::nullopt, launch_files,
          base::BindOnce(&OnAppLaunched, std::move(keep_alive)));
}

void OnProtocolHandlerDialogCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* profile,
    const GURL& protocol_url,
    const AppId& app_id,
    std::unique_ptr<WebAppLaunchKeepAlive> keep_alive,
    bool allowed,
    bool remember_user_choice) {
  auto launch_callback = base::BindOnce(
      &OnPersistProtocolHandlersUserChoiceCompleted, command_line, cur_dir,
      profile, protocol_url, app_id, std::move(keep_alive), allowed);

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
    std::unique_ptr<WebAppLaunchKeepAlive> keep_alive,
    bool allowed,
    bool remember_user_choice) {
  auto launch_callback = base::BindOnce(
      &OnPersistFileHandlersUserChoiceCompleted, command_line, cur_dir, profile,
      std::move(launch_files), app_id, std::move(keep_alive), allowed);

  if (remember_user_choice) {
    PersistFileHandlersUserChoice(profile, app_id, allowed,
                                  std::move(launch_callback));
  } else {
    std::move(launch_callback).Run();
  }
}

// Determines if the launch is a protocol handler launch. If so, takes
// responsibility for the rest of the launch process and returns true.
// Otherwise, returns false.
bool MaybeLaunchProtocolHandler(const base::CommandLine& command_line,
                                const base::FilePath& cur_dir,
                                Profile* profile,
                                const AppId& app_id) {
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
  if (protocol_url.is_empty())
    return false;

  // Check if the user has already disallowed this app to launch the protocol.
  // This check takes priority over checking if the protocol is handled
  // by the application.
  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  DCHECK(provider->on_registry_ready().is_signaled());
  WebAppRegistrar& registrar = provider->registrar();
  if (registrar.IsDisallowedLaunchProtocol(app_id, protocol_url.scheme())) {
    // If disallowed, return true to signal that the launch is spoken for, but
    // do not launch a browser or app window. Create and destroy a
    // WebAppLaunchKeepAlive to make sure the process shuts down.
    WebAppLaunchKeepAlive keep_alive(profile);
    return true;
  }

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
    return false;
  }

  // We've committed to handling this as a protocol handler launch. The
  // `WebAppLaunchKeepAlive` makes sure the process shuts down if the launch is
  // denied by the user.
  auto launch_callback = base::BindOnce(
      &OnProtocolHandlerDialogCompleted, command_line, cur_dir, profile,
      protocol_url, app_id, std::make_unique<WebAppLaunchKeepAlive>(profile));

  // Check if we have permission to launch the app directly.
  if (registrar.IsAllowedLaunchProtocol(app_id, protocol_url.scheme())) {
    std::move(launch_callback)
        .Run(/*allowed=*/true, /*remember_user_choice=*/false);
  } else {
    chrome::ShowWebAppProtocolHandlerIntentPicker(protocol_url, profile, app_id,
                                                  std::move(launch_callback));
  }
  return true;
}

// Determines if the launch is a file handler launch. If so, takes
// responsibility for the rest of the launch process and returns true.
// Otherwise, returns false.
bool MaybeLaunchFileHandler(const base::CommandLine& command_line,
                            const base::FilePath& cur_dir,
                            Profile* profile,
                            const AppId& app_id) {
  // This path is only used when file handling is gated on settings.
  if (!base::FeatureList::IsEnabled(
          features::kDesktopPWAsFileHandlingSettingsGated)) {
    return false;
  }

  std::vector<base::FilePath> launch_files =
      apps::GetLaunchFilesFromCommandLine(command_line);
  if (launch_files.empty())
    return false;

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  absl::optional<GURL> file_handler_url =
      provider->os_integration_manager().GetMatchingFileHandlerURL(
          app_id, launch_files);
  if (!file_handler_url)
    return false;

  const WebApp* web_app = provider->registrar().GetAppById(app_id);
  DCHECK(web_app);

  // We've committed to handling this as a file handler launch. The `KeepAlive`
  // makes sure the process shuts down if the launch is denied by the user.
  auto launch_callback =
      base::BindOnce(&OnFileHandlerDialogCompleted, command_line, cur_dir,
                     profile, std::move(launch_files), app_id,
                     std::make_unique<WebAppLaunchKeepAlive>(profile));

  switch (web_app->file_handler_approval_state()) {
    case ApiApprovalState::kRequiresPrompt:
      // ShowWebAppProtocolHandlerIntentPicker keeps the `profile` alive through
      // holding onto `launch_callback`.
      // TODO(estade): this should use a file handling dialog, but until that's
      // implemented, this reuses the PH dialog as a stand-in.
      chrome::ShowWebAppProtocolHandlerIntentPicker(GURL("https://example.com"),
                                                    profile, app_id,
                                                    std::move(launch_callback));
      break;
    case ApiApprovalState::kAllowed:
      std::move(launch_callback)
          .Run(/*allowed=*/true, /*remember_user_choice=*/false);
      break;
    case ApiApprovalState::kDisallowed:
      // The disallowed case should have been handled by
      // `GetMatchingFileHandlerURL()`.
      NOTREACHED();
      break;
  }
  return true;
}

void OnAppRegistryReady(const base::CommandLine& command_line,
                        const base::FilePath& cur_dir,
                        Profile* profile,
                        std::string app_id) {
  if (MaybeLaunchProtocolHandler(command_line, cur_dir, profile, app_id) ||
      MaybeLaunchFileHandler(command_line, cur_dir, profile, app_id)) {
    return;
  }

  // Fall back to a normal app launch. This opens an empty browser window if the
  // app_id is invalid.
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(
          app_id, command_line, cur_dir,
          /*url_handler_launch_url=*/absl::nullopt,
          /*protocol_handler_launch_url=*/absl::nullopt,
          /*launch_files=*/{},
          base::BindOnce(&FinalizeWebAppLaunch,
                         LaunchMode::kAsWebAppInWindowByAppId));
}

}  // namespace

bool MaybeHandleWebAppLaunch(const base::CommandLine& command_line,
                             const base::FilePath& cur_dir,
                             Profile* profile) {
  std::string app_id = command_line.GetSwitchValueASCII(switches::kAppId);
  // There must be a kAppId switch arg in the command line to launch.
  if (app_id.empty())
    return false;

  WebAppProvider* const provider = WebAppProvider::GetForWebApps(profile);
  DCHECK(provider);
  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&OnAppRegistryReady, command_line, cur_dir,
                                profile, app_id));
  return true;
}

void FinalizeWebAppLaunch(absl::optional<LaunchMode> app_launch_mode,
                          Browser* browser,
                          apps::mojom::LaunchContainer container) {
  if (!browser)
    return;

  LaunchMode mode;
  switch (container) {
    case apps::mojom::LaunchContainer::kLaunchContainerWindow:
      DCHECK(browser->is_type_app());
      mode = app_launch_mode.value_or(LaunchMode::kAsWebAppInWindowOther);
      break;
    case apps::mojom::LaunchContainer::kLaunchContainerTab:
      DCHECK(!browser->is_type_app());
      mode = LaunchMode::kAsWebAppInTab;
      break;
    case apps::mojom::LaunchContainer::kLaunchContainerPanelDeprecated:
      NOTREACHED();
      FALLTHROUGH;
    case apps::mojom::LaunchContainer::kLaunchContainerNone:
      DCHECK(!browser->is_type_app());
      mode = LaunchMode::kUnknownWebApp;
      break;
  }

  LaunchModeRecorder().SetLaunchMode(mode);

  StartupBrowserCreatorImpl::MaybeToggleFullscreen(browser);
}

}  // namespace startup
}  // namespace web_app
