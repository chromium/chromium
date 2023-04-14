// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/files_page/google_drive_page_handler.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ui/webui/settings/ash/files_page/mojom/google_drive_handler.mojom.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash::settings {

using drivefs::pinning::Progress;

namespace {

google_drive::mojom::StatusPtr CreateStatusPtr(const Progress& progress) {
  auto status = google_drive::mojom::Status::New();
  status->required_space =
      base::UTF16ToUTF8(ui::FormatBytes(progress.required_space));
  status->remaining_space = base::UTF16ToUTF8(
      ui::FormatBytes(progress.free_space - progress.required_space));
  status->stage = progress.stage;
  return status;
}

}  // namespace

GoogleDrivePageHandler::GoogleDrivePageHandler(
    mojo::PendingReceiver<google_drive::mojom::PageHandler> receiver,
    mojo::PendingRemote<google_drive::mojom::Page> page,
    Profile* profile)
    : profile_(profile),
      page_(std::move(page)),
      receiver_(this, std::move(receiver)) {}

GoogleDrivePageHandler::~GoogleDrivePageHandler() {
  auto* const pin_manager = GetPinManager();
  if (!pin_manager || !pin_manager->HasObserver(this)) {
    return;
  }
  pin_manager->RemoveObserver(this);
}

void GoogleDrivePageHandler::CalculateRequiredSpace() {
  auto* pin_manager = GetPinManager();
  if (!pin_manager) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(pin_manager->GetProgress());
  if (!pin_manager->HasObserver(this)) {
    pin_manager->AddObserver(this);
  }
  pin_manager->CalculateRequiredSpace();
}

void GoogleDrivePageHandler::NotifyProgress(const Progress& progress) {
  page_->OnProgress(CreateStatusPtr(progress));
}

drivefs::pinning::PinManager* GoogleDrivePageHandler::GetPinManager() {
  drive::DriveIntegrationService* service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile_);
  if (!service || !service->IsMounted() || !service->GetPinManager()) {
    return nullptr;
  }
  return service->GetPinManager();
}

void GoogleDrivePageHandler::OnProgress(const Progress& progress) {
  if (progress.stage != drivefs::pin_manager_types::mojom::Stage::kSuccess &&
      !progress.IsError()) {
    return;
  }

  auto* pin_manager = GetPinManager();
  if (!pin_manager) {
    page_->OnServiceUnavailable();
    return;
  }

  NotifyProgress(progress);
}

void GoogleDrivePageHandler::OnDrop() {
  page_->OnServiceUnavailable();
}

}  // namespace ash::settings
