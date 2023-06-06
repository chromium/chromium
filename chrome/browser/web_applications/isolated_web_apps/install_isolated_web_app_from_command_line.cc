// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_from_command_line.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_dev_mode.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_location.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using MaybeIwaLocation =
    base::expected<absl::optional<IsolatedWebAppLocation>, std::string>;

void OnGetBundlePathFromCommandLine(
    base::OnceCallback<void(MaybeIwaLocation)> callback,
    MaybeIwaLocation proxy_url,
    MaybeIwaLocation bundle_path) {
  bool is_proxy_url_set = !proxy_url.has_value() || proxy_url->has_value();
  bool is_bundle_path_set =
      !bundle_path.has_value() || bundle_path->has_value();
  if (!is_proxy_url_set && !is_bundle_path_set) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  if (is_proxy_url_set && is_bundle_path_set) {
    std::move(callback).Run(base::unexpected(
        base::StrCat({"--", switches::kInstallIsolatedWebAppFromUrl, " and --",
                      switches::kInstallIsolatedWebAppFromFile,
                      " cannot both be provided."})));
    return;
  }

  std::move(callback).Run(is_proxy_url_set ? proxy_url : bundle_path);
}

void GetBundlePathFromCommandLine(
    const base::CommandLine& command_line,
    base::OnceCallback<void(MaybeIwaLocation)> callback) {
  base::FilePath switch_value =
      command_line.GetSwitchValuePath(switches::kInstallIsolatedWebAppFromFile);

  if (switch_value.empty()) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::FilePath switch_value) -> MaybeIwaLocation {
            base::FilePath absolute_path =
                base::MakeAbsoluteFilePath(switch_value);

            if (!base::PathExists(absolute_path) ||
                base::DirectoryExists(absolute_path)) {
              return base::unexpected(
                  base::StrCat({"Invalid path provided to --",
                                switches::kInstallIsolatedWebAppFromFile,
                                " flag: '", switch_value.AsUTF8Unsafe(), "'"}));
            }

            return DevModeBundle{.path = absolute_path};
          },
          std::move(switch_value)),
      std::move(callback));
}

MaybeIwaLocation GetProxyUrlFromCommandLine(
    const base::CommandLine& command_line) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedWebAppFromUrl);

  if (switch_value.empty()) {
    return absl::nullopt;
  }

  GURL url{switch_value};
  url::Origin url_origin = url::Origin::Create(url);

  if (!url.is_valid() || url_origin.opaque()) {
    return base::unexpected(base::StrCat(
        {"Invalid URL provided to --", switches::kInstallIsolatedWebAppFromUrl,
         " flag: '", url.possibly_invalid_spec(), "'"}));
  }

  if (url_origin.GetURL() != url) {
    return base::unexpected(base::StrCat(
        {"Non-origin URL provided to --",
         switches::kInstallIsolatedWebAppFromUrl, " flag: '",
         url.possibly_invalid_spec(), "'", ". Possible origin URL: '",
         url_origin.Serialize(), "'."}));
  }

  return DevModeProxy{.proxy_url = url_origin};
}

}  // namespace

bool HasIwaInstallSwitch(const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kInstallIsolatedWebAppFromUrl) ||
         command_line.HasSwitch(switches::kInstallIsolatedWebAppFromFile);
}

void GetIsolatedWebAppLocationFromCommandLine(
    const base::CommandLine& command_line,
    base::OnceCallback<void(MaybeIwaLocation)> callback) {
  MaybeIwaLocation proxy_url = GetProxyUrlFromCommandLine(command_line);

  GetBundlePathFromCommandLine(
      command_line, base::BindOnce(&OnGetBundlePathFromCommandLine,
                                   std::move(callback), std::move(proxy_url)));
}

IsolatedWebAppCommandLineInstallManager::
    IsolatedWebAppCommandLineInstallManager(Profile& profile)
    : profile_(profile) {}

IsolatedWebAppCommandLineInstallManager::
    ~IsolatedWebAppCommandLineInstallManager() = default;

void IsolatedWebAppCommandLineInstallManager::SetSubsystems(
    WebAppCommandScheduler* command_scheduler) {
  command_scheduler_ = command_scheduler;
}

void IsolatedWebAppCommandLineInstallManager::Start() {
#if BUILDFLAG(IS_CHROMEOS)
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  if (!HasIwaInstallSwitch(command_line)) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsWebAppsCrosapiEnabled()) {
    // If Lacros manages Web Apps, then Ash only manages System Web Apps. Thus,
    // do not attempt to install IWAs in Ash, because Lacros will take care of
    // that.
    return;
  }
#endif

  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    ReportInstallationResult(base::unexpected(
        "Unable to install IWA due to browser shutting down."));
    return;
  }
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive;
  if (!profile_->IsOffTheRecord()) {
    optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        &*profile_, ProfileKeepAliveOrigin::kIsolatedWebAppInstall);
  }

  InstallFromCommandLine(command_line, std::move(keep_alive),
                         std::move(optional_profile_keep_alive),
                         base::TaskPriority::BEST_EFFORT);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void IsolatedWebAppCommandLineInstallManager::Shutdown() {
  // Avoid dangling pointer error on destruction of the `WebAppProvider` by
  // removing our reference to the command scheduler.
  command_scheduler_ = nullptr;
}

void IsolatedWebAppCommandLineInstallManager::InstallFromCommandLine(
    const base::CommandLine& command_line,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::TaskPriority task_priority) {
  CHECK(keep_alive);

  if (!HasIwaInstallSwitch(command_line)) {
    return;
  }
  content::GetUIThreadTaskRunner({task_priority})
      ->PostTask(FROM_HERE,
                 base::BindOnce(
                     &GetIsolatedWebAppLocationFromCommandLine, command_line,
                     base::BindOnce(
                         &IsolatedWebAppCommandLineInstallManager::
                             OnGetIsolatedWebAppLocationFromCommandLine,
                         weak_ptr_factory_.GetWeakPtr(), std::move(keep_alive),
                         std::move(optional_profile_keep_alive))));
}

void IsolatedWebAppCommandLineInstallManager::
    OnGetIsolatedWebAppLocationFromCommandLine(
        std::unique_ptr<ScopedKeepAlive> keep_alive,
        std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
        MaybeIwaLocation location) {
  // Check the base::expected.
  if (!location.has_value()) {
    ReportInstallationResult(base::unexpected(location.error()));
    return;
  }
  // Check the absl::optional.
  if (!location->has_value()) {
    return;
  }

  if (!IsIwaDevModeEnabled(*profile_->GetPrefs())) {
    ReportInstallationResult(
        base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
    return;
  }

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppLocation(
      **location,
      base::BindOnce(
          &IsolatedWebAppCommandLineInstallManager::OnGetIsolatedWebAppUrlInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(keep_alive),
          std::move(optional_profile_keep_alive), **location));
}

void IsolatedWebAppCommandLineInstallManager::OnGetIsolatedWebAppUrlInfo(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    const IsolatedWebAppLocation& location,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  if (!url_info.has_value()) {
    ReportInstallationResult(
        base::unexpected("Failed to get IsolationInfo: " + url_info.error()));
    return;
  }

  command_scheduler_->InstallIsolatedWebApp(
      url_info.value(), location,
      /*expected_version=*/absl::nullopt, std::move(keep_alive),
      std::move(optional_profile_keep_alive),
      base::BindOnce(
          &IsolatedWebAppCommandLineInstallManager::OnInstallIsolatedWebApp,
          weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppCommandLineInstallManager::OnInstallIsolatedWebApp(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  ReportInstallationResult(
      result.transform_error([](auto error) { return error.message; }));
}

void IsolatedWebAppCommandLineInstallManager::ReportInstallationResult(
    base::expected<InstallIsolatedWebAppCommandSuccess, std::string> result) {
  if (result.has_value()) {
    LOG(WARNING) << "Isolated Web App command line installation successful.";
  } else {
    LOG(ERROR) << "Isolated Web App command line installation failed: "
               << result.error();
  }
  on_report_installation_result_.Run(std::move(result));
}

void MaybeInstallIwaFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile) {
  if (!HasIwaInstallSwitch(command_line)) {
    // Early-exit for better performance when none of the IWA-specific command
    // line switches are present
    return;
  }

  // Web applications are not available on some platforms and
  // `WebAppProvider::GetForWebApps` returns `nullptr` in such cases.
  //
  // See the `WebAppProvider::GetForWebApps` documentation for details.
  WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile);
  if (provider == nullptr) {
    return;
  }

  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    // If the browser is shutting down, then there is no point in attempting to
    // install an IWA.
    LOG(ERROR) << "Isolated Web App command line installation failed: Browser "
                  "is shutting down.";
    return;
  }
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive;
  if (!profile.IsOffTheRecord()) {
    optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        &profile, ProfileKeepAliveOrigin::kIsolatedWebAppInstall);
  }

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(
          [](web_app::WebAppProvider& provider, base::CommandLine command_line,
             std::unique_ptr<ScopedKeepAlive> keep_alive,
             std::unique_ptr<ScopedProfileKeepAlive>
                 optional_profile_keep_alive) {
            provider.iwa_command_line_install_manager().InstallFromCommandLine(
                command_line, std::move(keep_alive),
                std::move(optional_profile_keep_alive),
                // Use higher task priority here since the user may be actively
                // waiting for the installation to finish. Also, using
                // `base::TaskPriority::BEST_EFFORT` will not work if the
                // installation is triggered in combination with
                // `--no-startup-window`.
                base::TaskPriority::USER_VISIBLE);
          },
          std::ref(*provider), command_line, std::move(keep_alive),
          std::move(optional_profile_keep_alive)));
}

}  // namespace web_app
