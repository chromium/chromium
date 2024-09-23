// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_installation_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/overloaded.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/garbage_collect_storage_partitions_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using MaybeIwaInstallSource =
    IsolatedWebAppInstallationManager::MaybeIwaInstallSource;

void OnGetBundlePathFromCommandLine(
    base::OnceCallback<void(MaybeIwaInstallSource)> callback,
    MaybeIwaInstallSource proxy_url,
    MaybeIwaInstallSource bundle_path) {
  bool is_proxy_url_set = !proxy_url.has_value() || proxy_url->has_value();
  bool is_bundle_path_set =
      !bundle_path.has_value() || bundle_path->has_value();
  if (!is_proxy_url_set && !is_bundle_path_set) {
    std::move(callback).Run(std::nullopt);
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
    base::OnceCallback<void(MaybeIwaInstallSource)> callback) {
  base::FilePath switch_value =
      command_line.GetSwitchValuePath(switches::kInstallIsolatedWebAppFromFile);

  if (switch_value.empty()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          [](base::FilePath switch_value) -> MaybeIwaInstallSource {
            base::FilePath absolute_path =
                base::MakeAbsoluteFilePath(switch_value);

            if (!base::PathExists(absolute_path) ||
                base::DirectoryExists(absolute_path)) {
              return base::unexpected(
                  base::StrCat({"Invalid path provided to --",
                                switches::kInstallIsolatedWebAppFromFile,
                                " flag: '", switch_value.AsUTF8Unsafe(), "'"}));
            }

            return IsolatedWebAppInstallSource::FromDevCommandLine(
                IwaSourceBundleDevModeWithFileOp(absolute_path,
                                                 kDefaultBundleDevFileOp));
          },
          std::move(switch_value)),
      std::move(callback));
}

base::expected<url::Origin, std::string> ValidateProxyOrigin(const GURL& gurl) {
  if (!gurl.SchemeIsHTTPOrHTTPS()) {
    return base::unexpected(base::StrCat(
        {"Proxy URL must be HTTP or HTTPS: ", gurl.possibly_invalid_spec()}));
  }

  if (gurl.path() != "/") {
    return base::unexpected(base::StrCat(
        {"Non-origin URL provided: '", gurl.possibly_invalid_spec()}));
  }

  url::Origin url_origin = url::Origin::Create(gurl);
  if (!network::IsUrlPotentiallyTrustworthy(gurl)) {
    return base::unexpected(base::StrCat(
        {"Proxy URL not trustworthy: ", gurl.possibly_invalid_spec()}));
  }
  return url_origin;
}

MaybeIwaInstallSource GetProxyUrlFromCommandLine(
    const base::CommandLine& command_line) {
  std::string switch_value =
      command_line.GetSwitchValueASCII(switches::kInstallIsolatedWebAppFromUrl);
  if (switch_value.empty()) {
    return std::nullopt;
  }
  return ValidateProxyOrigin(GURL(switch_value))
      .transform([](url::Origin proxy_url) {
        return IsolatedWebAppInstallSource::FromDevCommandLine(
            IwaSourceProxy(proxy_url));
      });
}

}  // namespace

IsolatedWebAppInstallationManager::IsolatedWebAppInstallationManager(
    Profile& profile)
    : profile_(profile) {}

IsolatedWebAppInstallationManager::~IsolatedWebAppInstallationManager() =
    default;

void IsolatedWebAppInstallationManager::SetProvider(
    base::PassKey<WebAppProvider>,
    WebAppProvider& provider) {
  provider_ = &provider;
}

void IsolatedWebAppInstallationManager::Start() {
  MaybeScheduleGarbageCollection();
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

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeProxy(
    const GURL& gurl,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());

  // Ensure the URL we're given is okay.
  base::expected<url::Origin, std::string> proxy_origin =
      ValidateProxyOrigin(gurl);
  if (!proxy_origin.has_value()) {
    std::move(callback).Run(base::unexpected(proxy_origin.error()));
    return;
  }

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(*proxy_origin, install_surface), std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeBundle(
    const base::FilePath& path,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(path, install_surface), std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeBundle(
    const base::ScopedTempFile* file,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(file, install_surface), std::move(callback));
}

// static
bool IsolatedWebAppInstallationManager::HasIwaInstallSwitch(
    const base::CommandLine& command_line) {
  return command_line.HasSwitch(switches::kInstallIsolatedWebAppFromUrl) ||
         command_line.HasSwitch(switches::kInstallIsolatedWebAppFromFile);
}

// static
void IsolatedWebAppInstallationManager::MaybeInstallIwaFromCommandLine(
    const base::CommandLine& command_line,
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
            provider.isolated_web_app_installation_manager()
                .InstallFromCommandLine(
                    command_line, std::move(keep_alive),
                    std::move(optional_profile_keep_alive),
                    // Use higher task priority here since the user may be
                    // actively waiting for the installation to finish. Also,
                    // using `base::TaskPriority::BEST_EFFORT` will not work if
                    // the installation is triggered in combination with
                    // `--no-startup-window`.
                    base::TaskPriority::USER_VISIBLE);
          },
          std::ref(*provider), command_line, std::move(keep_alive),
          std::move(optional_profile_keep_alive)));
}

// static
void IsolatedWebAppInstallationManager::
    GetIsolatedWebAppInstallSourceFromCommandLine(
        const base::CommandLine& command_line,
        base::OnceCallback<void(MaybeIwaInstallSource)> callback) {
  MaybeIwaInstallSource proxy_url = GetProxyUrlFromCommandLine(command_line);

  GetBundlePathFromCommandLine(
      command_line, base::BindOnce(&OnGetBundlePathFromCommandLine,
                                   std::move(callback), std::move(proxy_url)));
}

// static
IsolatedWebAppInstallSource
IsolatedWebAppInstallationManager::CreateInstallSource(
    absl::variant<base::FilePath, const base::ScopedTempFile*, url::Origin>
        source,
    InstallSurface surface) {
  switch (surface) {
    case InstallSurface::kDevUi:
      return IsolatedWebAppInstallSource::FromDevUi(absl::visit(
          base::Overloaded{
              [](base::FilePath path) -> IwaSourceDevModeWithFileOp {
                return IwaSourceBundleDevModeWithFileOp(
                    std::move(path), kDefaultBundleDevFileOp);
              },
              [](const base::ScopedTempFile* temp_file)
                  -> IwaSourceDevModeWithFileOp {
                return IwaSourceBundleDevModeWithFileOp(
                    temp_file->path(), IwaSourceBundleDevFileOp::kMove);
              },
              [](url::Origin proxy_url) -> IwaSourceDevModeWithFileOp {
                return IwaSourceProxy(std::move(proxy_url));
              }},
          std::move(source)));
  }
}

void IsolatedWebAppInstallationManager::InstallFromCommandLine(
    const base::CommandLine& command_line,
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    base::TaskPriority task_priority) {
  CHECK(keep_alive);

  if (!HasIwaInstallSwitch(command_line)) {
    return;
  }
  content::GetUIThreadTaskRunner({task_priority})
      ->PostTask(
          FROM_HERE,
          base::BindOnce(
              &GetIsolatedWebAppInstallSourceFromCommandLine, command_line,
              base::BindOnce(
                  &IsolatedWebAppInstallationManager::
                      OnGetIsolatedWebAppInstallSourceFromCommandLine,
                  weak_ptr_factory_.GetWeakPtr(), std::move(keep_alive),
                  std::move(optional_profile_keep_alive))));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromInstallSource(
    MaybeIwaInstallSource install_source,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    // If the browser is shutting down, then there is no point in attempting to
    // install an IWA.
    std::move(callback).Run(base::unexpected("Browser is shutting down"));
    return;
  }

  if (profile_->IsOffTheRecord()) {
    std::move(callback).Run(
        base::unexpected(std::string("incognito profiles are not supported")));
    return;
  }

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  auto optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      &profile_.get(), ProfileKeepAliveOrigin::kIsolatedWebAppInstall);
  InstallIsolatedWebAppFromInstallSource(std::move(keep_alive),
                                         std::move(optional_profile_keep_alive),
                                         install_source, std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromInstallSource(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    MaybeIwaInstallSource install_source,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  ASSIGN_OR_RETURN(
      std::optional<IsolatedWebAppInstallSource> optional_install_source,
      install_source, [&](std::string error) {
        std::move(callback).Run(base::unexpected(std::move(error)));
      });
  if (!optional_install_source.has_value()) {
    return;
  }

  if (!IsIwaDevModeEnabled(&*profile_)) {
    std::move(callback).Run(
        base::unexpected(std::string(kIwaDevModeNotEnabledMessage)));
    return;
  }

  IsolatedWebAppUrlInfo::CreateFromIsolatedWebAppSource(
      optional_install_source->source(),
      base::BindOnce(
          &IsolatedWebAppInstallationManager::OnGetIsolatedWebAppUrlInfo,
          weak_ptr_factory_.GetWeakPtr(), std::move(keep_alive),
          std::move(optional_profile_keep_alive), *optional_install_source,
          std::move(callback)));
}

void IsolatedWebAppInstallationManager::
    OnGetIsolatedWebAppInstallSourceFromCommandLine(
        std::unique_ptr<ScopedKeepAlive> keep_alive,
        std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
        MaybeIwaInstallSource install_source) {
  InstallIsolatedWebAppFromInstallSource(
      std::move(keep_alive), std::move(optional_profile_keep_alive),
      std::move(install_source),
      base::BindOnce(
          &IsolatedWebAppInstallationManager::ReportInstallationResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppInstallationManager::OnGetIsolatedWebAppUrlInfo(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    const IsolatedWebAppInstallSource& install_source,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  RETURN_IF_ERROR(url_info, [&](std::string error) {
    std::move(callback).Run(
        base::unexpected("Failed to get IsolationInfo: " + std::move(error)));
  });

  provider_->scheduler().InstallIsolatedWebApp(
      url_info.value(), install_source,
      /*expected_version=*/std::nullopt, std::move(keep_alive),
      std::move(optional_profile_keep_alive),
      base::BindOnce(
          &IsolatedWebAppInstallationManager::OnInstallIsolatedWebApp,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IsolatedWebAppInstallationManager::OnInstallIsolatedWebApp(
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  std::move(callback).Run(
      result.transform_error([](auto error) { return error.message; }));
}

void IsolatedWebAppInstallationManager::ReportInstallationResult(
    MaybeInstallIsolatedWebAppCommandSuccess result) {
  if (result.has_value()) {
    LOG(WARNING) << "Isolated Web App command line installation successful. "
                    "Installed version "
                 << result->installed_version.GetString() << ".";
  } else {
    LOG(ERROR) << "Isolated Web App command line installation failed: "
               << result.error();
  }
  on_report_installation_result_.Run(std::move(result));
}

void IsolatedWebAppInstallationManager::MaybeScheduleGarbageCollection() {
  // We are migrating from `ExtensionsPref::kStorageGarbageCollect` to
  // `prefs::kShouldGarbageCollectStoragePartitions`. During the migration,
  // either one of the prefs can trigger garbage collection.
  // TODO(crbug.com/40922689): Delete `ExtensionsPref::kStorageGarbageCollect`.
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kShouldGarbageCollectStoragePartitions) ||
      provider_->extensions_manager().ShouldGarbageCollectStoragePartitions()) {
    provider_->command_manager().ScheduleCommand(
        std::make_unique<web_app::GarbageCollectStoragePartitionsCommand>(
            &profile_.get(),
            base::BindOnce(
                [](base::WeakPtr<IsolatedWebAppInstallationManager> weak_this) {
                  if (!weak_this) {
                    return;
                  }
                  weak_this
                      ->on_garbage_collect_storage_partitions_done_for_testing_
                      .Signal();
                },
                weak_ptr_factory_.GetWeakPtr())));
  }
}

}  // namespace web_app
