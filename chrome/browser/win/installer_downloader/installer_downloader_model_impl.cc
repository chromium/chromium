// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/win/installer_downloader/installer_downloader_model_impl.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/win/cloud_synced_folder_checker.h"
#include "chrome/browser/win/installer_downloader/installer_downloader_pref_names.h"
#include "chrome/browser/win/installer_downloader/system_info_provider.h"
#include "chrome/browser/win/installer_downloader/system_info_provider_impl.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

namespace installer_downloader {

InstallerDownloaderModelImpl::InstallerDownloaderModelImpl(
    std::unique_ptr<SystemInfoProvider> system_info_provider)
    : system_info_provider_(std::move(system_info_provider)) {}

InstallerDownloaderModelImpl::~InstallerDownloaderModelImpl() = default;

void InstallerDownloaderModelImpl::CheckEligibility(
    base::OnceCallback<void(const std::optional<base::FilePath>& destination)>
        callback) {
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
    const base::FilePath& dest,
    CompletionCallback completion_callback) {}

bool InstallerDownloaderModelImpl::IsMaxShowCountReached() const {
  return g_browser_process->local_state()->GetInteger(
             prefs::kInstallerDownloaderInfobarShowCount) >= kMaxShowCount;
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
