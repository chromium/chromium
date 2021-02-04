// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_page_handler.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crostini/crostini_disk.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace chromeos {

CrostiniInstallerPageHandler::CrostiniInstallerPageHandler(
    crostini::CrostiniInstallerUIDelegate* installer_ui_delegate,
    mojo::PendingReceiver<chromeos::crostini_installer::mojom::PageHandler>
        pending_page_handler,
    mojo::PendingRemote<chromeos::crostini_installer::mojom::Page> pending_page,
    base::OnceClosure on_page_closed)
    : installer_ui_delegate_{installer_ui_delegate},
      receiver_{this, std::move(pending_page_handler)},
      page_{std::move(pending_page)},
      on_page_closed_{std::move(on_page_closed)} {}

CrostiniInstallerPageHandler::~CrostiniInstallerPageHandler() = default;

void CrostiniInstallerPageHandler::Install(int64_t disk_size_bytes,
                                           const std::string& username) {
  crostini::CrostiniManager::RestartOptions options{};
  if (base::FeatureList::IsEnabled(chromeos::features::kCrostiniDiskResizing)) {
    options.disk_size_bytes = disk_size_bytes;
  }
  options.container_username = username;
  installer_ui_delegate_->Install(
      std::move(options),
      base::BindRepeating(&CrostiniInstallerPageHandler::OnProgressUpdate,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&CrostiniInstallerPageHandler::OnInstallFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::Cancel() {
  installer_ui_delegate_->Cancel(
      base::BindOnce(&CrostiniInstallerPageHandler::OnCanceled,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::CancelBeforeStart() {
  installer_ui_delegate_->CancelBeforeStart();
}

void CrostiniInstallerPageHandler::OnPageClosed() {
  if (on_page_closed_) {
    std::move(on_page_closed_).Run();
  }
}

void CrostiniInstallerPageHandler::OnProgressUpdate(
    crostini::mojom::InstallerState installer_state,
    double progress_fraction) {
  page_->OnProgressUpdate(installer_state, progress_fraction);
}

void CrostiniInstallerPageHandler::OnInstallFinished(
    crostini::mojom::InstallerError error) {
  page_->OnInstallFinished(error);
}

void CrostiniInstallerPageHandler::OnCanceled() {
  page_->OnCanceled();
}

void CrostiniInstallerPageHandler::RequestAmountOfFreeDiskSpace() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     base::FilePath(crostini::kHomeDirectory)),
      base::BindOnce(&CrostiniInstallerPageHandler::OnAmountOfFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniInstallerPageHandler::RequestClosePage() {
  page_->RequestClose();
}

void CrostiniInstallerPageHandler::OnAmountOfFreeDiskSpace(int64_t free_bytes) {
  int64_t max_bytes = free_bytes - crostini::disk::kDiskHeadroomBytes;

  if (max_bytes < crostini::disk::kMinimumDiskSizeBytes) {
    page_->OnAmountOfFreeDiskSpace({}, 0, false);
    return;
  }

  int64_t default_size = crostini::disk::kRecommendedDiskSizeBytes;
  if (default_size > max_bytes) {
    // Let's adjust to the mid-point.
    default_size = (max_bytes + crostini::disk::kMinimumDiskSizeBytes) / 2;
  }

  int default_index = 0;
  std::vector<crostini::mojom::DiskSliderTickPtr> ticks =
      crostini::disk::GetTicks(crostini::disk::kMinimumDiskSizeBytes,
                               default_size, max_bytes, &default_index);
  page_->OnAmountOfFreeDiskSpace(
      std::move(ticks), default_index,
      max_bytes < crostini::disk::kRecommendedDiskSizeBytes);
}

}  // namespace chromeos
