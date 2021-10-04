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
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_browser_creator.h"
#include "chrome/browser/ui/startup/startup_browser_creator_impl.h"
#include "chrome/browser/web_applications/url_handler_launch_params.h"
#include "chrome/browser/web_applications/url_handler_manager_impl.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

void LaunchApp(const base::FilePath& profile_path,
               const AppId& app_id,
               const base::CommandLine& command_line,
               const base::FilePath& cur_dir,
               const GURL& url,
               startup::FinalizeWebAppLaunchCallback callback) {
  apps::AppServiceProxyFactory::GetForProfile(
      g_browser_process->profile_manager()->GetProfile(profile_path))
      ->BrowserAppLauncher()
      ->LaunchAppWithCallback(app_id, command_line, cur_dir, url,
                              /*protocol_handler_launch_url=*/absl::nullopt,
                              /*launch_files=*/{}, std::move(callback));
}

void OnUrlHandlerIntentPickerDialogCompleted(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    base::OnceClosure on_urls_unhandled_cb,
    startup::FinalizeWebAppLaunchCallback app_launched_callback,
    bool accepted,
    absl::optional<UrlHandlerLaunchParams> selected_match) {
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

bool ShouldLaunchSavedChoice(
    const std::vector<UrlHandlerLaunchParams>& url_handler_matches) {
  return url_handler_matches.size() == 1 &&
         url_handler_matches.front().saved_choice ==
             UrlHandlerSavedChoice::kInApp;
}

void LaunchSavedChoice(
    const std::vector<UrlHandlerLaunchParams>& url_handler_matches,
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    startup::FinalizeWebAppLaunchCallback app_launched_callback) {
  // Default choice found. The first match returned is the saved choice,
  // which should be launched directly. Do not show the dialog.
  const UrlHandlerLaunchParams& saved_choice = url_handler_matches.front();
  LaunchApp(saved_choice.profile_path, saved_choice.app_id, command_line,
            cur_dir, saved_choice.url, std::move(app_launched_callback));
}

// Returns true if `url` is handled, either by launching an app or showing a
// dialog with all matching URL Handling apps. Returns false otherwise.
// If `should_process_unhandled_url` is true, `on_urls_unhandled_cb` will be
// run to handle `url`.
bool MaybeHandleUrl(
    const GURL& url,
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    bool should_process_unhandled_url,
    base::OnceClosure on_urls_unhandled_cb,
    startup::FinalizeWebAppLaunchCallback app_launched_callback) {
  auto matches = UrlHandlerManagerImpl::GetUrlHandlerMatches(url);
  if (matches.empty()) {
    if (should_process_unhandled_url)
      std::move(on_urls_unhandled_cb).Run();
    return false;
  }

  if (ShouldLaunchSavedChoice(matches)) {
    // TODO(crbug.com/1217419): Verify if site permission is enabled.
    LaunchSavedChoice(matches, command_line, cur_dir,
                      std::move(app_launched_callback));
    return true;
  }

  chrome::ShowWebAppUrlHandlerIntentPickerDialog(
      url, std::move(matches),
      base::BindOnce(&OnUrlHandlerIntentPickerDialogCompleted, command_line,
                     cur_dir, std::move(on_urls_unhandled_cb),
                     std::move(app_launched_callback)));
  return true;
}

}  // namespace

namespace startup {

bool MaybeLaunchUrlHandlerWebAppFromCmd(
    const base::CommandLine& command_line,
    const base::FilePath& cur_dir,
    base::OnceClosure on_urls_unhandled_cb,
    FinalizeWebAppLaunchCallback app_launched_callback) {
  absl::optional<GURL> url =
      UrlHandlerManagerImpl::GetUrlFromCommandLine(command_line);
  if (!url)
    return false;

  return MaybeHandleUrl(url.value(), command_line, cur_dir,
                        /*should_process_unhandled_url=*/false,
                        std::move(on_urls_unhandled_cb),
                        std::move(app_launched_callback));
}

void MaybeLaunchUrlHandlerWebAppFromUrls(
    const std::vector<GURL>& urls,
    base::OnceClosure on_urls_unhandled_cb,
    FinalizeWebAppLaunchCallback app_launched_callback) {
  if (urls.size() != 1) {
    std::move(on_urls_unhandled_cb).Run();
    return;
  }

  MaybeHandleUrl(urls.front(), base::CommandLine(base::CommandLine::NO_PROGRAM),
                 base::FilePath(), /*should_process_unhandled_url=*/true,
                 std::move(on_urls_unhandled_cb),
                 std::move(app_launched_callback));
}

}  // namespace startup
}  // namespace web_app
