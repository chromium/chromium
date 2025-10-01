// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_installation_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/garbage_collect_storage_partitions_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
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
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
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
                IwaSourceBundleDevModeWithFileOp(
                    absolute_path, IwaSourceBundleDevFileOp::kCopy));
          },
          std::move(switch_value)),
      std::move(callback));
}

base::expected<url::Origin, std::string> ValidateProxyOrigin(const GURL& gurl) {
  if (!gurl.SchemeIsHTTPOrHTTPS()) {
    return base::unexpected(base::StrCat(
        {"Proxy URL must be HTTP or HTTPS: ", gurl.possibly_invalid_spec()}));
  }

  if (gurl.GetPath() != "/") {
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

constexpr auto kDownloadAnnotationDevToolsProtocol =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_dev_tools_installation_web_bundle",
        "iwa_bundle_downloader",
        R"(
    semantics {
      sender: "DevTools PWA.install"
      description:
        "Downloads a Signed Web Bundle of an Isolated Web App which contains "
        "code and other resources of this app."
      trigger:
        "User triggers PWA.install CDT command."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      policy_exception_justification: "Not implemented."
    })");

constexpr auto kDownloadAnnotationDevUi =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_web_app_internals_web_bundle",
        "iwa_bundle_downloader",
        R"(
    semantics {
      sender: "Web App Internals page"
      description:
        "Downloads a Signed Web Bundle of an Isolated Web App which contains "
        "code and other resources of this app."
      trigger:
        "User accepts the installation dialog in chrome://web-app-internals."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      policy_exception_justification: "Not implemented."
    })");

net::PartialNetworkTrafficAnnotationTag GetDownloadAnnotationTag(
    IsolatedWebAppInstallationManager::InstallSurface install_surface) {
  switch (install_surface) {
    case IsolatedWebAppInstallationManager::InstallSurface::kDevUi:
      return kDownloadAnnotationDevUi;
    case IsolatedWebAppInstallationManager::InstallSurface::kDevToolsProtocol:
      return kDownloadAnnotationDevToolsProtocol;
  }
}
}  // namespace

IsolatedWebAppInstallationManager::IsolatedWebAppInstallationManager(
    Profile& profile)
    : profile_(profile),
      are_isolated_web_apps_enabled_(
          content::AreIsolatedWebAppsEnabled(&profile)) {}

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
  if (!are_isolated_web_apps_enabled_ || !HasIwaInstallSwitch(command_line)) {
    return;
  }

  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    ReportInstallationResult(base::unexpected(
        "Unable to install IWA due to browser shutting down."));
    return;
  }
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive;
  optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      &*profile_, ProfileKeepAliveOrigin::kIsolatedWebAppInstall);

  InstallFromCommandLine(command_line, std::move(keep_alive),
                         std::move(optional_profile_keep_alive),
                         base::TaskPriority::BEST_EFFORT);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeProxy(
    const GURL& gurl,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    std::optional<web_package::SignedWebBundleId> explicit_bundle_id) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());
  if (!are_isolated_web_apps_enabled_) {
    std::move(callback).Run(base::unexpected("IWAs are not enabled"));
    return;
  }
  if (explicit_bundle_id && !explicit_bundle_id->is_for_proxy_mode()) {
    std::move(callback).Run(
        base::unexpected("The bundle_id for devModeProxy installation must "
                         "be of proxy_mode type"));
    return;
  }

  // Ensure the URL we're given is okay.
  base::expected<url::Origin, std::string> proxy_origin =
      ValidateProxyOrigin(gurl);
  if (!proxy_origin.has_value()) {
    std::move(callback).Run(base::unexpected(proxy_origin.error()));
    return;
  }

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(
          IwaSourceProxy(*proxy_origin, std::move(explicit_bundle_id)),
          install_surface),
      /*expected_bundle_id=*/std::nullopt, std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeBundle(
    const base::FilePath& path,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());
  if (!are_isolated_web_apps_enabled_) {
    std::move(callback).Run(base::unexpected("IWAs are not enabled"));
    return;
  }

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(path, install_surface), std::move(expected_bundle_id),
      std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromDevModeBundle(
    const base::ScopedTempFile* file,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());
  if (!are_isolated_web_apps_enabled_) {
    std::move(callback).Run(base::unexpected("IWAs are not enabled"));
    return;
  }

  InstallIsolatedWebAppFromInstallSource(
      CreateInstallSource(file, install_surface), std::move(expected_bundle_id),
      std::move(callback));
}

void IsolatedWebAppInstallationManager::
    DownloadAndInstallIsolatedWebAppFromDevModeBundle(
        const GURL& url,
        InstallSurface install_surface,
        base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
            callback,
        std::optional<web_package::SignedWebBundleId> expected_bundle_id) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK(!callback.is_null());
  CHECK(url.SchemeIsHTTPOrHTTPS());
  if (!are_isolated_web_apps_enabled_) {
    std::move(callback).Run(base::unexpected("IWAs are not enabled"));
    return;
  }

  ScopedTempWebBundleFile::Create(base::BindOnce(
      &IsolatedWebAppInstallationManager::DownloadWebBundleToFile,
      weak_ptr_factory_.GetWeakPtr(), url, std::move(install_surface),
      std::move(callback), std::move(expected_bundle_id)));
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
  if (!content::AreIsolatedWebAppsEnabled(&profile) ||
      !HasIwaInstallSwitch(command_line)) {
    // Early-exit for better performance when the IWAs are not enabled or none
    // of the IWA-specific command line switches are present
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
  optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      &profile, ProfileKeepAliveOrigin::kIsolatedWebAppInstall);

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
    std::variant<base::FilePath, const base::ScopedTempFile*, IwaSourceProxy>
        source,
    InstallSurface surface) {
  switch (surface) {
    case InstallSurface::kDevUi:
    case InstallSurface::kDevToolsProtocol:
      return IsolatedWebAppInstallSource::FromDevUi(std::visit(
          absl::Overload{
              [](base::FilePath path) -> IwaSourceDevModeWithFileOp {
                return IwaSourceBundleDevModeWithFileOp(
                    std::move(path), IwaSourceBundleDevFileOp::kCopy);
              },
              [](const base::ScopedTempFile* temp_file)
                  -> IwaSourceDevModeWithFileOp {
                return IwaSourceBundleDevModeWithFileOp(
                    temp_file->path(), IwaSourceBundleDevFileOp::kMove);
              },
              [](IwaSourceProxy proxy) -> IwaSourceDevModeWithFileOp {
                return proxy;
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
    std::optional<web_package::SignedWebBundleId> expected_bundle_id,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)>
        callback) {
  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    // If the browser is shutting down, then there is no point in attempting to
    // install an IWA.
    std::move(callback).Run(base::unexpected("Browser is shutting down"));
    return;
  }

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_INSTALL,
      KeepAliveRestartOption::DISABLED);
  auto optional_profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      &profile_.get(), ProfileKeepAliveOrigin::kIsolatedWebAppInstall);
  InstallIsolatedWebAppFromInstallSource(
      std::move(keep_alive), std::move(optional_profile_keep_alive),
      std::move(expected_bundle_id), install_source, std::move(callback));
}

void IsolatedWebAppInstallationManager::InstallIsolatedWebAppFromInstallSource(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id,
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
          std::move(optional_profile_keep_alive), std::move(expected_bundle_id),
          *optional_install_source, std::move(callback)));
}

void IsolatedWebAppInstallationManager::
    OnGetIsolatedWebAppInstallSourceFromCommandLine(
        std::unique_ptr<ScopedKeepAlive> keep_alive,
        std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
        MaybeIwaInstallSource install_source) {
  InstallIsolatedWebAppFromInstallSource(
      std::move(keep_alive), std::move(optional_profile_keep_alive),
      /*expected_bundle_id=*/std::nullopt, std::move(install_source),
      base::BindOnce(
          &IsolatedWebAppInstallationManager::ReportInstallationResult,
          weak_ptr_factory_.GetWeakPtr()));
}

void IsolatedWebAppInstallationManager::OnGetIsolatedWebAppUrlInfo(
    std::unique_ptr<ScopedKeepAlive> keep_alive,
    std::unique_ptr<ScopedProfileKeepAlive> optional_profile_keep_alive,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id,
    const IsolatedWebAppInstallSource& install_source,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    base::expected<IsolatedWebAppUrlInfo, std::string> url_info) {
  RETURN_IF_ERROR(url_info, [&](std::string error) {
    std::move(callback).Run(
        base::unexpected("Failed to get IsolationInfo: " + std::move(error)));
  });

  if (expected_bundle_id.has_value() &&
      url_info->web_bundle_id() != expected_bundle_id) {
    std::move(callback).Run(base::unexpected(base::StringPrintf(
        "Web bundle id mismatch. Expected = %s. Actual = %s",
        expected_bundle_id->id(), url_info->web_bundle_id().id())));
    return;
  }

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
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kShouldGarbageCollectStoragePartitions)) {
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

void IsolatedWebAppInstallationManager::DownloadWebBundleToFile(
    const GURL& web_bundle_url,
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id,
    ScopedTempWebBundleFile bundle) {
  base::FilePath path = bundle.path();
  auto downloader = std::make_unique<IsolatedWebAppDownloader>(
      profile()->GetURLLoaderFactory());
  auto* downloader_ptr = downloader.get();
  base::OnceClosure downloader_keep_alive =
      base::DoNothingWithBoundArgs(std::move(downloader));

  downloader_ptr->DownloadSignedWebBundle(
      web_bundle_url, std::move(path),
      GetDownloadAnnotationTag(install_surface),
      base::BindOnce(&IsolatedWebAppInstallationManager::OnWebBundleDownloaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(install_surface),
                     std::move(callback), std::move(expected_bundle_id),
                     std::move(bundle))
          .Then(std::move(downloader_keep_alive)));
}

void IsolatedWebAppInstallationManager::OnWebBundleDownloaded(
    InstallSurface install_surface,
    base::OnceCallback<void(MaybeInstallIsolatedWebAppCommandSuccess)> callback,
    std::optional<web_package::SignedWebBundleId> expected_bundle_id,
    ScopedTempWebBundleFile bundle,
    int32_t result) {
  if (result != net::OK) {
    std::move(callback).Run(base::unexpected(
        base::StrCat({"Network error while downloading bundle file: ",
                      base::ToString(result)})));
    return;
  }

  const base::ScopedTempFile* file = bundle.file();
  base::OnceClosure bundle_keep_alive =
      base::DoNothingWithBoundArgs(std::move(bundle));

  WebAppProvider::GetForWebApps(profile())
      ->isolated_web_app_installation_manager()
      .InstallIsolatedWebAppFromDevModeBundle(
          file, std::move(install_surface),
          std::move(callback).Then(std::move(bundle_keep_alive)),
          std::move(expected_bundle_id));
}

}  // namespace web_app
