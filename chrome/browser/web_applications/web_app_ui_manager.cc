// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_ui_manager.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_switches.h"

namespace web_app {


// static
apps::AppLaunchParams WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
    const webapps::AppId& app_id,
    const base::CommandLine& command_line,
    const base::FilePath& current_directory,
    const std::optional<GURL>& protocol_handler_launch_url,
    const std::optional<GURL>& file_launch_url,
    const std::vector<base::FilePath>& launch_files) {
  // At most one of these parameters should be non-empty.
  DCHECK_LE(protocol_handler_launch_url.has_value() + !launch_files.empty(), 1);

  apps::LaunchSource launch_source = apps::LaunchSource::kFromCommandLine;
  if (!launch_files.empty()) {
    DCHECK(file_launch_url.has_value());
    launch_source = apps::LaunchSource::kFromFileManager;
  }

  if (command_line.HasSwitch(switches::kAppRunOnOsLoginMode)) {
    launch_source = apps::LaunchSource::kFromOsLogin;
  } else if (protocol_handler_launch_url.has_value()) {
    launch_source = apps::LaunchSource::kFromProtocolHandler;
  }

  apps::AppLaunchParams params(app_id,
                               apps::LaunchContainer::kLaunchContainerNone,
                               WindowOpenDisposition::UNKNOWN, launch_source);
  params.command_line = command_line;
  params.current_directory = current_directory;
  params.launch_files = launch_files;
  params.protocol_handler_launch_url = protocol_handler_launch_url;
  if (file_launch_url) {
    params.override_url = *file_launch_url;
  } else {
    params.override_url = GURL(command_line.GetSwitchValueASCII(
        switches::kAppLaunchUrlForShortcutsMenuItem));
  }

  return params;
}

WebAppUiManager::WebAppUiManager() = default;

WebAppUiManager::~WebAppUiManager() {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnWebAppUiManagerDestroyed();
}

base::WeakPtr<WebAppUiManager> WebAppUiManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void WebAppUiManager::AddObserver(WebAppUiManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WebAppUiManager::RemoveObserver(WebAppUiManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppUiManager::NotifyReadyToCommitNavigation(
    const webapps::AppId& app_id,
    content::NavigationHandle* navigation_handle) {
  for (WebAppUiManagerObserver& observer : observers_)
    observer.OnReadyToCommitNavigation(app_id, navigation_handle);
}

FakeWebAppUiManager* WebAppUiManager::AsFakeWebAppUiManagerForTesting() {
  return nullptr;
}

}  // namespace web_app
