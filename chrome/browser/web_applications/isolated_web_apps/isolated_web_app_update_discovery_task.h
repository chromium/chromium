// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_DISCOVERY_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_ISOLATED_WEB_APP_UPDATE_DISCOVERY_TASK_H_

#include <memory>

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
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/webapps/common/web_app_id.h"
#include "net/base/net_errors.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

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
      GURL update_manifest_url,
      IsolatedWebAppUrlInfo url_info,
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

  const IsolatedWebAppUrlInfo& url_info() const { return url_info_; }

  base::Value AsDebugValue() const;

 private:
  void SucceedWith(Success success);

  void FailWith(Error error);

  void OnUpdateManifestFetched(
      base::expected<UpdateManifest, UpdateManifestFetcher::Error>
          update_manifest);

  void GetDownloadPath(UpdateManifest::VersionEntry version_entry);

  void OnGetDownloadPath(UpdateManifest::VersionEntry version_entry,
                         absl::optional<base::FilePath> download_path);

  void OnWebBundleDownloaded(const base::FilePath& download_path,
                             const base::Version& expected_version,
                             int32_t net_error);

  void OnUpdateDryRunDone(
      base::expected<void, IsolatedWebAppUpdatePrepareAndStoreCommandError>
          result);

  base::Value::Dict debug_log_;
  bool has_started_ = false;
  CompletionCallback callback_;

  GURL update_manifest_url_;
  IsolatedWebAppUrlInfo url_info_;

  raw_ref<WebAppCommandScheduler> command_scheduler_;
  raw_ref<WebAppRegistrar> registrar_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

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
