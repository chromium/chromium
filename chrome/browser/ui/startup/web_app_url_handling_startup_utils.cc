// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/startup/web_app_url_handling_startup_utils.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
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

void OnUrlHandlerIntentPickerDialogCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    base::OnceClosure on_urls_unhandled_cb,
    web_app::startup::FinalizeWebAppLaunchCallback app_launched_callback,
    bool accepted,
    absl::optional<web_app::UrlHandlerLaunchParams> selected_match) {
  // Dialog is not accepted. Quit the process and do nothing.
  if (!accepted)
    return;

  if (selected_match.has_value()) {
    // The user has selected an app to handle the URL.
    LaunchApp(selected_match->profile_path, selected_match->app_id,
              command_line, cur_dir, selected_match->url,
              std::move(app_launched_callback));
  } else {
    // The user has selected the browser. Open the link in the browser.
    std::move(on_urls_unhandled_cb).Run();
  }
}

std::vector<web_app::UrlHandlerLaunchParams> GetValidUrlHandlerMatches(
    std::vector<web_app::UrlHandlerLaunchParams> url_handler_matches,
    const base::FilePath& last_used_profile) {
  // TODO(crbug.com/1200951): Save matches from all valid profiles, not just
  // the last used profile.
  url_handler_matches.erase(
      std::remove_if(
          url_handler_matches.begin(), url_handler_matches.end(),
          [&last_used_profile](const web_app::UrlHandlerLaunchParams& match) {
            return match.profile_path != last_used_profile;
          }),
      url_handler_matches.end());
  return url_handler_matches;
}

}  // namespace

namespace web_app {
namespace startup {

bool MaybeLaunchUrlHandlerWebAppFromCmd(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    Profile* last_used_profile,
    base::OnceClosure on_urls_unhandled_cb,
    FinalizeWebAppLaunchCallback app_launched_callback) {
  auto valid_matches = GetValidUrlHandlerMatches(
      UrlHandlerManagerImpl::GetUrlHandlerMatches(command_line),
      last_used_profile->GetPath());
  if (valid_matches.empty())
    return false;

  chrome::ShowWebAppUrlHandlerIntentPickerDialog(
      std::move(valid_matches),
      base::BindOnce(&OnUrlHandlerIntentPickerDialogCompleted, command_line,
                     cur_dir, std::move(on_urls_unhandled_cb),
                     std::move(app_launched_callback)));
  return true;
}

void MaybeLaunchUrlHandlerWebAppFromUrls(
    const std::vector<GURL>& urls,
    base::OnceClosure on_urls_unhandled_cb,
    FinalizeWebAppLaunchCallback app_launched_callback) {
  if (urls.size() != 1 || !g_browser_process->profile_manager()) {
    std::move(on_urls_unhandled_cb).Run();
    return;
  }

  auto valid_matches = GetValidUrlHandlerMatches(
      UrlHandlerManagerImpl::GetUrlHandlerMatches(urls.front()),
      g_browser_process->profile_manager()
          ->GetLastUsedProfileAllowedByPolicy()
          ->GetPath());
  if (valid_matches.empty()) {
    std::move(on_urls_unhandled_cb).Run();
    return;
  }

  chrome::ShowWebAppUrlHandlerIntentPickerDialog(
      std::move(valid_matches),
      base::BindOnce(&OnUrlHandlerIntentPickerDialogCompleted,
                     base::CommandLine(base::CommandLine::NO_PROGRAM),
                     base::FilePath(), std::move(on_urls_unhandled_cb),
                     std::move(app_launched_callback)));
}

}  // namespace startup
}  // namespace web_app
