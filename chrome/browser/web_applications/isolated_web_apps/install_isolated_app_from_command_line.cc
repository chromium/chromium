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
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_isolated_app_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_url_loader.h"
#include "chrome/common/chrome_switches.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {

namespace {

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

  return base::BindRepeating(
      [](WebAppProvider& provider, GURL url, base::OnceClosure callback) {
        DCHECK(!callback.is_null());

        provider.on_registry_ready().Post(
            FROM_HERE,
            base::BindOnce(
                [](WebAppProvider& provider, GURL url,
                   base::OnceClosure callback) {
                  std::unique_ptr<WebAppUrlLoader> url_loader =
                      std::make_unique<WebAppUrlLoader>();

                  WebAppUrlLoader& url_loader_ref = *url_loader;

                  base::OnceCallback<void(
                      base::expected<InstallIsolatedAppCommandSuccess,
                                     InstallIsolatedAppCommandError>)>
                      install_isolated_app_callback = base::BindOnce(
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
                          std::move(install_isolated_app_callback)
                              .Then(std::move(callback))));
                },
                std::ref(provider), url, std::move(callback)));
      },
      std::ref(*provider));
}

struct NextDoneCallbackHolder {
  base::OnceClosure Get() {
    auto value = std::move(next_done_callback_).value_or(base::DoNothing());
    next_done_callback_ = absl::nullopt;
    return value;
  }

  void Set(base::OnceClosure next_done_callback) {
    next_done_callback_ = std::move(next_done_callback);
  }

  static NextDoneCallbackHolder& GetInstance() {
    static base::NoDestructor<NextDoneCallbackHolder> kInstance{};
    return *kInstance;
  }

 private:
  absl::optional<base::OnceClosure> next_done_callback_;
};

}  // namespace

void SetNextInstallationDoneCallbackForTesting(  // IN-TEST
    base::OnceClosure done_callback) {
  NextDoneCallbackHolder::GetInstance().Set(std::move(done_callback));
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
  base::OnceClosure done = NextDoneCallbackHolder::GetInstance().Get();
  DCHECK(!done.is_null());

  MaybeInstallAppFromCommandLine(
      command_line, CreateProductionInstallApplicationFromUrl(profile),
      std::move(done));
}

}  // namespace web_app
