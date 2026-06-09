// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_CHECK_AND_PREPARE_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_CHECK_AND_PREPARE_TASK_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_prepare_and_store_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/download/bundle_downloader.h"
#include "components/webapps/isolated_web_apps/types/iwa_version.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "net/base/net_errors.h"

namespace web_app {
class WebAppCommandScheduler;
class WebAppRegistrar;

class IwaUpdateCheckAndPrepareTaskParams {
 public:
  IwaUpdateCheckAndPrepareTaskParams(
      const GURL& update_manifest_url,
      const UpdateChannel& update_channel,
      bool allow_downgrades,
      const std::optional<IwaVersion>& pinned_version,
      const IsolatedWebAppUrlInfo& url_info,
      bool dev_mode,
      bool update_manifest_check_only = false);

  IwaUpdateCheckAndPrepareTaskParams(
      IwaUpdateCheckAndPrepareTaskParams&& other);
  IwaUpdateCheckAndPrepareTaskParams(
      const IwaUpdateCheckAndPrepareTaskParams& other);
  IwaUpdateCheckAndPrepareTaskParams& operator=(
      const IwaUpdateCheckAndPrepareTaskParams& other);
  ~IwaUpdateCheckAndPrepareTaskParams();

  const GURL& update_manifest_url() const { return update_manifest_url_; }
  const UpdateChannel& update_channel() const { return update_channel_; }
  bool allow_downgrades() const { return allow_downgrades_; }
  const std::optional<IwaVersion>& pinned_version() const {
    return pinned_version_;
  }
  const IsolatedWebAppUrlInfo& url_info() const { return url_info_; }
  bool dev_mode() const { return dev_mode_; }

  // If true, the update check task will stop once the update manifest has
  // been fetched and version applicability has been checked. It won't download
  // the bundle or stage the update in the database.
  bool update_manifest_check_only() const {
    return update_manifest_check_only_;
  }

 private:
  GURL update_manifest_url_;
  UpdateChannel update_channel_;
  bool allow_downgrades_;
  std::optional<IwaVersion> pinned_version_;
  IsolatedWebAppUrlInfo url_info_;
  bool dev_mode_;
  bool update_manifest_check_only_;
};

class IsolatedWebAppUpdateCheckAndPrepareTask {
 public:
  enum class Success {
    kNoUpdateFound,
    kUpdateAlreadyPending,
    kPinnedVersionUpdateFoundAndSavedInDatabase,  // Update to pinned version
                                                  // was successful. This type
                                                  // of update can happen only
                                                  // once, right after the app
                                                  // is pinned. After that, no
                                                  // update should happen.
    kDowngradeVersionFoundAndSavedInDatabase,
    kUpdateFoundAndSavedInDatabase,
    kUpdateFound
  };

  enum class Error {
    // Update Manifest errors
    kUpdateManifestDownloadFailed,
    kUpdateManifestInvalidJson,
    kUpdateManifestInvalidManifest,
    kUpdateManifestNoApplicableVersion,
    kIwaNotInstalled,

    // Version pinning errors
    kPinnedVersionNotFoundInUpdateManifest,

    // Version downgrade errors
    kDowngradetNotAllowed,

    // Signed Web Bundle download errors
    kDownloadPathCreationFailed,
    kBundleDownloadError,

    // Update dry run errors
    kUpdateDryRunFailed,

    kSystemShutdown,
  };

  static std::string SuccessToString(Success success);
  static std::string ErrorToString(Error error);

  using CompletionStatus = base::expected<Success, Error>;
  using CompletionCallback = base::OnceCallback<void(CompletionStatus status)>;

  IsolatedWebAppUpdateCheckAndPrepareTask(
      IwaUpdateCheckAndPrepareTaskParams task_params,
      WebAppCommandScheduler& command_scheduler,
      WebAppRegistrar& registrar,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile& profile);
  ~IsolatedWebAppUpdateCheckAndPrepareTask();

  IsolatedWebAppUpdateCheckAndPrepareTask(
      const IsolatedWebAppUpdateCheckAndPrepareTask&) = delete;
  IsolatedWebAppUpdateCheckAndPrepareTask& operator=(
      const IsolatedWebAppUpdateCheckAndPrepareTask&) = delete;

  void Start(CompletionCallback callback);
  bool has_started() const { return has_started_; }

  const IsolatedWebAppUrlInfo& url_info() const {
    return task_params_.url_info();
  }

  // The version discovered by the update check. This is populated once the
  // update manifest has been fetched and parsed, and contains the version
  // that was determined to be applicable to the app.
  const std::optional<IwaVersion>& discovered_version() const {
    return discovered_version_;
  }

  base::Value AsDebugValue() const;

 private:
  void SucceedWith(Success success);

  void FailWith(Error error);

  void OnUpdateManifestFetched(
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          fetch_result);

  void CheckIntegrityBundleForRotatedKey(
      UpdateManifest::VersionEntry version_entry,
      std::vector<uint8_t> rotated_key,
      std::optional<std::string> initial_bytes);

  void CreateTempFile(UpdateManifest::VersionEntry version_entry);

  void OnTempFileCreated(UpdateManifest::VersionEntry version_entry,
                         ScopedTempWebBundleFile bundle);

  void OnWebBundleDownloaded(const IwaVersion& expected_version,
                             int32_t net_error);

  void OnUpdateDryRunDone(
      IsolatedWebAppUpdatePrepareAndStoreCommandResult result);

  base::DictValue debug_log_;
  bool has_started_ = false;
  CompletionCallback callback_;

  const IwaUpdateCheckAndPrepareTaskParams task_params_;

  raw_ref<WebAppCommandScheduler> command_scheduler_;
  raw_ref<WebAppRegistrar> registrar_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // Set on Start:
  std::unique_ptr<ScopedKeepAlive> keep_alive_;
  std::unique_ptr<ScopedProfileKeepAlive> profile_keep_alive_;
  const raw_ref<Profile> profile_;

  ScopedTempWebBundleFile bundle_;
  std::optional<IwaVersion> currently_installed_version_;
  std::optional<IwaVersion> discovered_version_;

  std::unique_ptr<UpdateManifestFetcher> update_manifest_fetcher_;
  std::unique_ptr<IsolatedWebAppDownloader> bundle_downloader_;

  base::WeakPtrFactory<IsolatedWebAppUpdateCheckAndPrepareTask> weak_factory_{
      this};
};

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdateCheckAndPrepareTask::Success& success);

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdateCheckAndPrepareTask::Error& error);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_UPDATE_ISOLATED_WEB_APP_UPDATE_CHECK_AND_PREPARE_TASK_H_
