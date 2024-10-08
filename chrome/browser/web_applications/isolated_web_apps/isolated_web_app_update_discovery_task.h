// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_DISCOVERY_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_DISCOVERY_TASK_H_

#include <iosfwd>
#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "base/version.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_downloader.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest.h"
#include "chrome/browser/web_applications/isolated_web_apps/update_manifest/update_manifest_fetcher.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "net/base/net_errors.h"

namespace web_app {

class IwaUpdateDiscoveryTaskParams {
 public:
  IwaUpdateDiscoveryTaskParams(const GURL& update_manifest_url,
                               const UpdateChannel& update_channel,
                               const IsolatedWebAppUrlInfo& url_info,
                               bool dev_mode);

  IwaUpdateDiscoveryTaskParams(IwaUpdateDiscoveryTaskParams&& other);

  const GURL& update_manifest_url() const { return update_manifest_url_; }
  const UpdateChannel& update_channel() const { return update_channel_; }
  const IsolatedWebAppUrlInfo& url_info() const { return url_info_; }
  bool dev_mode() const { return dev_mode_; }

 private:
  GURL update_manifest_url_;
  UpdateChannel update_channel_;
  IsolatedWebAppUrlInfo url_info_;
  bool dev_mode_;
};

class IsolatedWebAppUpdateDiscoveryTask {
 public:
  enum class Success {
    kNoUpdateFound,
    kUpdateAlreadyPending,
    kUpdateFoundAndSavedInDatabase,
  };

  enum class Error {
    // Update Manifest errors
    kUpdateManifestDownloadFailed,
    kUpdateManifestInvalidJson,
    kUpdateManifestInvalidManifest,
    kUpdateManifestNoApplicableVersion,

    kIwaNotInstalled,

    // Signed Web Bundle download errors
    kDownloadPathCreationFailed,
    kBundleDownloadError,

    // Update dry run errors
    kUpdateDryRunFailed
  };

  static std::string SuccessToString(Success success);
  static std::string ErrorToString(Error error);

  using CompletionStatus = base::expected<Success, Error>;
  using CompletionCallback = base::OnceCallback<void(CompletionStatus status)>;

  IsolatedWebAppUpdateDiscoveryTask(
      IwaUpdateDiscoveryTaskParams task_params,
      WebAppCommandScheduler& command_scheduler,
      WebAppRegistrar& registrar,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~IsolatedWebAppUpdateDiscoveryTask();

  IsolatedWebAppUpdateDiscoveryTask(const IsolatedWebAppUpdateDiscoveryTask&) =
      delete;
  IsolatedWebAppUpdateDiscoveryTask& operator=(
      const IsolatedWebAppUpdateDiscoveryTask&) = delete;

  void Start(CompletionCallback callback);
  bool has_started() const { return has_started_; }

  const IsolatedWebAppUrlInfo& url_info() const {
    return task_params_.url_info();
  }

  base::Value AsDebugValue() const;

 private:
  void SucceedWith(Success success);

  void FailWith(Error error);

  void OnUpdateManifestFetched(
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          fetch_result);

  void CreateTempFile(UpdateManifest::VersionEntry version_entry);

  void OnTempFileCreated(UpdateManifest::VersionEntry version_entry,
                         ScopedTempWebBundleFile bundle);

  void OnWebBundleDownloaded(const base::Version& expected_version,
                             int32_t net_error);

  void OnUpdateDryRunDone(
      IsolatedWebAppUpdatePrepareAndStoreCommandResult result);

  base::Value::Dict debug_log_;
  bool has_started_ = false;
  CompletionCallback callback_;

  const IwaUpdateDiscoveryTaskParams task_params_;

  raw_ref<WebAppCommandScheduler> command_scheduler_;
  raw_ref<WebAppRegistrar> registrar_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  ScopedTempWebBundleFile bundle_;

  std::unique_ptr<UpdateManifestFetcher> update_manifest_fetcher_;
  std::unique_ptr<IsolatedWebAppDownloader> bundle_downloader_;

  base::WeakPtrFactory<IsolatedWebAppUpdateDiscoveryTask> weak_factory_{this};
};

std::ostream& operator<<(
    std::ostream& os,
    const IsolatedWebAppUpdateDiscoveryTask::Success& success);

std::ostream& operator<<(std::ostream& os,
                         const IsolatedWebAppUpdateDiscoveryTask::Error& error);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_DISCOVERY_TASK_H_
