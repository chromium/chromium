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
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"
#include "chrome/browser/web_applications/isolation_data.h"
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

void ScheduleInstallIsolatedApp(GURL url,
                                IsolationData isolation_data,
                                WebAppProvider& provider,
                                Profile& profile,
                                base::OnceClosure callback) {
  DCHECK(url.is_valid());
  DCHECK(!callback.is_null());

  provider.command_manager().ScheduleCommand(
      std::make_unique<InstallIsolatedAppCommand>(
          url, isolation_data, CreateWebContents(profile),
          std::make_unique<WebAppUrlLoader>(), provider.install_finalizer(),
          base::BindOnce(&ReportInstallationResult).Then(std::move(callback))));
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

base::expected<absl::optional<IsolationData>, std::string>
GetIsolationDataFromCommandLine(const base::CommandLine& command_line) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedWebAppFromUrl);

  if (switch_value.empty()) {
    return absl::nullopt;
  }

  GURL url{switch_value};

  if (!url.is_valid()) {
    return base::unexpected(base::StrCat(
        {"Invalid URL provided to --", switches::kInstallIsolatedWebAppFromUrl,
         " flag: '", url.possibly_invalid_spec(), "'"}));
  }

  return IsolationData{IsolationData::DevModeProxy{.proxy_url = url.spec()}};
}

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile) {
  base::OnceClosure& next_done_callback = GetNextDoneCallbackInstance();
  base::OnceClosure done = next_done_callback.is_null()
                               ? base::DoNothing()
                               : std::move(next_done_callback);

  // Web applications are not available on some platforms and
  // |WebAppProvider::GetForWebApps| returns nullptr in such cases.
  //
  // See |WebAppProvider::GetForWebApps| documentation for details.
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile);
  if (provider == nullptr) {
    std::move(done).Run();
    return;
  }

  base::expected<absl::optional<IsolationData>, std::string> isolation_data =
      GetIsolationDataFromCommandLine(command_line);
  if (!isolation_data.has_value()) {
    LOG(ERROR) << isolation_data.error();
    std::move(done).Run();
    return;
  }
  if (!isolation_data->has_value()) {
    std::move(done).Run();
    return;
  }

  // TODO(b/245352649): Replace with randomly generated isolated-app: URL.
  GURL url(absl::get<IsolationData::DevModeProxy>(
               isolation_data.value().value().content)
               .proxy_url);

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&ScheduleInstallIsolatedApp, url, **isolation_data,
                     std::ref(*provider), std::ref(profile), std::move(done)));
}

}  // namespace web_app
