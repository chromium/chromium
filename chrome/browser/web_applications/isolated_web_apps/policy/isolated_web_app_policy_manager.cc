// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <optional>
#include <string>

#include "base/barrier_callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/callback_utils.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/common/pref_names.h"
#include "chromeos/components/mgs/managed_guest_session_utils.h"
#include "components/prefs/pref_service.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace web_app {

namespace {

template <typename Range, typename Proj = std::identity>
base::Value::List AsList(const Range& items, Proj proj = {}) {
  base::Value::List list;
  for (const auto& item : items) {
    list.Append(std::invoke(proj, item));
  }
  return list;
}

base::File::Error CreateDirectoryWithStatus(const base::FilePath& path) {
  base::File::Error err = base::File::FILE_OK;
  base::CreateDirectoryAndGetError(path, &err);
  return err;
}

base::File::Error CreateNonExistingDirectory(const base::FilePath& path) {
  if (base::PathExists(path)) {
    return base::File::FILE_ERROR_EXISTS;
  }
  return CreateDirectoryWithStatus(path);
}

std::vector<IsolatedWebAppExternalInstallOptions> ParseIwaPolicyValues(
    const base::Value::List& iwa_policy_values) {
  std::vector<IsolatedWebAppExternalInstallOptions> iwa_install_options;
  iwa_install_options.reserve(iwa_policy_values.size());
  for (const auto& policy_entry : iwa_policy_values) {
    const base::expected<IsolatedWebAppExternalInstallOptions, std::string>
        options = IsolatedWebAppExternalInstallOptions::FromPolicyPrefValue(
            policy_entry);
    if (options.has_value()) {
      iwa_install_options.push_back(options.value());
    } else {
      LOG(ERROR) << "Could not interpret IWA force-install policy: "
                 << options.error();
    }
  }

  return iwa_install_options;
}

base::flat_set<web_package::SignedWebBundleId> GetInstalledIwas(
    const WebAppRegistrar& registrar) {
  base::flat_set<web_package::SignedWebBundleId> installed_ids;
  for (const WebApp& web_app : registrar.GetApps()) {
    if (!web_app.isolation_data().has_value()) {
      continue;
    }
    auto url_info = IsolatedWebAppUrlInfo::Create(web_app.start_url());
    if (!url_info.has_value()) {
      LOG(ERROR) << "Unable to calculate IsolatedWebAppUrlInfo from "
                 << web_app.start_url();
      continue;
    }

    installed_ids.insert(url_info->web_bundle_id());
  }

  return installed_ids;
}

}  // namespace

namespace internal {

BulkIwaInstaller::IwaInstallCommandWrapperImpl::IwaInstallCommandWrapperImpl(
    web_app::WebAppProvider* provider)
    : provider_(provider) {}

void BulkIwaInstaller::IwaInstallCommandWrapperImpl::Install(
    const IsolatedWebAppLocation& location,
    const IsolatedWebAppUrlInfo& url_info,
    const base::Version& expected_version,
    WebAppCommandScheduler::InstallIsolatedWebAppCallback callback) {
  // There is no need to keep the browser or profile alive when
  // policy-installing an IWA. If the browser or profile shut down, installation
  // will be re-attempted the next time they start, assuming that the policy is
  // still set.
  provider_->scheduler().InstallIsolatedWebApp(
      url_info, location, expected_version,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, std::move(callback));
}

BulkIwaInstaller::BulkIwaInstaller(
    const base::FilePath& context_dir,
    std::vector<IsolatedWebAppExternalInstallOptions> iwa_install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IwaInstallCommandWrapper> installer,
    ResultCallback ephemeral_install_cb)
    : ephemeral_iwa_install_options_(std::move(iwa_install_options)),
      current_app_(ephemeral_iwa_install_options_.begin()),
      installation_dir_(context_dir.Append(kEphemeralIwaRootDirectory)),
      url_loader_factory_(std::move(url_loader_factory)),
      installer_(std::move(installer)),
      ephemeral_install_cb_(std::move(ephemeral_install_cb)) {}
BulkIwaInstaller::~BulkIwaInstaller() = default;

void BulkIwaInstaller::InstallEphemeralApps() {
  if (!chromeos::IsManagedGuestSession()) {
    LOG(ERROR) << "The IWAs should be installed only in managed guest session.";
    SetResultForAllAndFinish(
        EphemeralAppInstallResult::kErrorNotEphemeralSession);
    return;
  }

  if (ephemeral_iwa_install_options_.empty()) {
    SetResultForAllAndFinish(EphemeralAppInstallResult::kSuccess);
    return;
  }

  CreateIwaEphemeralRootDirectory();
}

void BulkIwaInstaller::CreateIwaEphemeralRootDirectory() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(CreateDirectoryWithStatus, installation_dir_),
      base::BindOnce(&BulkIwaInstaller::OnIwaEphemeralRootDirectoryCreated,
                     weak_factory_.GetWeakPtr()));
}

void BulkIwaInstaller::OnIwaEphemeralRootDirectoryCreated(
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Error in creating the directory for ephemeral IWAs: "
               << base::File::ErrorToString(error);
    SetResultForAllAndFinish(
        EphemeralAppInstallResult::kErrorCantCreateRootDirectory);
    return;
  }

  DownloadUpdateManifest();
}

void BulkIwaInstaller::DownloadUpdateManifest() {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
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

  current_update_manifest_fetcher_ = std::make_unique<UpdateManifestFetcher>(
      current_app_->update_manifest_url(),
      std::move(partial_traffic_annotation), url_loader_factory_);
  current_update_manifest_fetcher_->FetchUpdateManifest(base::BindOnce(
      &BulkIwaInstaller::OnUpdateManifestParsed, weak_factory_.GetWeakPtr()));
}

void BulkIwaInstaller::ContinueWithTheNextApp() {
  ++current_app_;
  if (current_app_ == ephemeral_iwa_install_options_.end()) {
    std::move(ephemeral_install_cb_).Run(result_vector_);
    return;
  }

  DownloadUpdateManifest();
}

void BulkIwaInstaller::FinishWithResult(EphemeralAppInstallResult result) {
  result_vector_.emplace_back(current_app_->web_bundle_id(), result);

  // We always copy the downloaded files into the profile during installation.
  // So we don't need the downloaded file any more.
  WipeIwaDownloadDirectory();
}

void BulkIwaInstaller::SetResultForAllAndFinish(
    EphemeralAppInstallResult result) {
  result_vector_.clear();
  for (const auto& options : ephemeral_iwa_install_options_) {
    result_vector_.emplace_back(options.web_bundle_id(), result);
  }
  std::move(ephemeral_install_cb_).Run(result_vector_);
}

void BulkIwaInstaller::OnUpdateManifestParsed(
    base::expected<UpdateManifest, UpdateManifestFetcher::Error>
        update_manifest) {
  current_update_manifest_fetcher_.reset();
  if (!update_manifest.has_value()) {
    switch (update_manifest.error()) {
      case UpdateManifestFetcher::Error::kDownloadFailed:
        FinishWithResult(
            EphemeralAppInstallResult::kErrorUpdateManifestDownloadFailed);
        break;
      case UpdateManifestFetcher::Error::kInvalidJson:
      case UpdateManifestFetcher::Error::kInvalidManifest:
        FinishWithResult(
            EphemeralAppInstallResult::kErrorUpdateManifestParsingFailed);
        break;
      case UpdateManifestFetcher::Error::kNoApplicableVersion:
        FinishWithResult(
            EphemeralAppInstallResult::kErrorWebBundleUrlCantBeDetermined);
        break;
    }
    return;
  }

  UpdateManifest::VersionEntry latest_version =
      GetLatestVersionEntry(*update_manifest);

  current_app_->set_web_bundle_url_and_expected_version(
      latest_version.src(), latest_version.version());
  CreateIwaDirectory();
}

void BulkIwaInstaller::CreateIwaDirectory() {
  base::FilePath iwa_dir =
      installation_dir_.Append(current_app_->web_bundle_id().id());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, iwa_dir),
      base::BindOnce(&BulkIwaInstaller::OnIwaDirectoryCreated,
                     weak_factory_.GetWeakPtr(), iwa_dir));
}

void BulkIwaInstaller::OnIwaDirectoryCreated(const base::FilePath& iwa_dir,
                                             base::File::Error error) {
  if (error != base::File::FILE_OK) {
    FinishWithResult(EphemeralAppInstallResult::kErrorCantCreateIwaDirectory);
    return;
  }

  current_app_->set_app_directory(iwa_dir);
  DownloadWebBundle();
}

void BulkIwaInstaller::DownloadWebBundle() {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
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

  base::FilePath swbn_path =
      current_app_->app_directory().Append(kMainSignedWebBundleFileName);

  current_bundle_downloader_ =
      IsolatedWebAppDownloader::CreateAndStartDownloading(
          current_app_->web_bundle_url(), swbn_path,
          std::move(partial_traffic_annotation), url_loader_factory_,
          base::BindOnce(
              &BulkIwaInstaller::OnWebBundleDownloaded,
              // If `this` is deleted, `current_bundle_downloader_` is deleted
              // as well, and thus the callback will never run.
              base::Unretained(this), swbn_path));
}

void BulkIwaInstaller::OnWebBundleDownloaded(const base::FilePath& path,
                                             int32_t net_error) {
  current_bundle_downloader_.reset();

  if (net_error != net::OK) {
    FinishWithResult(EphemeralAppInstallResult::kErrorCantDownloadWebBundle);
    return;
  }

  IsolatedWebAppLocation location = InstalledBundle{.path = path};
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          current_app_->web_bundle_id());

  installer_->Install(location, url_info, current_app_->expected_version(),
                      base::BindOnce(&BulkIwaInstaller::OnIwaInstalled,
                                     weak_factory_.GetWeakPtr()));
}

void BulkIwaInstaller::OnIwaInstalled(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Could not install the IWA "
               << current_app_->web_bundle_id().id();
  }
  FinishWithResult(
      result.has_value()
          ? EphemeralAppInstallResult::kSuccess
          : EphemeralAppInstallResult::kErrorCantInstallFromWebBundle);
}

void BulkIwaInstaller::WipeIwaDownloadDirectory() {
  const base::FilePath iwa_path_to_delete(current_app_->app_directory());
  current_app_->reset_app_directory();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, iwa_path_to_delete),
      base::BindOnce(&BulkIwaInstaller::OnIwaDownloadDirectoryWiped,
                     weak_factory_.GetWeakPtr()));
}

void BulkIwaInstaller::OnIwaDownloadDirectoryWiped(bool wipe_result) {
  if (!wipe_result) {
    LOG(ERROR) << "Could not wipe an IWA directory";
  }

  ContinueWithTheNextApp();
}

BulkIwaUninstaller::BulkIwaUninstaller(web_app::WebAppProvider& provider)
    : provider_(provider) {}

BulkIwaUninstaller::~BulkIwaUninstaller() = default;

void BulkIwaUninstaller::UninstallApps(
    const std::vector<web_package::SignedWebBundleId>& web_bundle_ids,
    ResultCallback callback) {
  if (web_bundle_ids.empty()) {
    std::move(callback).Run({});
    return;
  }

  auto uninstall_callback = base::BarrierCallback<Result>(
      web_bundle_ids.size(),
      base::BindOnce(&BulkIwaUninstaller::OnAppsUninstalled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));

  for (const auto& web_bundle_id : web_bundle_ids) {
    auto url_info =
        IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id);
    provider_->scheduler().RemoveInstallManagementMaybeUninstall(
        url_info.app_id(),
        // TODO(b/325885543): This is the wrong management type.
        WebAppManagement::Type::kCommandLine,
        webapps::WebappUninstallSource::kIwaEnterprisePolicy,
        base::BindOnce(
            [](web_package::SignedWebBundleId web_bundle_id,
               webapps::UninstallResultCode uninstall_code) -> Result {
              return {web_bundle_id, uninstall_code};
            },
            web_bundle_id)
            .Then(uninstall_callback));
  }
}

void BulkIwaUninstaller::OnAppsUninstalled(
    ResultCallback callback,
    std::vector<Result> uninstall_results) {
  std::move(callback).Run(std::move(uninstall_results));
}

}  // namespace internal

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(Profile* profile)
    : profile_(profile) {}
IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::Start(base::OnceClosure on_started_callback) {
  CHECK(on_started_callback_.is_null());
  on_started_callback_ = std::move(on_started_callback);

  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kIsolatedWebAppInstallForceList,
      base::BindRepeating(&IsolatedWebAppPolicyManager::ProcessPolicy,
                          weak_ptr_factory_.GetWeakPtr()));
  ProcessPolicy();
  if (!on_started_callback_.is_null()) {
    std::move(on_started_callback_).Run();
  }
}

void IsolatedWebAppPolicyManager::SetProvider(base::PassKey<WebAppProvider>,
                                              WebAppProvider& provider) {
  provider_ = &provider;
  bulk_uninstaller_ = std::make_unique<internal::BulkIwaUninstaller>(provider);
}

void IsolatedWebAppPolicyManager::ProcessPolicy() {
  CHECK(provider_);
  // Ensure that only one policy resolution can happen at one time.
  if (policy_is_being_processed_) {
    reprocess_policy_needed_ = true;
    return;
  }

  policy_is_being_processed_ = true;

  // So far we support only MGS.
  if (!chromeos::IsManagedGuestSession()) {
    OnPolicyProcessed();
    return;
  }

  provider_->scheduler().ScheduleCallback<AllAppsLock>(
      "IsolatedWebAppPolicyManager::ProcessPolicy", AllAppsLockDescription(),
      base::BindOnce(&IsolatedWebAppPolicyManager::DoProcessPolicy,
                     weak_ptr_factory_.GetWeakPtr()),
      /*on_complete=*/base::DoNothing());
}

void IsolatedWebAppPolicyManager::DoProcessPolicy(
    AllAppsLock& lock,
    base::Value::Dict& debug_info) {
  CHECK(provider_);
  CHECK(to_be_installed_.empty());
  CHECK(to_be_removed_.empty());
  CHECK(!bulk_installer_.get());

  const base::Value::List& iwa_policy_values =
      profile_->GetPrefs()->GetList(prefs::kIsolatedWebAppInstallForceList);

  std::vector<IsolatedWebAppExternalInstallOptions> apps_in_policy =
      ParseIwaPolicyValues(iwa_policy_values);
  debug_info.Set("apps_in_policy",
                 AsList(apps_in_policy, [](const auto& options) {
                   return options.web_bundle_id().id();
                 }));

  base::flat_set<web_package::SignedWebBundleId> installed_apps =
      GetInstalledIwas(lock.registrar());
  debug_info.Set("installed_apps",
                 AsList(installed_apps, &web_package::SignedWebBundleId::id));

  // This currently only installs apps that aren't already installed.
  // TODO (peletskyi@): As soon as we support version pinning
  // implement force update.
  for (const IsolatedWebAppExternalInstallOptions& app : apps_in_policy) {
    if (!base::Contains(installed_apps, app.web_bundle_id())) {
      to_be_installed_.push_back(app);
    }
  }
  debug_info.Set("to_be_installed",
                 AsList(to_be_installed_, [](const auto& options) {
                   return options.web_bundle_id().id();
                 }));

  for (const web_package::SignedWebBundleId& installed_app : installed_apps) {
    if (!base::Contains(apps_in_policy, installed_app,
                        &IsolatedWebAppExternalInstallOptions::web_bundle_id)) {
      to_be_removed_.push_back(installed_app);
    }
  }
  debug_info.Set("to_be_removed",
                 AsList(to_be_removed_, &web_package::SignedWebBundleId::id));

  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  RunChainedCallbacks(
      // Let's start with uninstalling because:
      // - we free up space for the potential installs;
      // - usually there is a strong reason why an admin whats to uninstall an
      //   app (e.g. security vulnerability). So it is better to uninstall it
      //   ASAP.
      base::BindOnce(&IsolatedWebAppPolicyManager::Uninstall, weak_ptr),
      base::BindOnce(&IsolatedWebAppPolicyManager::Install, weak_ptr),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnPolicyProcessed,
                     weak_ptr));
}

void IsolatedWebAppPolicyManager::Uninstall(
    base::OnceClosure next_step_callback) {
  bulk_uninstaller_->UninstallApps(
      to_be_removed_,
      base::BindOnce(&IsolatedWebAppPolicyManager::OnUninstalled,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(next_step_callback)));
}

void IsolatedWebAppPolicyManager::OnUninstalled(
    base::OnceClosure next_step_callback,
    std::vector<web_app::internal::BulkIwaUninstaller::Result>
        uninstall_results) {
  for (const auto& [web_bundle_id, uninstall_result] : uninstall_results) {
    if (uninstall_result != webapps::UninstallResultCode::kSuccess) {
      DLOG(WARNING) << "Could not uninstall IWA " << web_bundle_id.id()
                    << ". Error: " << uninstall_result;
    }
  }

  to_be_removed_.clear();

  std::move(next_step_callback).Run();
}

void IsolatedWebAppPolicyManager::Install(
    base::OnceClosure next_step_callback) {
  std::unique_ptr<internal::BulkIwaInstaller::IwaInstallCommandWrapper>
      installer = std::make_unique<
          internal::BulkIwaInstaller::IwaInstallCommandWrapperImpl>(provider_);

  auto url_loader_factory = profile_->GetURLLoaderFactory();

  auto install_complete_callback = base::BindOnce(
      &IsolatedWebAppPolicyManager::OnInstalled, weak_ptr_factory_.GetWeakPtr(),
      std::move(next_step_callback));

  bulk_installer_ = std::make_unique<internal::BulkIwaInstaller>(
      profile_->GetPath(), to_be_installed_, url_loader_factory,
      std::move(installer), std::move(install_complete_callback));
  bulk_installer_->InstallEphemeralApps();
}

void IsolatedWebAppPolicyManager::OnInstalled(
    base::OnceClosure next_step_callback,
    std::vector<web_app::internal::BulkIwaInstaller::Result> install_results) {
  for (const auto& [web_bundle_id, install_result] : install_results) {
    if (install_result !=
        internal::BulkIwaInstaller::EphemeralAppInstallResult::kSuccess) {
      DLOG(WARNING) << "Could not force-install IWA " << web_bundle_id.id()
                    << ". Error: " << base::to_underlying(install_result);
    }
  }

  bulk_installer_.reset();
  to_be_installed_.clear();

  std::move(next_step_callback).Run();
}

void IsolatedWebAppPolicyManager::OnPolicyProcessed() {
  policy_is_being_processed_ = false;

  if (!on_started_callback_.is_null()) {
    std::move(on_started_callback_).Run();
  }

  if (reprocess_policy_needed_) {
    reprocess_policy_needed_ = false;
    ProcessPolicy();
  }
  // TODO (peletskyi): Check policy compliance here as in theory
  // more race conditions are possible.
}

}  // namespace web_app
