// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_app_from_command_line.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/common/chrome_switches.h"
#include "components/webapps/browser/installable/installable_manager.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

void ReportInstallationResult(
    base::expected<InstallIsolatedAppCommandSuccess,
                   InstallIsolatedAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Isolated app auto installation "
                  "failed. Error: "
               << result.error();
  }
}

std::unique_ptr<content::WebContents> CreateWebContents(Profile& profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(
          /*context=*/&profile));

  webapps::InstallableManager::CreateForWebContents(web_contents.get());

  return web_contents;
}

void ScheduleInstallIsolatedApp(WebAppProvider& provider,
                                Profile& profile,
                                GURL url,
                                base::OnceClosure callback) {
  DCHECK(url.is_valid());
  DCHECK(!callback.is_null());

  provider.command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedAppCommand>(
          url,
          IsolationData{IsolationData::DevModeProxy{.proxy_url = url.spec()}},
          CreateWebContents(profile), std::make_unique<WebAppUrlLoader>(),
          provider.install_finalizer(),
          base::BindOnce(&ReportInstallationResult).Then(std::move(callback))));
}

void InstallApplicationFromUrl(WebAppProvider& provider,
                               Profile& profile,
                               GURL url,
                               base::OnceClosure callback) {
  DCHECK(url.is_valid());
  DCHECK(!callback.is_null());

  provider.on_registry_ready().Post(
      FROM_HERE, base::BindOnce(ScheduleInstallIsolatedApp, std::ref(provider),
                                std::ref(profile), url, std::move(callback)));
}

base::RepeatingCallback<void(GURL url, base::OnceClosure callback)>
CreateProductionInstallApplicationFromUrl(Profile& profile) {
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile);

  // Web applications are not available on some platform and
  // |WebAppProvider::GetForWebApps| returns nullptr in such cases.
  //
  // See |WebAppProvider::GetForWebApps| documentation for details.
  if (provider == nullptr) {
    return base::DoNothing();
  }

  return base::BindRepeating(InstallApplicationFromUrl, std::ref(*provider),
                             std::ref(profile));
}

base::OnceClosure& GetNextDoneCallbackInstance() {
  static base::NoDestructor<base::OnceClosure> kInstance{base::NullCallback()};
  return *kInstance;
}

}  // namespace

void SetNextInstallationDoneCallbackForTesting(  // IN-TEST
    base::OnceClosure done_callback) {
  GetNextDoneCallbackInstance() = std::move(done_callback);
}

absl::optional<GURL> GetAppToInstallFromCommandLine(
    const base::CommandLine& command_line) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedAppAtStartup);

  GURL url{switch_value};

  if (!url.is_valid()) {
    return absl::nullopt;
  }

  return url;
}

void MaybeInstallAppFromCommandLine(
    const base::CommandLine& command_line,
    base::RepeatingCallback<void(GURL url, base::OnceClosure callback)>
        install_application_from_url,
    base::OnceClosure done) {
  DCHECK(!done.is_null());

  absl::optional<GURL> app_to_install =
      GetAppToInstallFromCommandLine(command_line);
  if (!app_to_install.has_value()) {
    std::move(done).Run();
    return;
  }

  install_application_from_url.Run(*app_to_install, std::move(done));
}

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile) {
  base::OnceClosure& done = GetNextDoneCallbackInstance();

  MaybeInstallAppFromCommandLine(
      command_line, CreateProductionInstallApplicationFromUrl(profile),
      done.is_null() ? base::DoNothing() : std::move(done));
}

}  // namespace web_app
