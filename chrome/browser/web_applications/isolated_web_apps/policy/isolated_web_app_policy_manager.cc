// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_manager.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/web_applications/isolated_web_apps/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/policy/isolated_web_app_policy_constants.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace {

base::File::Error CreateNonExistingDirectory(const base::FilePath& path) {
  if (base::PathExists(path)) {
    return base::File::FILE_ERROR_EXISTS;
  }
  base::File::Error err = base::File::FILE_OK;
  base::CreateDirectoryAndGetError(path, &err);
  return err;
}

}  // namespace

namespace web_app {
IsolatedWebAppPolicyManager::IwaInstallCommandWrapperImpl::
    IwaInstallCommandWrapperImpl(web_app::WebAppProvider* provider)
    : provider_(provider) {}

void IsolatedWebAppPolicyManager::IwaInstallCommandWrapperImpl::Install(
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

IsolatedWebAppPolicyManager::IsolatedWebAppPolicyManager(
    const base::FilePath& context_dir,
    std::vector<IsolatedWebAppExternalInstallOptions> iwa_install_options,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    std::unique_ptr<IwaInstallCommandWrapper> installer,
    base::OnceCallback<void(std::vector<EphemeralAppInstallResult>)>
        ephemeral_install_cb)
    : ephemeral_iwa_install_options_(std::move(iwa_install_options)),
      current_app_(ephemeral_iwa_install_options_.begin()),
      installation_dir_(context_dir.Append(kEphemeralIwaRootDirectory)),
      url_loader_factory_(std::move(url_loader_factory)),
      result_vector_(ephemeral_iwa_install_options_.size(),
                     EphemeralAppInstallResult::kUnknown),
      installer_(std::move(installer)),
      ephemeral_install_cb_(std::move(ephemeral_install_cb)) {}
IsolatedWebAppPolicyManager::~IsolatedWebAppPolicyManager() = default;

void IsolatedWebAppPolicyManager::InstallEphemeralApps() {
  if (!profiles::IsPublicSession()) {
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

void IsolatedWebAppPolicyManager::CreateIwaEphemeralRootDirectory() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, installation_dir_),
      base::BindOnce(
          &IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated,
          weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnIwaEphemeralRootDirectoryCreated(
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

void IsolatedWebAppPolicyManager::DownloadUpdateManifest() {
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
      # TODO(cmfcmf): `internal` and `user_data` is duplicated in
      # `UpdateManifestFetcher::DownloadUpdateManifest`, but the
      # traffic annotator script complains that it is missing if it is not also
      # present here.
      internal {
        contacts {
          email: "peletskyi@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-05-25"
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
  current_update_manifest_fetcher_->FetchUpdateManifest(
      base::BindOnce(&IsolatedWebAppPolicyManager::OnUpdateManifestParsed,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::ContinueWithTheNextApp() {
  ++current_app_;
  if (current_app_ == ephemeral_iwa_install_options_.end()) {
    std::move(ephemeral_install_cb_).Run(result_vector_);
    return;
  }

  DownloadUpdateManifest();
}

void IsolatedWebAppPolicyManager::SetResultAndContinue(
    EphemeralAppInstallResult result) {
  const auto index =
      std::distance(ephemeral_iwa_install_options_.begin(), current_app_);
  result_vector_.at(index) = result;

  // If the error occurs after the directory for an app had been created,
  // then we should wipe the directory.
  if (result != EphemeralAppInstallResult::kSuccess) {
    WipeCurrentIwaDirectory();
    return;
  }

  ContinueWithTheNextApp();
}

void IsolatedWebAppPolicyManager::SetResultForAllAndFinish(
    EphemeralAppInstallResult result) {
  base::ranges::fill(result_vector_, result);
  std::move(ephemeral_install_cb_).Run(result_vector_);
}

void IsolatedWebAppPolicyManager::OnUpdateManifestParsed(
    base::expected<UpdateManifest, UpdateManifestFetcher::Error>
        update_manifest) {
  current_update_manifest_fetcher_.reset();
  if (!update_manifest.has_value()) {
    switch (update_manifest.error()) {
      case UpdateManifestFetcher::Error::kDownloadFailed:
        SetResultAndContinue(
            EphemeralAppInstallResult::kErrorUpdateManifestDownloadFailed);
        break;
      case UpdateManifestFetcher::Error::kInvalidJson:
      case UpdateManifestFetcher::Error::kInvalidManifest:
        SetResultAndContinue(
            EphemeralAppInstallResult::kErrorUpdateManifestParsingFailed);
        break;
      case UpdateManifestFetcher::Error::kNoApplicableVersion:
        SetResultAndContinue(
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

void IsolatedWebAppPolicyManager::CreateIwaDirectory() {
  base::FilePath iwa_dir =
      installation_dir_.Append(current_app_->web_bundle_id().id());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&CreateNonExistingDirectory, iwa_dir),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnIwaDirectoryCreated,
                     weak_factory_.GetWeakPtr(), iwa_dir));
}

void IsolatedWebAppPolicyManager::OnIwaDirectoryCreated(
    const base::FilePath& iwa_dir,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorCantCreateIwaDirectory);
    return;
  }

  current_app_->set_app_directory(iwa_dir);
  DownloadWebBundle();
}

void IsolatedWebAppPolicyManager::DownloadWebBundle() {
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
      # TODO(cmfcmf): `internal` and `user_data` is duplicated in
      # `IsolatedWebAppDownloader::DownloadSignedWebBundle`, but the
      # traffic annotator script complains that it is missing if it is not also
      # present here.
      internal {
        contacts {
          email: "cmfcmf@google.com"
        }
      }
      user_data {
        type: NONE
      }
      last_reviewed: "2023-06-01"
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
              &IsolatedWebAppPolicyManager::OnWebBundleDownloaded,
              // If `this` is deleted, `current_bundle_downloader_` is deleted
              // as well, and thus the callback will never run.
              base::Unretained(this), swbn_path));
}

void IsolatedWebAppPolicyManager::OnWebBundleDownloaded(
    const base::FilePath& path,
    int32_t net_error) {
  current_bundle_downloader_.reset();

  if (net_error != net::OK) {
    SetResultAndContinue(
        EphemeralAppInstallResult::kErrorCantDownloadWebBundle);
    return;
  }

  IsolatedWebAppLocation location = InstalledBundle{.path = path};
  IsolatedWebAppUrlInfo url_info =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
          current_app_->web_bundle_id());

  installer_->Install(
      location, url_info, current_app_->expected_version(),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnIwaInstalled,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnIwaInstalled(
    base::expected<InstallIsolatedWebAppCommandSuccess,
                   InstallIsolatedWebAppCommandError> result) {
  if (!result.has_value()) {
    LOG(ERROR) << "Could not install the IWA "
               << current_app_->web_bundle_id().id();
  }
  SetResultAndContinue(
      result.has_value()
          ? EphemeralAppInstallResult::kSuccess
          : EphemeralAppInstallResult::kErrorCantInstallFromWebBundle);
}

void IsolatedWebAppPolicyManager::WipeCurrentIwaDirectory() {
  const base::FilePath iwa_path_to_delete(current_app_->app_directory());
  current_app_->reset_app_directory();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, iwa_path_to_delete),
      base::BindOnce(&IsolatedWebAppPolicyManager::OnCurrentIwaDirectoryWiped,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppPolicyManager::OnCurrentIwaDirectoryWiped(bool wipe_result) {
  if (!wipe_result) {
    LOG(ERROR) << "Could not wipe an IWA directory";
  }

  ContinueWithTheNextApp();
}

}  // namespace web_app
