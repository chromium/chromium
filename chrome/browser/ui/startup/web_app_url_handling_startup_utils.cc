// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_url_handling_startup_utils.h"

#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/components/url_handler_launch_params.h"
#include "chrome/browser/web_applications/components/url_handler_manager_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

void LaunchApp(const base::FilePath& profile_path,
               const web_app::AppId& app_id,
               const base::CommandLine& command_line,
               const base::FilePath& cur_dir,
               const GURL& url,
               web_app::startup::FinalizeWebAppLaunchCallback callback) {
  apps::AppServiceProxyFactory::GetForProfile(
      g_browser_process->profile_manager()->GetProfile(profile_path))
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(app_id, command_line, cur_dir, url,
                              /*protocol_handler_launch_url=*/absl::nullopt,
                              std::move(callback));
}

bool LaunchFirstValidMatch(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    const std::vector<web_app::UrlHandlerLaunchParams>& url_handler_matches,
    web_app::startup::FinalizeWebAppLaunchCallback callback) {
  // Launch the first match for which a Profile can be loaded.
  // TODO(crbug/1072058): Use WebAppUiManagerImpl and WebAppDialogManager
  // to display the intent picker dialog. Use the first match here for testing.
  // TODO(crbug/1072058): Check user preferences before showing intent picker.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* profile = nullptr;
  const web_app::UrlHandlerLaunchParams* found_match = nullptr;
  for (const auto& match : url_handler_matches) {
    // Do not load profile if profile path is not valid.
    if (!profile_manager->GetProfileAttributesStorage()
             .GetProfileAttributesWithPath(match.profile_path)) {
      continue;
    }
    profile = profile_manager->GetProfile(match.profile_path);
    if (profile) {
      found_match = &match;
      break;
    }
  }

  if (profile && found_match) {
    LaunchApp(found_match->profile_path, found_match->app_id, command_line,
              cur_dir, found_match->url, std::move(callback));
    return true;
  }
  return false;
}

}  // namespace

namespace web_app {
namespace startup {

bool MaybeLaunchUrlHandlerWebAppFromCmd(const base::CommandLine& command_line,
                                        const base::FilePath& cur_dir,
                                        FinalizeWebAppLaunchCallback callback) {
  return LaunchFirstValidMatch(
      command_line, cur_dir,
      UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line),
      std::move(callback));
}

bool MaybeLaunchUrlHandlerWebAppFromUrls(
    const std::vector<GURL>& urls,
    FinalizeWebAppLaunchCallback callback) {
  if (urls.size() != 1)
    return false;

  return LaunchFirstValidMatch(
      base::CommandLine(base::CommandLine::NO_PROGRAM), base::FilePath(),
      UrlHandlerManagerImpl::GetUrlHandlerMatches(urls.front()),
      std::move(callback));
}

}  // namespace startup
}  // namespace web_app
