// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include <memory>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/file_manager/open_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/ui/ash/clipboard_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "net/base/mime_util.h"
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

void HoldingSpaceClientImpl::CopyImageToClipboard(const HoldingSpaceItem& item,
                                                  SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      {&item}, holding_space_metrics::ItemAction::kCopy);

  std::string mime_type;
  if (item.file_path().empty() ||
      !net::GetMimeTypeFromFile(item.file_path(), &mime_type) ||
      !net::MatchesMimeType(kMimeTypeImage, mime_type)) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // Reading and decoding of the image file needs to be done on an I/O thread.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&clipboard_util::ReadFileAndCopyToClipboardLocal,
                     item.file_path()),
      base::BindOnce(
          [](SuccessCallback callback) {
            // We don't currently receive a signal regarding whether image
            // decoding was successful or not. For the time being, assume
            // success when the task runs until proven otherwise.
            std::move(callback).Run(/*success=*/true);
          },
          std::move(callback)));
}

void HoldingSpaceClientImpl::OpenDownloads(SuccessCallback callback) {
  auto file_path = file_manager::util::GetDownloadsFolderForProfile(profile_);
  if (file_path.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }
  file_manager::util::OpenItem(
      profile_, file_path, platform_util::OPEN_FOLDER,
      base::BindOnce(
          [](SuccessCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

void HoldingSpaceClientImpl::OpenItems(
    const std::vector<const HoldingSpaceItem*>& items,
    SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      items, holding_space_metrics::ItemAction::kLaunch);

  if (items.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto complete_success = std::make_unique<bool>(true);
  auto* complete_success_ptr = complete_success.get();

  base::RepeatingClosure barrier_closure = base::BarrierClosure(
      items.size(),
      base::BindOnce(
          [](std::unique_ptr<bool> complete_success, SuccessCallback callback) {
            std::move(callback).Run(*complete_success);
          },
          std::move(complete_success), std::move(callback)));

  for (const HoldingSpaceItem* item : items) {
    if (item->file_path().empty()) {
      *complete_success_ptr = false;
      barrier_closure.Run();
      return;
    }
    file_manager::util::OpenItem(
        profile_, item->file_path(), platform_util::OPEN_FILE,
        base::BindOnce(
            [](base::RepeatingClosure barrier_closure, bool* complete_success,
               platform_util::OpenOperationResult result) {
              const bool success = result == platform_util::OPEN_SUCCEEDED;
              *complete_success &= success;
              barrier_closure.Run();
            },
            barrier_closure, complete_success_ptr));
  }
}

void HoldingSpaceClientImpl::ShowItemInFolder(const HoldingSpaceItem& item,
                                              SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      {&item}, holding_space_metrics::ItemAction::kShowInFolder);

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

void HoldingSpaceClientImpl::PinItems(
    const std::vector<const HoldingSpaceItem*>& items) {
  HoldingSpaceKeyedService* service = GetHoldingSpaceKeyedService(profile_);
  for (const HoldingSpaceItem* item : items) {
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileSystemContextForExtensionId(
            profile_, file_manager::kFileManagerAppId)
            ->CrackURL(item->file_system_url());
    if (!service->ContainsPinnedFile(file_system_url))
      service->AddPinnedFile(file_system_url);
  }
}

void HoldingSpaceClientImpl::UnpinItems(
    const std::vector<const HoldingSpaceItem*>& items) {
  HoldingSpaceKeyedService* service = GetHoldingSpaceKeyedService(profile_);
  for (const HoldingSpaceItem* item : items) {
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileSystemContextForExtensionId(
            profile_, file_manager::kFileManagerAppId)
            ->CrackURL(item->file_system_url());
    if (service->ContainsPinnedFile(file_system_url))
      service->RemovePinnedFile(file_system_url);
  }
}

}  // namespace ash
