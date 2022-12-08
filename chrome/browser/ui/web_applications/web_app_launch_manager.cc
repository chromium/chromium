// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/web_applications/web_app_launch_process.h"
#include "chrome/browser/ui/web_applications/web_app_launch_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/common/constants.h"
#include "third_party/blink/public/common/custom_handlers/protocol_handler_utils.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace web_app {

WebAppLaunchManager::WebAppLaunchManager(Profile* profile)
    : profile_(profile),
      provider_(WebAppProvider::GetForLocalAppsUnchecked(profile)) {}

WebAppLaunchManager::~WebAppLaunchManager() = default;

content::WebContents* WebAppLaunchManager::OpenApplication(
    apps::AppLaunchParams&& params) {
  if (GetOpenApplicationCallbackForTesting())
    return GetOpenApplicationCallbackForTesting().Run(std::move(params));

  WebAppProvider* provider =
      WebAppProvider::GetForLocalAppsUnchecked(profile_.get());
  DCHECK(provider);
  return WebAppLaunchProcess::CreateAndRun(
      *profile_, provider->registrar_unsafe(),
      provider->os_integration_manager(), params);
}

void WebAppLaunchManager::LaunchApplication(
    const std::string& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const absl::optional<GURL>& url_handler_launch_url,
    const absl::optional<GURL>& protocol_handler_launch_url,
    const absl::optional<GURL>& file_launch_url,
    const std::vector<base::FilePath>& launch_files,
    base::OnceCallback<void(Browser* browser, apps::LaunchContainer container)>
        callback) {
  if (!provider_)
    return;

  // At most one of these parameters should be non-empty.
  DCHECK_LE(url_handler_launch_url.has_value() +
                protocol_handler_launch_url.has_value() + !launch_files.empty(),
            1);

  apps::LaunchSource launch_source = apps::LaunchSource::kFromCommandLine;

  if (url_handler_launch_url.has_value()) {
    launch_source = apps::LaunchSource::kFromUrlHandler;
  } else if (!launch_files.empty()) {
    DCHECK(file_launch_url.has_value());
    launch_source = apps::LaunchSource::kFromFileManager;
  }

  if (base::FeatureList::IsEnabled(features::kDesktopPWAsRunOnOsLogin) &&
      command_line.HasSwitch(switches::kAppRunOnOsLoginMode)) {
    launch_source = apps::LaunchSource::kFromOsLogin;
  } else if (protocol_handler_launch_url.has_value()) {
    launch_source = apps::LaunchSource::kFromProtocolHandler;
  }

  apps::AppLaunchParams params(
      app_id, apps::LaunchContainer::kLaunchContainerWindow,
      WindowOpenDisposition::NEW_WINDOW, launch_source);
  params.command_line = command_line;
  params.current_directory = current_directory;
  params.launch_files = launch_files;
  params.url_handler_launch_url = url_handler_launch_url;
  params.protocol_handler_launch_url = protocol_handler_launch_url;
  if (file_launch_url) {
    params.override_url = *file_launch_url;
  } else {
    params.override_url = GURL(command_line.GetSwitchValueASCII(
        switches::kAppLaunchUrlForShortcutsMenuItem));
  }

  // Wait for the web applications database to load.
  // If the profile and WebAppLaunchManager are destroyed,
  // on_registry_ready will not fire.
  provider_->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&WebAppLaunchManager::LaunchWebApplication,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::move(params), std::move(callback)));
}

// static
void WebAppLaunchManager::SetOpenApplicationCallbackForTesting(
    OpenApplicationCallback callback) {
  GetOpenApplicationCallbackForTesting() = std::move(callback);
}

// static
WebAppLaunchManager::OpenApplicationCallback&
WebAppLaunchManager::GetOpenApplicationCallbackForTesting() {
  static base::NoDestructor<WebAppLaunchManager::OpenApplicationCallback>
      callback;
  return *callback;
}
void WebAppLaunchManager::LaunchWebApplication(
    apps::AppLaunchParams&& params,
    base::OnceCallback<void(Browser* browser, apps::LaunchContainer container)>
        callback) {
  apps::LaunchContainer container;
  Browser* browser = nullptr;
  if (provider_->registrar_unsafe().IsInstalled(params.app_id)) {
    if (provider_->registrar_unsafe().GetAppEffectiveDisplayMode(
            params.app_id) == blink::mojom::DisplayMode::kBrowser) {
      params.container = apps::LaunchContainer::kLaunchContainerTab;
      params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    }

    container = params.container;
    const content::WebContents* web_contents =
        OpenApplication(std::move(params));
    if (web_contents)
      browser = chrome::FindBrowserWithWebContents(web_contents);
  } else {
    // Open an empty browser window as the app_id is invalid.
    container = apps::LaunchContainer::kLaunchContainerNone;
    browser = apps::CreateBrowserWithNewTabPage(profile_);
  }
  std::move(callback).Run(browser, container);
}

}  // namespace web_app
