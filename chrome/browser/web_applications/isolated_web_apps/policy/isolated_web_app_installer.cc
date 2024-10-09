// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_installer.h"

#include "base/lazy_instance.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_external_install_options.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
}  // namespace

IwaInstaller::IwaInstallCommandWrapperImpl::IwaInstallCommandWrapperImpl(
    web_app::WebAppProvider* provider)
    : provider_(provider) {}

void IwaInstaller::IwaInstallCommandWrapperImpl::Install(
    const IsolatedWebAppInstallSource& install_source,
    const IsolatedWebAppUrlInfo& url_info,
    const base::Version& expected_version,
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
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IwaInstallCommandWrapper> install_command_wrapper,
    base::Value::List& log,
    ResultCallback callback)
    : install_options_(std::move(install_options)),
      url_loader_factory_(std::move(url_loader_factory)),
      install_command_wrapper_(std::move(install_command_wrapper)),
      log_(log),
      callback_(std::move(callback)) {}

IwaInstaller::~IwaInstaller() = default;

void IwaInstaller::Start() {
  if (chromeos::IsManagedGuestSession() &&
      !base::FeatureList::IsEnabled(
          features::kIsolatedWebAppManagedGuestSessionInstall)) {
    LOG(ERROR) << "IWA installation in managed guest sessions is disabled.";
    Finish(Result(Result::Type::kErrorManagedGuestSessionInstallDisabled));
    return;
  }

  auto weak_ptr = weak_factory_.GetWeakPtr();
  RunChainedCallbacks(
      base::BindOnce(&IwaInstaller::CreateTempFile, weak_ptr),
      base::BindOnce(&IwaInstaller::DownloadUpdateManifest, weak_ptr),
      base::BindOnce(&IwaInstaller::DownloadWebBundle, weak_ptr),
      base::BindOnce(&IwaInstaller::RunInstallCommand, weak_ptr));
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
    base::OnceCallback<void(GURL, base::Version)> next_step_callback) {
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
    base::OnceCallback<void(GURL, base::Version)> next_step_callback,
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

  std::optional<UpdateManifest::VersionEntry> latest_version =
      update_manifest.GetLatestVersion(install_options_.update_channel());
  if (!latest_version.has_value()) {
    Finish(Result(Result::Type::kErrorWebBundleUrlCantBeDetermined));
    return;
  }

  log_->Append(base::Value("Downloaded Update Manifest. Latest version: " +
                           latest_version->version().GetString() + " from " +
                           latest_version->src().possibly_invalid_spec()));
  std::move(next_step_callback)
      .Run(latest_version->src(), latest_version->version());
}

void IwaInstaller::DownloadWebBundle(
    base::OnceCallback<void(base::Version)> next_step_callback,
    GURL web_bundle_url,
    base::Version expected_version) {
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

void IwaInstaller::RunInstallCommand(base::Version expected_version) {
  log_->Append(base::Value("Running install command, expected version: " +
                           expected_version.GetString()));
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          install_options_.web_bundle_id());

  // TODO: crbug.com/306638108 - In the time it took to download everything, the
  // app might have already been installed by other means.

  install_command_wrapper_->Install(
      IsolatedWebAppInstallSource::FromExternalPolicy(
          IwaSourceBundleProdModeWithFileOp(bundle_.path(),
                                            IwaSourceBundleProdFileOp::kMove)),
      url_info, std::move(expected_version),
      base::BindOnce(&IwaInstaller::OnIwaInstalled,
                     weak_factory_.GetWeakPtr()));
}

void IwaInstaller::OnIwaInstalled(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Could not install the IWA "
               << install_options_.web_bundle_id();
    Finish(Result(Result::Type::kErrorCantInstallFromWebBundle,
                  base::ToString(result.error())));
  } else {
    Finish(Result(Result::Type::kSuccess));
  }
}

void IwaInstaller::Finish(Result result) {
  std::move(callback_).Run(std::move(result));
}

std::unique_ptr<IwaInstaller> IwaInstallerFactory::Create(
    IsolatedWebAppExternalInstallOptions install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::Value::List& log,
    WebAppProvider* provider,
    IwaInstaller::ResultCallback callback) {
  return GetIwaInstallerFactory().Run(std::move(install_options),
                                      std::move(url_loader_factory), log,
                                      provider, std::move(callback));
}

IwaInstallerFactory::IwaInstallerFactoryCallback&
IwaInstallerFactory::GetIwaInstallerFactory() {
  static base::LazyInstance<IwaInstallerFactoryCallback>::Leaky
      iwa_installer_factory = LAZY_INSTANCE_INITIALIZER;
  if (!iwa_installer_factory.Get()) {
    iwa_installer_factory.Get() = base::BindRepeating(
        [](IsolatedWebAppExternalInstallOptions install_options,
           scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
           base::Value::List& log, WebAppProvider* provider,
           IwaInstaller::ResultCallback callback) {
          return std::make_unique<IwaInstaller>(
              std::move(install_options), std::move(url_loader_factory),
              std::make_unique<IwaInstaller::IwaInstallCommandWrapperImpl>(
                  provider),
              log, std::move(callback));
        });
  }
  return iwa_installer_factory.Get();
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
  }
}

}  // namespace web_app
