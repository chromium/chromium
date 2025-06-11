// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/cloud_synced_folder_checker.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/browser/win/installer_downloader/system_info_provider.h"
#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_url_parameters.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/download_manager.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace installer_downloader {

class InstallerDownloaderObserver final
    : public download::DownloadItem::Observer {
 public:
  InstallerDownloaderObserver(download::DownloadItem* item,
                              CompletionCallback completion_callback)
      : completion_callback_(std::move(completion_callback)) {
    observation_.Observe(item);
  }

  InstallerDownloaderObserver(const InstallerDownloaderObserver&) = delete;
  InstallerDownloaderObserver& operator=(const InstallerDownloaderObserver&) =
      delete;

 private:
  void OnDownloadUpdated(download::DownloadItem* item) override {
    CHECK_EQ(observation_.GetSource(), item);

    switch (item->GetState()) {
      case download::DownloadItem::COMPLETE:
        // `this` is deleted by `completion_callback_`, so nothing below this
        // point may access it.
        std::move(completion_callback_).Run(/*succeeded=*/true);
        break;
      case download::DownloadItem::IN_PROGRESS:
        break;
      case download::DownloadItem::INTERRUPTED:
      case download::DownloadItem::CANCELLED:
        // `this` is deleted by `completion_callback_`, so nothing below this
        // point may access it.
        std::move(completion_callback_).Run(/*succeeded=*/false);
        break;
      case download::DownloadItem::MAX_DOWNLOAD_STATE:
        NOTREACHED();
    }
  }

  void OnDownloadDestroyed(download::DownloadItem* item) override {
    CHECK_EQ(observation_.GetSource(), item);
    std::move(completion_callback_).Run(/*succeeded=*/false);
  }

  base::ScopedObservation<download::DownloadItem,
                          download::DownloadItem::Observer>
      observation_{this};
  CompletionCallback completion_callback_;
};

InstallerDownloaderModelImpl::InstallerDownloaderModelImpl(
    std::unique_ptr<SystemInfoProvider> system_info_provider)
    : system_info_provider_(std::move(system_info_provider)) {}

InstallerDownloaderModelImpl::~InstallerDownloaderModelImpl() = default;

void InstallerDownloaderModelImpl::CheckEligibility(
    EligibilityCheckCallback callback) {
  if (!system_info_provider_->IsOsEligible()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }

  // Installer Downloader is a global feature, therefore it's guaranteed that
  // InstallerDownloaderModelImpl will be alive at any point during the browser
  // runtime.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&InstallerDownloaderModelImpl::GetInstallerDestination,
                     base::Unretained(this)),
      std::move(callback));
}

void InstallerDownloaderModelImpl::StartDownload(
    const GURL& url,
    const base::FilePath& destination,
    content::DownloadManager& download_manager,
    CompletionCallback completion_callback) {
  CHECK(url.is_valid());

  static constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation("windows_installer_downloader",
                                          R"(semantics {
          sender: "Windows Installer Downloader"
          description:
            "Download Chrome installer to the user OneDrive folder on "
            "their consent."
          trigger: "Once, when the user accept the download request."
          data: "None."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
            owners: "//chrome/browser/win/installer_downloader/OWNERS"
          }
        }
        last_reviewed: "2025-05-01
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can controller this feature by closing the Installer"
          "Downloader Infobar."
    })");

  auto params = std::make_unique<download::DownloadUrlParameters>(
      url, kTrafficAnnotation);
  params->set_file_path(destination);
  params->set_transient(true);
  params->set_download_source(download::DownloadSource::INTERNAL_API);

  // The InstallerDownloaderController that hold this model is a browser global
  // feature. Therefore, it is safe to use base::Unretained here.
  params->set_callback(base::BindOnce(
      &InstallerDownloaderModelImpl::OnInstallerDownloadCreated,
      base::Unretained(this), destination, std::move(completion_callback)));

  download_manager.DownloadUrl(std::move(params));
}

bool InstallerDownloaderModelImpl::CanShowInfobar() const {
  const PrefService* local_state = g_browser_process->local_state();
  if (local_state->GetBoolean(
          prefs::kInstallerDownloaderPreventFutureDisplay)) {
    return false;
  }

  return local_state->GetInteger(prefs::kInstallerDownloaderInfobarShowCount) <
         kMaxShowCount;
}

void InstallerDownloaderModelImpl::IncrementShowCount() {
  PrefService* local_state = g_browser_process->local_state();
  local_state->SetInteger(
      prefs::kInstallerDownloaderInfobarShowCount,
      local_state->GetInteger(prefs::kInstallerDownloaderInfobarShowCount) + 1);
}

void InstallerDownloaderModelImpl::PreventFutureDisplay() {
  g_browser_process->local_state()->SetBoolean(
      prefs::kInstallerDownloaderPreventFutureDisplay, true);
}

bool InstallerDownloaderModelImpl::ShouldByPassEligibilityCheck() const {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kInstallerDownloaderBypassEligibilityCheck);
}

void InstallerDownloaderModelImpl::OnInstallerDownloadCreated(
    const base::FilePath& expected_path,
    CompletionCallback completion_callback,
    download::DownloadItem* item,
    download::DownloadInterruptReason reason) {
  CHECK(!installer_downloader_observer_);

  // If `reason` is anything other than NONE (or we never got a DownloadItem)
  // will be treated as a failure.
  if (reason != download::DOWNLOAD_INTERRUPT_REASON_NONE || !item) {
    std::move(completion_callback).Run(/*succeeded=*/false);
    return;
  }

  // Did DownloadManager keep exactly the path we requested?
  base::UmaHistogramBoolean("Windows.InstallerDownloader.DestinationMatches",
                            item->GetFullPath() == expected_path);

  // The InstallerDownloaderController that hold this model is a browser global
  // feature. Therefore, it is safe to use base::Unretained here.
  installer_downloader_observer_ =
      std::make_unique<InstallerDownloaderObserver>(
          item, base::BindOnce(
                    &InstallerDownloaderModelImpl::OnInstallerDownloadFinished,
                    base::Unretained(this), std::move(completion_callback)));
}

void InstallerDownloaderModelImpl::OnInstallerDownloadFinished(
    CompletionCallback completion_callback,
    bool succeeded) {
  installer_downloader_observer_.reset();
  std::move(completion_callback).Run(succeeded);
}

std::optional<base::FilePath>
InstallerDownloaderModelImpl::GetInstallerDestination() const {
  // 1) If this machine already meets the Win‑11 hardware requirements, an
  //    in‑place upgrade is possible, so there is no need for a standalone
  //    installer.
  if (system_info_provider_->IsHardwareEligibleForWin11()) {
    return std::nullopt;
  }

  // 2) Check OneDrive sync status (root + Desktop).
  cloud_synced_folder_checker::CloudSyncStatus cloud_sync_status =
      system_info_provider_->EvaluateOneDriveSyncStatus();

  // 3) Prefer the syncing Desktop folder; otherwise fall back to OneDrive root.
  if (cloud_sync_status.desktop_path.has_value()) {
    return std::move(cloud_sync_status.desktop_path);
  }

  return std::move(cloud_sync_status.one_drive_path);
}

}  // namespace installer_downloader
