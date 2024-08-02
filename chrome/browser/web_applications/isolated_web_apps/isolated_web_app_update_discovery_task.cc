// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_update_discovery_task.h"

#include <optional>
#include <ostream>

#include "base/containers/to_value_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

namespace web_app {

// static
std::string IsolatedWebAppUpdateDiscoveryTask::SuccessToString(
    Success success) {
  switch (success) {
    case IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound:
      return "Success::kNoUpdateFound";
    case IsolatedWebAppUpdateDiscoveryTask::Success::kUpdateAlreadyPending:
      return "Success::kUpdateAlreadyPending";
    case IsolatedWebAppUpdateDiscoveryTask::Success::
        kUpdateFoundAndSavedInDatabase:
      return "Success::kUpdateFoundAndDryRunSuccessful";
  }
}

// static
std::string IsolatedWebAppUpdateDiscoveryTask::ErrorToString(Error error) {
  switch (error) {
    case IsolatedWebAppUpdateDiscoveryTask::Error::
        kUpdateManifestDownloadFailed:
      return "Error::kUpdateManifestDownloadFailed";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kUpdateManifestInvalidJson:
      return "Error::kUpdateManifestInvalidJson";
    case IsolatedWebAppUpdateDiscoveryTask::Error::
        kUpdateManifestInvalidManifest:
      return "Error::kUpdateManifestInvalidManifest";
    case IsolatedWebAppUpdateDiscoveryTask::Error::
        kUpdateManifestNoApplicableVersion:
      return "Error::kUpdateManifestNoApplicableVersion";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kIwaNotInstalled:
      return "Error::kIwaNotInstalled";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kBundleDownloadError:
      return "Error::kBundleDownloadError";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kDownloadPathCreationFailed:
      return "Error::kDownloadPathCreationFailed";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kUpdateDryRunFailed:
      return "Error::kUpdateDryRunFailed";
  }
}

IsolatedWebAppUpdateDiscoveryTask::IsolatedWebAppUpdateDiscoveryTask(
    GURL update_manifest_url,
    IsolatedWebAppUrlInfo url_info,
    WebAppCommandScheduler& command_scheduler,
    WebAppRegistrar& registrar,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : update_manifest_url_(std::move(update_manifest_url)),
      url_info_(std::move(url_info)),
      command_scheduler_(command_scheduler),
      registrar_(registrar),
      url_loader_factory_(std::move(url_loader_factory)) {
  CHECK(url_loader_factory_);
  debug_log_ = base::Value::Dict()
                   .Set("bundle_id", url_info_.web_bundle_id().id())
                   .Set("app_id", url_info_.app_id())
                   .Set("update_manifest_url", update_manifest_url_.spec());
}

IsolatedWebAppUpdateDiscoveryTask::~IsolatedWebAppUpdateDiscoveryTask() =
    default;

void IsolatedWebAppUpdateDiscoveryTask::Start(CompletionCallback callback) {
  CHECK(!has_started_);
  has_started_ = true;
  callback_ = std::move(callback);

  debug_log_.Set("start_time", base::TimeToValue(base::Time::Now()));

  // TODO(crbug.com/40274186): Once we support updating IWAs not installed via
  // policy, we need to update this annotation.
  net::PartialNetworkTrafficAnnotationTag update_manifest_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "iwa_update_discovery_update_manifest", "iwa_update_manifest_fetcher",
          R"(
    semantics {
      sender: "Isolated Web App Update Manager"
      description:
        "Downloads the Update Manifest of an Isolated Web App that is "
        "policy-installed. The Update Manifest contains at least the list of "
        "the available versions of the IWA and the URL to the Signed Web "
        "Bundles that correspond to each version."
      trigger:
        "The browser automatically checks for updates of all policy-installed "
        "Isolated Web Apps after startup and in regular time intervals."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      chrome_policy {
        IsolatedWebAppInstallForceList {
          IsolatedWebAppInstallForceList: ""
        }
      }
    })");

  update_manifest_fetcher_ = std::make_unique<UpdateManifestFetcher>(
      update_manifest_url_, update_manifest_traffic_annotation,
      url_loader_factory_);
  update_manifest_fetcher_->FetchUpdateManifest(base::BindOnce(
      &IsolatedWebAppUpdateDiscoveryTask::OnUpdateManifestFetched,
      weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateDiscoveryTask::OnUpdateManifestFetched(
    base::expected<UpdateManifest, UpdateManifestFetcher::Error> fetch_result) {
  ASSIGN_OR_RETURN(UpdateManifest update_manifest, fetch_result,
                   [&](UpdateManifestFetcher::Error error) {
                     switch (error) {
                       case UpdateManifestFetcher::Error::kDownloadFailed:
                         FailWith(Error::kUpdateManifestDownloadFailed);
                         break;
                       case UpdateManifestFetcher::Error::kInvalidJson:
                         FailWith(Error::kUpdateManifestInvalidJson);
                         break;
                       case UpdateManifestFetcher::Error::kInvalidManifest:
                         FailWith(Error::kUpdateManifestInvalidManifest);
                         break;
                     }
                   });

  std::optional<UpdateManifest::VersionEntry> latest_version_entry =
      update_manifest.GetLatestVersion(
          // TODO(b/294481776): In the future, we will support channel selection
          // via policy and by the end user for unmanaged users. For now, we
          // always use the "default" channel.
          UpdateChannelId::default_id());
  if (!latest_version_entry.has_value()) {
    FailWith(Error::kUpdateManifestNoApplicableVersion);
    return;
  }

  debug_log_.Set(
      "available_versions",
      base::ToValueList(update_manifest.versions(), [](const auto& entry) {
        return entry.version().GetString();
      }));
  debug_log_.Set(
      "latest_version",
      base::Value::Dict()
          .Set("version", latest_version_entry->version().GetString())
          .Set("src", latest_version_entry->src().spec()));

  ASSIGN_OR_RETURN(
      const WebApp& iwa, GetIsolatedWebAppById(*registrar_, url_info_.app_id()),
      [&](const std::string&) { FailWith(Error::kIwaNotInstalled); });
  const auto& isolation_data = *iwa.isolation_data();
  base::Version currently_installed_version = isolation_data.version;
  debug_log_.Set("currently_installed_version",
                 currently_installed_version.GetString());

  const auto& pending_update = isolation_data.pending_update_info();

  bool same_version_update_allowed_by_key_rotation = false;
  bool pending_info_overwrite_allowed_by_key_rotation = false;
  switch (LookupRotatedKey(url_info_.web_bundle_id(), debug_log_)) {
    case KeyRotationLookupResult::kNoKeyRotation:
      break;
    case KeyRotationLookupResult::kKeyFound: {
      KeyRotationData data =
          GetKeyRotationData(url_info_.web_bundle_id(), isolation_data);
      if (!data.current_installation_has_rk) {
        same_version_update_allowed_by_key_rotation = true;
      }
      if (!data.pending_update_has_rk) {
        pending_info_overwrite_allowed_by_key_rotation = true;
      }
    } break;
    case KeyRotationLookupResult::kKeyBlocked: {
      FailWith(Error::kUpdateManifestNoApplicableVersion);
      return;
    }
  }

  if (pending_update &&
      pending_update->version == latest_version_entry->version() &&
      !pending_info_overwrite_allowed_by_key_rotation) {
    // If we already have a pending update for this version, stop. However,
    // we do allow overwriting a pending update with a different pending
    // update version or if there's a chance that this will yield a bundle
    // signed by a rotated key.
    SucceedWith(Success::kUpdateAlreadyPending);
    return;
  }

  // Since this task is not holding any `WebAppLock`s, there is no guarantee
  // that the installed version of the IWA won't change in the time between
  // now and when we schedule the
  // `IsolatedWebAppUpdatePrepareAndStoreCommand`. This is not an issue, as
  // `IsolatedWebAppUpdatePrepareAndStoreCommand` will re-check that the new
  // version is indeed newer than the currently installed version.
  if (currently_installed_version > latest_version_entry->version() ||
      (currently_installed_version == latest_version_entry->version() &&
       !same_version_update_allowed_by_key_rotation)) {
    // Never downgrade apps for now.
    SucceedWith(Success::kNoUpdateFound);
    return;
  }

  GetDownloadPath(std::move(*latest_version_entry));
}

void IsolatedWebAppUpdateDiscoveryTask::GetDownloadPath(
    UpdateManifest::VersionEntry version_entry) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce([]() -> std::optional<base::FilePath> {
        base::FilePath download_path;
        bool success = base::CreateTemporaryFile(&download_path);
        return success ? std::make_optional(download_path) : std::nullopt;
      }),
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnGetDownloadPath,
                     weak_factory_.GetWeakPtr(), std::move(version_entry)));
}

void IsolatedWebAppUpdateDiscoveryTask::OnGetDownloadPath(
    UpdateManifest::VersionEntry version_entry,
    std::optional<base::FilePath> download_path) {
  if (!download_path.has_value()) {
    FailWith(Error::kDownloadPathCreationFailed);
    return;
  }

  // TODO(crbug.com/40274186): Once we support updating IWAs not installed via
  // policy, we need to update this annotation.
  net::PartialNetworkTrafficAnnotationTag web_bundle_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "iwa_update_discovery_web_bundle", "iwa_bundle_downloader",
          R"(
    semantics {
      sender: "Isolated Web App Update Manager"
      description:
        "Downloads an updated Signed Web Bundle of an Isolated Web App that is "
        "policy-installed, after an update has been discovered for it. The "
        "Signed Web Bundle contains code and other resources of the IWA."
      trigger:
        "The browser automatically checks for updates of all policy-installed "
        "Isolated Web Apps after startup and in regular time intervals. If an "
        "update is found, then the corresponding Signed Web Bundle is "
        "downloaded."
    }
    policy {
      setting: "This feature cannot be disabled in settings."
      chrome_policy {
        IsolatedWebAppInstallForceList {
          IsolatedWebAppInstallForceList: ""
        }
      }
    })");

  debug_log_.Set("bundle_download_path", download_path->LossyDisplayName());
  bundle_downloader_ = IsolatedWebAppDownloader::CreateAndStartDownloading(
      version_entry.src(), *download_path, web_bundle_traffic_annotation,
      url_loader_factory_,
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnWebBundleDownloaded,
                     weak_factory_.GetWeakPtr(), *download_path,
                     version_entry.version()));
}

void IsolatedWebAppUpdateDiscoveryTask::OnWebBundleDownloaded(
    const base::FilePath& download_path,
    const base::Version& expected_version,
    int32_t net_error) {
  if (net_error != net::OK) {
    debug_log_.Set("bundle_download_error", net::ErrorToString(net_error));
    FailWith(Error::kBundleDownloadError);
    return;
  }

  command_scheduler_->PrepareAndStoreIsolatedWebAppUpdate(
      IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
          IwaSourceBundleProdModeWithFileOp(download_path,
                                            IwaSourceBundleProdFileOp::kMove),
          expected_version),
      url_info_,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr,
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnUpdateDryRunDone,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateDiscoveryTask::OnUpdateDryRunDone(
    IsolatedWebAppUpdatePrepareAndStoreCommandResult result) {
  if (result.has_value()) {
    debug_log_.Set("prepare_and_store_command_update_version",
                   result->update_version.GetString());
    SucceedWith(Success::kUpdateFoundAndSavedInDatabase);
  } else {
    debug_log_.Set("prepare_and_store_command_error", result.error().message);
    FailWith(Error::kUpdateDryRunFailed);
  }
}

void IsolatedWebAppUpdateDiscoveryTask::SucceedWith(Success success) {
  debug_log_.Set("end_time", base::TimeToValue(base::Time::Now()));
  debug_log_.Set("result", SuccessToString(success));
  VLOG(1) << "Isolated Web App update discovery task succeeded: " << success;
  std::move(callback_).Run(success);
}

void IsolatedWebAppUpdateDiscoveryTask::FailWith(Error error) {
  debug_log_.Set("end_time", base::TimeToValue(base::Time::Now()));
  debug_log_.Set("result", ErrorToString(error));
  LOG(ERROR) << "Isolated Web App update discovery task failed: " << error;
  std::move(callback_).Run(base::unexpected(error));
}

base::Value IsolatedWebAppUpdateDiscoveryTask::AsDebugValue() const {
  return base::Value(debug_log_.Clone());
}

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdateDiscoveryTask::Success& success) {
  return os << IsolatedWebAppUpdateDiscoveryTask::SuccessToString(success);
}

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdateDiscoveryTask::Error& error) {
  return os << IsolatedWebAppUpdateDiscoveryTask::ErrorToString(error);
}

}  // namespace web_app
