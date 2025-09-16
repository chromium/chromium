// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"

#include <memory>

#include "base/check_deref.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/histogram_functions.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/copy_bundle_to_cache_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/get_bundle_cache_path_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_cache_client.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/webapps/isolated_web_apps/error/uma_logging.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace web_app {

namespace {
constexpr auto kUpdateManifestFetchTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("iwa_policy_update_manifest",
                                               "iwa_update_manifest_fetcher",
                                               R"(
  semantics {
    sender: "Isolated Web App Policy Manager"
    description:
      "Downloads the update manifest of an Isolated Web App that is provided "
      "in an enterprise policy by the administrator. The update manifest "
      "contains at least the list of the available versions of the IWA "
      "and the URL to the Signed Web Bundles that correspond to each version."
    trigger:
      "Installation/update of a IWA from the enterprise policy requires "
      "fetching of a IWA Update Manifest"
  }
  policy {
    setting: "This feature cannot be disabled in settings."
    chrome_policy {
      IsolatedWebAppInstallForceList {
        IsolatedWebAppInstallForceList: ""
      }
    }
  })");

constexpr auto kWebBundleDownloadTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation("iwa_policy_signed_web_bundle",
                                               "iwa_bundle_downloader",
                                               R"(
  semantics {
    sender: "Isolated Web App Policy Manager"
    description:
      "Downloads the Signed Web Bundle of an Isolated Web App (IWA) from the "
      "URL read from an Update Manifest that is provided in an enterprise "
      "policy by the administrator. The Signed Web Bundle contains code and "
      "other resources of the IWA."
    trigger:
      "An Isolated Web App is installed from an enterprise policy."
  }
  policy {
    setting: "This feature cannot be disabled in settings."
    chrome_policy {
      IsolatedWebAppInstallForceList {
        IsolatedWebAppInstallForceList: ""
      }
    }
  })");

IsolatedWebAppInstallSource GetIsolatedWebAppInstallSource(
    IwaInstaller::InstallSourceType install_source_type,
    const base::FilePath& path,
    IwaSourceBundleProdFileOp operation) {
  auto src_bundle = IwaSourceBundleProdModeWithFileOp(path, operation);

  switch (install_source_type) {
    case IwaInstaller::InstallSourceType::kPolicy:
      return IsolatedWebAppInstallSource::FromExternalPolicy(
          std::move(src_bundle));
    case IwaInstaller::InstallSourceType::kKiosk:
      return IsolatedWebAppInstallSource::FromKiosk(std::move(src_bundle));
  }
}

std::optional<UpdateManifest::VersionEntry> GetVersionWithOptions(
    const UpdateManifest& update_manifest,
    const IsolatedWebAppExternalInstallOptions& install_options) {
  if (install_options.pinned_version().has_value()) {
    return update_manifest.GetVersion(*install_options.pinned_version(),
                                      install_options.update_channel());
  } else {
    return update_manifest.GetLatestVersion(install_options.update_channel());
  }
}
constexpr std::string_view kNonAllowlistedAppInstallationRejectedHistogramName =
    "WebApp.Isolated.NonAllowlistedAppInstallationRejected";

enum class ManagedSessionType {
  kManagedUserSession = 0,
  kManagedGuestSession = 1,
  kKiosk = 2,
  kMaxValue = kKiosk,
};

ManagedSessionType GetCurrentManagedSessionType() {
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsManagedGuestSession()) {
    return ManagedSessionType::kManagedGuestSession;
  }
  if (chromeos::IsKioskSession()) {
    return ManagedSessionType::kKiosk;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  return ManagedSessionType::kManagedUserSession;
}

}  // namespace

IwaInstaller::IwaInstallCommandWrapperImpl::IwaInstallCommandWrapperImpl(
    web_app::WebAppProvider* provider)
    : provider_(provider) {}

void IwaInstaller::IwaInstallCommandWrapperImpl::Install(
    const IsolatedWebAppInstallSource& install_source,
    const IsolatedWebAppUrlInfo& url_info,
    const IwaVersion& expected_version,
    WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
  // There is no need to keep the browser or profile alive when
  // policy-installing an IWA. If the browser or profile shut down, installation
  // will be re-attempted the next time they start, assuming that the policy is
  // still set.
  provider_->scheduler().InstallIsolatedWebApp(
      url_info, install_source, expected_version,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, std::move(callback));
}

IwaInstallerResult::IwaInstallerResult(Type type, std::string message)
    : type_(type), message_(std::move(message)) {}

base::Value::Dict IwaInstallerResult::ToDebugValue() const {
  return base::Value::Dict()
      .Set("type", base::ToString(type_))
      .Set("message", message_);
}

IwaInstaller::IwaInstaller(
    IsolatedWebAppExternalInstallOptions install_options,
    InstallSourceType install_source_type,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IwaInstallCommandWrapper> install_command_wrapper,
    base::Value::List& log,
    WebAppProvider* provider,
    ResultCallback callback)
    : install_options_(std::move(install_options)),
      install_source_type_(install_source_type),
      url_loader_factory_(std::move(url_loader_factory)),
      install_command_wrapper_(std::move(install_command_wrapper)),
      log_(log),
      provider_(provider),
      callback_(std::move(callback)) {
#if BUILDFLAG(IS_CHROMEOS)
  if (IsIwaBundleCacheEnabledInCurrentSession()) {
    log_->Append(base::Value(u"IWA bundle cache is enabled"));
    cache_client_ = std::make_unique<IwaCacheClient>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
}

IwaInstaller::~IwaInstaller() = default;

void IwaInstaller::Start() {
  if (!IwaKeyDistributionInfoProvider::GetInstance().IsManagedInstallPermitted(
          install_options_.web_bundle_id().id())) {
    base::UmaHistogramEnumeration(
        kNonAllowlistedAppInstallationRejectedHistogramName,
        GetCurrentManagedSessionType());
    LOG(ERROR) << "App " << install_options_.web_bundle_id().id()
               << " installation failed: Not in the managed allowlist.";
    Finish(Result(Result::Type::kErrorAppNotInAllowlist,
                  "Not in the managed allowlist."));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsManagedGuestSession() &&
      !base::FeatureList::IsEnabled(
          features::kIsolatedWebAppManagedGuestSessionInstall)) {
    LOG(ERROR) << "IWA installation in managed guest sessions is disabled.";
    Finish(Result(Result::Type::kErrorManagedGuestSessionInstallDisabled));
    return;
  }

  if (IsIwaBundleCacheEnabledInCurrentSession()) {
    // Install IWA from cache if possible, otherwise install it from the
    // Internet.
    log_->Append(base::Value(u"looking for cached bundle"));

    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            install_options_.web_bundle_id());

    CHECK_DEREF(provider_.get())
        .scheduler()
        .GetIsolatedWebAppBundleCachePath(
            url_info, install_options_.pinned_version(),
            IwaCacheClient::GetCurrentSessionType(),
            base::BindOnce(&IwaInstaller::OnBundleCachePathReceived,
                           weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  InstallFromInternet();
}

#if BUILDFLAG(IS_CHROMEOS)
void IwaInstaller::OnBundleCachePathReceived(GetBundleCachePathResult result) {
  if (result.has_value()) {
    log_->Append(base::Value("cached bundle is available, version: " +
                             result->cached_version().GetString() + ", path: " +
                             result->cached_bundle_path().MaybeAsASCII()));
    InstallFromCache(result->cached_bundle_path(), result->cached_version());
    return;
  }

  log_->Append(base::Value("cached bundle is not found"));
  InstallFromInternet();
}

void IwaInstaller::InstallFromCache(const base::FilePath& cache_file,
                                    const IwaVersion& version) {
  log_->Append(base::Value("start installing from the cache"));
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          install_options_.web_bundle_id());

  install_command_wrapper_->Install(
      GetIsolatedWebAppInstallSource(install_source_type_,
                                     std::move(cache_file),
                                     IwaSourceBundleProdFileOp::kCopy),
      url_info, std::move(version),
      base::BindOnce(&IwaInstaller::OnIwaInstalledFromCache,
                     weak_factory_.GetWeakPtr()));
}

void IwaInstaller::OnIwaInstalledFromCache(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  base::UmaHistogramBoolean("WebApp.Isolated.InstallFromCache",
                            result.has_value());
  if (result.has_value()) {
    log_->Append(base::Value("successfully installed IWA from the cache"));
    Finish(Result(Result::Type::kSuccess));
  } else {
    log_->Append(base::Value("could not install IWA from the cache"));
    // When installing from cache failed, try to install IWA from the Internet.
    InstallFromInternet();
  }
}

void IwaInstaller::OnBundleCopiedToCache(CopyBundleToCacheResult result) {
  web_app::UmaLogExpectedStatus(
      "WebApp.Isolated.CopyBundleToCacheAfterInstallation", result);
  if (result.has_value()) {
    log_->Append(base::Value(u"successfully copied bundle to the cache: " +
                             result->cached_bundle_path().LossyDisplayName()));
  } else {
    log_->Append(base::Value("failed to copy bundle to cache: " +
                             CopyBundleToCacheErrorToString(result.error())));
  }

  // `OnBundleCopiedToCache` is called only after the successful IWA
  // installation.
  Finish(Result(Result::Type::kSuccess));
}

#endif  // BUILDFLAG(IS_CHROMEOS)

void IwaInstaller::InstallFromInternet() {
  log_->Append(base::Value("start installing from the Internet"));
  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&IwaInstaller::CreateTempFile, weak_ptr),
      base::BindOnce(&IwaInstaller::DownloadUpdateManifest, weak_ptr),
      base::BindOnce(&IwaInstaller::DownloadWebBundle, weak_ptr),
      base::BindOnce(&IwaInstaller::RunInstallFromInternetCommand, weak_ptr));
}

void IwaInstaller::CreateTempFile(base::OnceClosure next_step_callback) {
  log_->Append(base::Value("creating temp file"));
  ScopedTempWebBundleFile::Create(base::BindOnce(
      &IwaInstaller::OnTempFileCreated, weak_factory_.GetWeakPtr(),
      std::move(next_step_callback)));
}

void IwaInstaller::OnTempFileCreated(base::OnceClosure next_step_callback,
                                     ScopedTempWebBundleFile bundle) {
  if (!bundle) {
    Finish(Result(Result::Type::kErrorCantCreateTempFile));
    return;
  }
  bundle_ = std::move(bundle);
  log_->Append(
      base::Value(u"created temp file: " + bundle_.path().LossyDisplayName()));
  std::move(next_step_callback).Run();
}

void IwaInstaller::DownloadUpdateManifest(
    base::OnceCallback<void(GURL, IwaVersion)> next_step_callback) {
  log_->Append(base::Value(
      "Downloading Update Manifest from " +
      install_options_.update_manifest_url().possibly_invalid_spec()));

  update_manifest_fetcher_ = std::make_unique<UpdateManifestFetcher>(
      install_options_.update_manifest_url(),
      kUpdateManifestFetchTrafficAnnotation, url_loader_factory_);
  update_manifest_fetcher_->FetchUpdateManifest(base::BindOnce(
      &IwaInstaller::OnUpdateManifestParsed, weak_factory_.GetWeakPtr(),
      std::move(next_step_callback)));
}

void IwaInstaller::OnUpdateManifestParsed(
    base::OnceCallback<void(GURL, IwaVersion)> next_step_callback,
    base::expected<UpdateManifest, UpdateManifestFetcher::Error> fetch_result) {
  update_manifest_fetcher_.reset();
  ASSIGN_OR_RETURN(
      UpdateManifest update_manifest, fetch_result,
      [&](UpdateManifestFetcher::Error error) {
        switch (error) {
          case UpdateManifestFetcher::Error::kDownloadFailed:
            Finish(Result(Result::Type::kErrorUpdateManifestDownloadFailed));
            break;
          case UpdateManifestFetcher::Error::kInvalidJson:
          case UpdateManifestFetcher::Error::kInvalidManifest:
            Finish(Result(Result::Type::kErrorUpdateManifestParsingFailed));
            break;
        }
      });

  std::optional<UpdateManifest::VersionEntry> version_to_install =
      GetVersionWithOptions(update_manifest, install_options_);

  if (!version_to_install) {
    Finish(Result(Result::Type::kErrorWebBundleUrlCantBeDetermined));
    return;
  }

  log_->Append(base::Value("Downloaded Update Manifest. Version to install: " +
                           version_to_install->version().GetString() +
                           " from " +
                           version_to_install->src().possibly_invalid_spec()));
  std::move(next_step_callback)
      .Run(version_to_install->src(), version_to_install->version());
}

void IwaInstaller::DownloadWebBundle(
    base::OnceCallback<void(IwaVersion)> next_step_callback,
    GURL web_bundle_url,
    IwaVersion expected_version) {
  log_->Append(base::Value("Downloading Web Bundle from " +
                           web_bundle_url.possibly_invalid_spec()));

  bundle_downloader_ = IsolatedWebAppDownloader::CreateAndStartDownloading(
      std::move(web_bundle_url), bundle_.path(),
      kWebBundleDownloadTrafficAnnotation, url_loader_factory_,
      base::BindOnce(&IwaInstaller::OnWebBundleDownloaded,
                     // If `this` is deleted, `bundle_downloader_` is deleted
                     // as well, and thus the callback will never run.
                     base::Unretained(this),
                     base::BindOnce(std::move(next_step_callback),
                                    std::move(expected_version))));
}

void IwaInstaller::OnWebBundleDownloaded(base::OnceClosure next_step_callback,
                                         int32_t net_error) {
  bundle_downloader_.reset();

  if (net_error != net::OK) {
    Finish(Result(Result::Type::kErrorCantDownloadWebBundle,
                  net::ErrorToString(net_error)));
    return;
  }
  log_->Append(base::Value("Downloaded Web Bundle"));
  std::move(next_step_callback).Run();
}

void IwaInstaller::RunInstallFromInternetCommand(IwaVersion expected_version) {
  log_->Append(base::Value("Running install command, expected version: " +
                           expected_version.GetString()));
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          install_options_.web_bundle_id());

  install_command_wrapper_->Install(
      GetIsolatedWebAppInstallSource(install_source_type_, bundle_.path(),
                                     IwaSourceBundleProdFileOp::kMove),
      url_info, expected_version,
      base::BindOnce(&IwaInstaller::OnIwaInstalledFromInternet,
                     weak_factory_.GetWeakPtr(), expected_version));
}

void IwaInstaller::OnIwaInstalledFromInternet(
    IwaVersion installed_version,
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Could not install the IWA "
               << install_options_.web_bundle_id();
    Finish(Result(Result::Type::kErrorCantInstallFromWebBundle,
                  base::ToString(result.error())));
    return;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (IsIwaBundleCacheEnabledInCurrentSession()) {
    // Successfully installed bundles should be copied to cache, so next time
    // the installation will happen from the cache.
    log_->Append(base::Value(
        "start copying bundle: " + install_options_.web_bundle_id().id() +
        " to cache after successful installation from the Internet"));

    IsolatedWebAppUrlInfo url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
            install_options_.web_bundle_id());
    CHECK_DEREF(provider_.get())
        .scheduler()
        .CopyIsolatedWebAppBundleToCache(
            url_info, IwaCacheClient::GetCurrentSessionType(),
            base::BindOnce(&IwaInstaller::OnBundleCopiedToCache,
                           weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Finish(Result(Result::Type::kSuccess));
}

void IwaInstaller::Finish(Result result) {
  std::move(callback_).Run(std::move(result));
}

std::unique_ptr<IwaInstaller> IwaInstallerFactory::Create(
    IsolatedWebAppExternalInstallOptions install_options,
    IwaInstaller::InstallSourceType install_source_type,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Value::List& log,
    WebAppProvider* provider,
    IwaInstaller::ResultCallback callback) {
  return GetIwaInstallerFactory().Run(
      std::move(install_options), install_source_type,
      std::move(url_loader_factory), log, provider, std::move(callback));
}

IwaInstallerFactory::IwaInstallerFactoryCallback&
IwaInstallerFactory::GetIwaInstallerFactory() {
  static base::NoDestructor<IwaInstallerFactoryCallback> iwa_installer_factory;
  if (!*iwa_installer_factory) {
    *iwa_installer_factory = GetDefaultIwaInstallerFactory();
  }
  return *iwa_installer_factory;
}

IwaInstallerFactory::IwaInstallerFactoryCallback
IwaInstallerFactory::GetDefaultIwaInstallerFactory() {
  return base::BindRepeating(
      [](IsolatedWebAppExternalInstallOptions install_options,
         IwaInstaller::InstallSourceType install_source_type,
         scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
         base::Value::List& log, WebAppProvider* provider,
         IwaInstaller::ResultCallback callback) {
        return std::make_unique<IwaInstaller>(
            std::move(install_options), install_source_type,
            std::move(url_loader_factory),
            std::make_unique<IwaInstaller::IwaInstallCommandWrapperImpl>(
                provider),
            log, provider, std::move(callback));
      });
}

std::ostream& operator<<(std::ostream& os,
                         IwaInstallerResultType install_result_type) {
  using Type = IwaInstallerResultType;

  switch (install_result_type) {
    case Type::kSuccess:
      return os << "kSuccess";
    case Type::kErrorCantCreateTempFile:
      return os << "kErrorCantCreateTempFile";
    case Type::kErrorUpdateManifestDownloadFailed:
      return os << "kErrorUpdateManifestDownloadFailed";
    case Type::kErrorUpdateManifestParsingFailed:
      return os << "kErrorUpdateManifestParsingFailed";
    case Type::kErrorWebBundleUrlCantBeDetermined:
      return os << "kErrorWebBundleUrlCantBeDetermined";
    case Type::kErrorCantDownloadWebBundle:
      return os << "kErrorCantDownloadWebBundle";
    case Type::kErrorCantInstallFromWebBundle:
      return os << "kErrorCantInstallFromWebBundle";
    case Type::kErrorManagedGuestSessionInstallDisabled:
      return os << "kErrorManagedGuestSessionInstallDisabled";
    case Type::kErrorAppNotInAllowlist:
      return os << "kErrorAppNotInAllowlist";
  }
}

}  // namespace web_app
