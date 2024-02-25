// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/files/google_drive_page_handler.h"

#include <optional>

#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash::settings {
namespace {

using drive::DriveIntegrationService;
using drivefs::pinning::PinningManager;
using drivefs::pinning::Progress;
using google_drive::mojom::Status;
using google_drive::mojom::StatusPtr;

StatusPtr CreateStatusPtr(const Progress& progress) {
  StatusPtr status = Status::New();
  status->required_space =
      (progress.required_space >= 0)
          ? base::UTF16ToUTF8(ui::FormatBytes(progress.required_space))
          : "";
  status->free_space =
      (progress.free_space >= 0)
          ? base::UTF16ToUTF8(ui::FormatBytes(progress.free_space))
          : "";
  status->stage = progress.stage;
  status->listed_files = progress.listed_files;
  status->is_error = progress.IsError();
  return status;
}

}  // namespace

GoogleDrivePageHandler::GoogleDrivePageHandler(
    mojo::PendingReceiver<google_drive::mojom::PageHandler> receiver,
    mojo::PendingRemote<google_drive::mojom::Page> page,
    Profile* profile)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {
  if (DriveIntegrationService* const service = GetDriveService()) {
    Observe(service);
  }
}

GoogleDrivePageHandler::~GoogleDrivePageHandler() = default;

void GoogleDrivePageHandler::CalculateRequiredSpace() {
  PinningManager* const pinning_manager = GetPinningManager();
  if (!pinning_manager) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(pinning_manager->GetProgress());
  if (!pinning_manager->CalculateRequiredSpace()) {
    page_->OnServiceUnavailable();
    return;
  }
}

void GoogleDrivePageHandler::NotifyProgress(const Progress& progress) {
  page_->OnProgress(CreateStatusPtr(progress));
}

DriveIntegrationService* GoogleDrivePageHandler::GetDriveService() {
  DriveIntegrationService* const service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  return service && service->IsMounted() ? service : nullptr;
}

PinningManager* GoogleDrivePageHandler::GetPinningManager() {
  DriveIntegrationService* const service = GetDriveService();
  return service ? service->GetPinningManager() : nullptr;
}

void GoogleDrivePageHandler::OnBulkPinProgress(const Progress& progress) {
  if (!GetPinningManager()) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(progress);
}

void GoogleDrivePageHandler::GetContentCacheSize(
    GetContentCacheSizeCallback callback) {
  if (!GetDriveService()) {
    page_->OnServiceUnavailable();
    std::move(callback).Run(std::nullopt);
    return;
  }

  const base::FilePath content_cache_path =
      GetDriveService()->GetDriveFsContentCachePath();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&drive::util::ComputeDriveFsContentCacheSize,
                     content_cache_path),
      base::BindOnce(&GoogleDrivePageHandler::OnGetContentCacheSize,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GoogleDrivePageHandler::OnGetContentCacheSize(
    GetContentCacheSizeCallback callback,
    int64_t size) {
  if (size < 0) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(base::UTF16ToUTF8(ui::FormatBytes(size)));
}

void GoogleDrivePageHandler::ClearPinnedFiles(
    ClearPinnedFilesCallback callback) {
  if (!GetDriveService()) {
    std::move(callback).Run();
    page_->OnServiceUnavailable();
    return;
  }

  // If Drive crashes, this callback may not get invoked so in that instance
  // ensure it gets invoked with drve::FILE_ERROR_ABORT to indicate that the
  // call has been aborted.
  auto on_clear_callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&GoogleDrivePageHandler::OnClearPinnedFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      drive::FILE_ERROR_ABORT);
  GetDriveService()->ClearOfflineFiles(std::move(on_clear_callback));
}

void GoogleDrivePageHandler::OnClearPinnedFiles(
    ClearPinnedFilesCallback callback,
    drive::FileError error) {
  std::move(callback).Run();
}

void GoogleDrivePageHandler::RecordBulkPinningEnabledMetric() {
  drivefs::pinning::RecordBulkPinningEnabledSource(
      drivefs::pinning::BulkPinningEnabledSource::kSystemSettings);
}

}  // namespace ash::settings
