// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_discovery_task.h"

#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/containers/to_value_list.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/values_util.h"
#include "base/strings/string_view_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace web_app {

namespace {

// TODO(crbug.com/40274186): Once we support updating IWAs not installed via
// policy, we need to update this annotation.
constexpr auto kUpdateManifestFetchTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_update_discovery_update_manifest",
        "iwa_update_manifest_fetcher",
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

// TODO(crbug.com/40274186): Once we support updating IWAs not installed via
// policy, we need to update this annotation.
constexpr auto kWebBundleDownloadTrafficAnnotation =
    net::DefinePartialNetworkTrafficAnnotation(
        "iwa_update_discovery_web_bundle",
        "iwa_bundle_downloader",
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

}  // namespace

IwaUpdateDiscoveryTaskParams::IwaUpdateDiscoveryTaskParams(
    const GURL& update_manifest_url,
    const UpdateChannel& update_channel,
    bool allow_downgrades,
    const std::optional<IwaVersion>& pinned_version,
    const IsolatedWebAppUrlInfo& url_info,
    bool dev_mode)
    : update_manifest_url_(update_manifest_url),
      update_channel_(update_channel),
      allow_downgrades_(allow_downgrades),
      pinned_version_(pinned_version),
      url_info_(url_info),
      dev_mode_(dev_mode) {}

IwaUpdateDiscoveryTaskParams::IwaUpdateDiscoveryTaskParams(
    IwaUpdateDiscoveryTaskParams&& other) = default;

IwaUpdateDiscoveryTaskParams::~IwaUpdateDiscoveryTaskParams() = default;

// static
std::string IsolatedWebAppUpdateDiscoveryTask::SuccessToString(
    Success success) {
  switch (success) {
    case IsolatedWebAppUpdateDiscoveryTask::Success::kNoUpdateFound:
      return "Success::kNoUpdateFound";
    case IsolatedWebAppUpdateDiscoveryTask::Success::kUpdateAlreadyPending:
      return "Success::kUpdateAlreadyPending";
    case IsolatedWebAppUpdateDiscoveryTask::Success::
        kPinnedVersionUpdateFoundAndSavedInDatabase:
      return "Success::kPinnedVersionUpdateFoundAndSavedInDatabase";
    case IsolatedWebAppUpdateDiscoveryTask::Success::
        kDowngradeVersionFoundAndSavedInDatabase:
      return "Success::kDowngradeVersionFoundAndSavedInDatabase";
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
    case IsolatedWebAppUpdateDiscoveryTask::Error::
        kPinnedVersionNotFoundInUpdateManifest:
      return "Error::kPinnedVersionNotFoundInUpdateManifest";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kDowngradetNotAllowed:
      return "Error::kDowngradetNotAllowed";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kBundleDownloadError:
      return "Error::kBundleDownloadError";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kDownloadPathCreationFailed:
      return "Error::kDownloadPathCreationFailed";
    case IsolatedWebAppUpdateDiscoveryTask::Error::kUpdateDryRunFailed:
      return "Error::kUpdateDryRunFailed";
    case Error::kSystemShutdown:
      return "Error::kSystemShutdown";
  }
}

IsolatedWebAppUpdateDiscoveryTask::IsolatedWebAppUpdateDiscoveryTask(
    IwaUpdateDiscoveryTaskParams task_params,
    WebAppCommandScheduler& command_scheduler,
    WebAppRegistrar& registrar,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Profile& profile)
    : task_params_(std::move(task_params)),
      command_scheduler_(command_scheduler),
      registrar_(registrar),
      url_loader_factory_(std::move(url_loader_factory)),
      profile_(profile) {
  CHECK(url_loader_factory_);
  debug_log_ =
      base::Value::Dict()
          .Set("bundle_id", task_params_.url_info().web_bundle_id().id())
          .Set("update_channel", task_params_.update_channel().ToString())
          .Set("allow_downgrades", task_params_.allow_downgrades())
          .Set("app_id", task_params_.url_info().app_id())
          .Set("update_manifest_url",
               task_params_.update_manifest_url().spec());
  if (task_params_.pinned_version()) {
    debug_log_.Set("pinned_version",
                   task_params_.pinned_version()->GetString());
  }
}

IsolatedWebAppUpdateDiscoveryTask::~IsolatedWebAppUpdateDiscoveryTask() =
    default;

void IsolatedWebAppUpdateDiscoveryTask::Start(CompletionCallback callback) {
  if (KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    FailWith(Error::kSystemShutdown);
    return;
  }

  keep_alive_ = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::ISOLATED_WEB_APP_UPDATE,
      KeepAliveRestartOption::DISABLED);
  profile_keep_alive_ =
      profile_->IsOffTheRecord()
          ? nullptr
          : std::make_unique<ScopedProfileKeepAlive>(
                &*profile_, ProfileKeepAliveOrigin::kIsolatedWebAppUpdate);

  CHECK(!has_started_);
  has_started_ = true;
  callback_ = std::move(callback);

  debug_log_.Set("start_time", base::TimeToValue(base::Time::Now()));

  update_manifest_fetcher_ = std::make_unique<UpdateManifestFetcher>(
      task_params_.update_manifest_url(), kUpdateManifestFetchTrafficAnnotation,
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

  std::optional<UpdateManifest::VersionEntry> version_entry;
  if (task_params_.pinned_version().has_value()) {
    version_entry = update_manifest.GetVersion(*task_params_.pinned_version(),
                                               task_params_.update_channel());
    if (!version_entry) {
      FailWith(Error::kPinnedVersionNotFoundInUpdateManifest);
      return;
    }
  } else {
    version_entry =
        update_manifest.GetLatestVersion(task_params_.update_channel());
    if (!version_entry) {
      FailWith(Error::kUpdateManifestNoApplicableVersion);
      return;
    }
  }

  debug_log_.Set(
      "available_versions",
      base::ToValueList(update_manifest.versions(), [](const auto& entry) {
        return base::Value::Dict()
            .Set("version", entry.version().GetString())
            .Set("update_channels",
                 base::ToValueList(entry.channels(), [](const auto& channel) {
                   return channel.ToString();
                 }));
      }));

  debug_log_.Set(
      "version_entry",
      base::Value::Dict()
          .Set("version", version_entry->version().GetString())
          .Set("src", version_entry->src().spec())
          .Set("update_channel", task_params_.update_channel().ToString()));

  ASSIGN_OR_RETURN(
      const WebApp& iwa,
      GetIsolatedWebAppById(*registrar_, task_params_.url_info().app_id()),
      [&](const std::string&) { FailWith(Error::kIwaNotInstalled); });
  const auto& isolation_data = *iwa.isolation_data();
  currently_installed_version_ = isolation_data.version();
  debug_log_.Set("currently_installed_version",
                 currently_installed_version_->GetString());

  const auto& pending_update = isolation_data.pending_update_info();

  bool same_version_update_allowed_by_key_rotation = false;
  bool pending_info_overwrite_allowed_by_key_rotation = false;
  std::optional<std::vector<uint8_t>> rotated_key;
  switch (
      LookupRotatedKey(task_params_.url_info().web_bundle_id(), debug_log_)) {
    case KeyRotationLookupResult::kNoKeyRotation:
      break;
    case KeyRotationLookupResult::kKeyFound: {
      KeyRotationData data = GetKeyRotationData(
          task_params_.url_info().web_bundle_id(), isolation_data);
      rotated_key = base::ToVector(data.rotated_key);
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

  if (pending_update && pending_update->version == version_entry->version() &&
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
  // the mentioned command will re-check that the new version can be applied.
  VersionChangeValidationResult validation_result =
      ValidateVersionChangeFeasibility(
          version_entry->version(), *currently_installed_version_,
          task_params_.allow_downgrades(),
          same_version_update_allowed_by_key_rotation);

  switch (validation_result) {
    case VersionChangeValidationResult::kDowngradeDisallowed:
      FailWith(Error::kDowngradetNotAllowed);
      return;
    case VersionChangeValidationResult::kSameVersionUpdateDisallowed:
      SucceedWith(Success::kNoUpdateFound);
      return;
    case VersionChangeValidationResult::kAllowed:
      break;
  }

  bundle_downloader_ = IsolatedWebAppDownloader::Create(url_loader_factory_);
  if (!rotated_key) {
    CreateTempFile(std::move(*version_entry));
    return;
  }
  bundle_downloader_->DownloadInitialBytes(
      version_entry->src(), kWebBundleDownloadTrafficAnnotation,
      base::BindOnce(
          &IsolatedWebAppUpdateDiscoveryTask::CheckIntegrityBundleForRotatedKey,
          weak_factory_.GetWeakPtr(), std::move(*version_entry),
          std::move(*rotated_key)));
}

void IsolatedWebAppUpdateDiscoveryTask::CheckIntegrityBundleForRotatedKey(
    UpdateManifest::VersionEntry version_entry,
    std::vector<uint8_t> rotated_key,
    std::optional<std::string> initial_bytes) {
  // If it contains at least 2 of "📦", it means that both the beginning of the
  // integrity block and the beginning of the web bundle itself are in the
  // downloaded chunk - hence, the chunk contains the whole integrity block and
  // all the public keys. This is a heuristic to skip downloading the entire,
  // potentially big, bundle if it is not signed by the appropriate rotated key.
  if (initial_bytes &&
      initial_bytes->rfind("📦") != initial_bytes->find("📦") &&
      !base::Contains(initial_bytes.value(),
                      base::as_string_view(rotated_key))) {
    FailWith(Error::kUpdateManifestNoApplicableVersion);
    return;
  }
  CreateTempFile(version_entry);
}

void IsolatedWebAppUpdateDiscoveryTask::CreateTempFile(
    UpdateManifest::VersionEntry version_entry) {
  ScopedTempWebBundleFile::Create(
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnTempFileCreated,
                     weak_factory_.GetWeakPtr(), std::move(version_entry)));
}

void IsolatedWebAppUpdateDiscoveryTask::OnTempFileCreated(
    UpdateManifest::VersionEntry version_entry,
    ScopedTempWebBundleFile bundle) {
  if (!bundle) {
    FailWith(Error::kDownloadPathCreationFailed);
    return;
  }
  bundle_ = std::move(bundle);

  debug_log_.Set("bundle_download_path", bundle_.path().LossyDisplayName());

  CHECK(bundle_downloader_);
  bundle_downloader_->DownloadSignedWebBundle(
      version_entry.src(), bundle_.path(), kWebBundleDownloadTrafficAnnotation,
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnWebBundleDownloaded,
                     weak_factory_.GetWeakPtr(), version_entry.version()));
}

void IsolatedWebAppUpdateDiscoveryTask::OnWebBundleDownloaded(
    const IwaVersion& expected_version,
    int32_t net_error) {
  if (net_error != net::OK) {
    debug_log_.Set("bundle_download_error", net::ErrorToString(net_error));
    FailWith(Error::kBundleDownloadError);
    return;
  }

  // Both prepare-update and apply-update tasks expect that the location type
  // (dev mode / prod mode) stays unchanged between updates. For this reason,
  // the update info is wrapped accordingly depending on `dev_mode`.
  auto update_info =
      task_params_.dev_mode()
          ? IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
                IwaSourceBundleDevModeWithFileOp(
                    bundle_.path(), IwaSourceBundleDevFileOp::kMove),
                expected_version, task_params_.allow_downgrades())
          : IsolatedWebAppUpdatePrepareAndStoreCommand::UpdateInfo(
                IwaSourceBundleProdModeWithFileOp(
                    bundle_.path(), IwaSourceBundleProdFileOp::kMove),
                expected_version, task_params_.allow_downgrades());

  command_scheduler_->PrepareAndStoreIsolatedWebAppUpdate(
      update_info, task_params_.url_info(), std::move(keep_alive_),
      std::move(profile_keep_alive_),
      base::BindOnce(&IsolatedWebAppUpdateDiscoveryTask::OnUpdateDryRunDone,
                     weak_factory_.GetWeakPtr()));
}

void IsolatedWebAppUpdateDiscoveryTask::OnUpdateDryRunDone(
    IsolatedWebAppUpdatePrepareAndStoreCommandResult result) {
  if (!result.has_value()) {
    debug_log_.Set("prepare_and_store_command_error", result.error().message);
    FailWith(Error::kUpdateDryRunFailed);
    return;
  }
  debug_log_.Set("prepare_and_store_command_update_version",
                 result->update_version.GetString());

  Success success_type = Success::kUpdateFoundAndSavedInDatabase;
  if (result->update_version == task_params_.pinned_version()) {
    success_type = Success::kPinnedVersionUpdateFoundAndSavedInDatabase;
  }
  if (result->update_version < currently_installed_version_.value()) {
    success_type = Success::kDowngradeVersionFoundAndSavedInDatabase;
  }

  SucceedWith(success_type);
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
