// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

namespace web_app {

namespace {

base::RepeatingCallback<void(GURL url)>
CreateProductionInstallApplicationFromUrl(Profile& profile) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile);

  // Web applications are not available on some platform and
  // |WebAppProvider::GetForWebApps| returns nullptr in such cases.
  //
  // See |WebAppProvider::GetForWebApps| documentation for details.
  if (provider == nullptr) {
    return base::DoNothing();
  }

  return base::BindRepeating(
      [](WebAppProvider& provider, GURL url) {
        provider.on_registry_ready().Post(
            FROM_HERE,
            base::BindOnce(
                [](WebAppProvider& provider, GURL url) {
                  std::unique_ptr<WebAppUrlLoader> url_loader =
                      std::make_unique<WebAppUrlLoader>();

                  WebAppUrlLoader& url_loader_ref = *url_loader;

                  base::OnceCallback<void(
                      base::expected<InstallIsolatedAppCommandSuccess,
                                     InstallIsolatedAppCommandError>)>
                      callback = base::BindOnce(
                          [](std::unique_ptr<WebAppUrlLoader> url_loader,
                             base::expected<InstallIsolatedAppCommandSuccess,
                                            InstallIsolatedAppCommandError>
                                 result) {
                            if (!result.has_value()) {
                              LOG(ERROR) << "Isolated app auto installation "
                                            "failed. Error: "
                                         << result.error();
                            }
                          },
                          std::move(url_loader));

                  provider.command_manager().ScheduleCommand(
                      std::make_unique<InstallIsolatedAppCommand>(
                          url, url_loader_ref, provider.install_finalizer(),
                          std::move(callback)));
                },
                std::ref(provider), url));
      },
      std::ref(*provider));
}

}  // namespace

std::vector<GURL> GetAppsToInstallFromCommandLine(
    const base::CommandLine& command_line) {
  std::vector<std::string> switch_values = base::SplitString(
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedAppsAtStartup),
      ",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  std::vector<GURL> app_urls;
  for (const std::string& switch_value : switch_values) {
    GURL app_url{switch_value};
    if (app_url.is_valid()) {
      app_urls.push_back(app_url);
    } else {
      LOG(ERROR) << "Invalid application URL: \"" << switch_value << "\"";
    }
  }
  return app_urls;
}

void MaybeInstallAppFromCommandLine(
    const base::CommandLine& command_line,
    base::RepeatingCallback<void(GURL url)> install_application_from_url) {
  for (const GURL& url : GetAppsToInstallFromCommandLine(command_line)) {
    install_application_from_url.Run(url);
  }
}

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile) {
  MaybeInstallAppFromCommandLine(
      command_line, CreateProductionInstallApplicationFromUrl(profile));
}

}  // namespace web_app
