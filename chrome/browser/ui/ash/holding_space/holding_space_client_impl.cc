// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {

namespace {

// Helpers ---------------------------------------------------------------------

// Returns the `HoldingSpaceKeyedService` associated with the given `profile`.
HoldingSpaceKeyedService* GetHoldingSpaceKeyedService(Profile* profile) {
  return HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);
}

}  // namespace

// HoldingSpaceClientImpl ------------------------------------------------------

HoldingSpaceClientImpl::HoldingSpaceClientImpl(Profile* profile)
    : profile_(profile) {}

HoldingSpaceClientImpl::~HoldingSpaceClientImpl() = default;

void HoldingSpaceClientImpl::AddScreenshot(const base::FilePath& file_path) {
  GetHoldingSpaceKeyedService(profile_)->AddScreenshot(file_path);
}

// TODO(crbug/1126274): Implement.
void HoldingSpaceClientImpl::CopyToClipboard(const HoldingSpaceItem& item,
                                             SuccessCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(/*success=*/false);
}

void HoldingSpaceClientImpl::OpenItem(const HoldingSpaceItem& item,
                                      SuccessCallback callback) {
  if (item.file_path().empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  file_manager::util::OpenItem(
      profile_, item.file_path(), platform_util::OPEN_FILE,
      base::BindOnce(
          [](SuccessCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

void HoldingSpaceClientImpl::ShowItemInFolder(const HoldingSpaceItem& item,
                                              SuccessCallback callback) {
  if (item.file_path().empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  file_manager::util::ShowItemInFolder(
      profile_, item.file_path(),
      base::BindOnce(
          [](SuccessCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

void HoldingSpaceClientImpl::PinItem(const HoldingSpaceItem& item) {
  DCHECK_NE(item.type(), HoldingSpaceItem::Type::kPinnedFile);
  const storage::FileSystemURL& file_system_url =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId)
          ->CrackURL(item.file_system_url());
  GetHoldingSpaceKeyedService(profile_)->AddPinnedFile(file_system_url);
}

void HoldingSpaceClientImpl::UnpinItem(const HoldingSpaceItem& item) {
  const storage::FileSystemURL& file_system_url =
      file_manager::util::GetFileSystemContextForExtensionId(
          profile_, file_manager::kFileManagerAppId)
          ->CrackURL(item.file_system_url());
  DCHECK(GetHoldingSpaceKeyedService(profile_)->ContainsPinnedFile(
      file_system_url));
  GetHoldingSpaceKeyedService(profile_)->RemovePinnedFile(file_system_url);
}

}  // namespace ash
