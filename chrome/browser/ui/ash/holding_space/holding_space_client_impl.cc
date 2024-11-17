// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/clipboard/clipboard_util.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "components/drive/drive_pref_names.h"
#include "components/prefs/pref_service.h"
#include "net/base/mime_util.h"
#include "storage/browser/file_system/file_system_context.h"

namespace ash {
namespace {

using ItemLaunchFailureReason = holding_space_metrics::ItemLaunchFailureReason;

// Helpers ---------------------------------------------------------------------

// Returns the `HoldingSpaceKeyedService` associated with the given `profile`.
HoldingSpaceKeyedService* GetHoldingSpaceKeyedService(Profile* profile) {
  return HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile);
}

// Returns file info for the specified `file_path` or `std::nullopt` in the
// event that file info cannot be obtained.
using GetFileInfoCallback =
    base::OnceCallback<void(const std::optional<base::File::Info>&)>;
void GetFileInfo(Profile* profile,
                 const base::FilePath& file_path,
                 GetFileInfoCallback callback) {
  scoped_refptr<storage::FileSystemContext> file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  file_manager::util::GetMetadataForPath(
      file_system_context, file_path,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize},
      base::BindOnce(
          [](GetFileInfoCallback callback, base::File::Error error,
             const base::File::Info& info) {
            std::move(callback).Run(error == base::File::FILE_OK
                                        ? std::make_optional<>(info)
                                        : std::nullopt);
          },
          std::move(callback)));
}

// Opens an in-progress item and returns the reason for failure if any. Returns
// `std::nullopt` if successful. Runs the command `kOpenItem` if there is one;
// otherwise, opens `item` when the underlying download completes.
std::optional<ItemLaunchFailureReason> OpenInProgressItem(
    Profile* profile,
    const HoldingSpaceItem& item,
    holding_space_metrics::EventSource event_source) {
  CHECK(!item.progress().IsComplete());

  auto command_iter = base::ranges::find(
      item.in_progress_commands(), HoldingSpaceCommandId::kOpenItem,
      &HoldingSpaceItem::InProgressCommand::command_id);
  if (command_iter != item.in_progress_commands().end()) {
    command_iter->handler.Run(&item, command_iter->command_id, event_source);
    return std::nullopt;
  }

  return GetHoldingSpaceKeyedService(profile)->OpenItemWhenComplete(&item);
}

// Returns the reason for failing to launch a holding space item for the
// specified open operation `result`. Returns `std::nullopt` on success.
std::optional<ItemLaunchFailureReason> ToItemLaunchFailureReason(
    platform_util::OpenOperationResult result) {
  switch (result) {
    case platform_util::OpenOperationResult::OPEN_SUCCEEDED:
      return std::nullopt;
    case platform_util::OpenOperationResult::OPEN_FAILED_PATH_NOT_FOUND:
      return ItemLaunchFailureReason::kPathNotFound;
    case platform_util::OpenOperationResult::OPEN_FAILED_INVALID_TYPE:
      return ItemLaunchFailureReason::kInvalidType;
    case platform_util::OpenOperationResult::
        OPEN_FAILED_NO_HANLDER_FOR_FILE_TYPE:
      return ItemLaunchFailureReason::kNoHandlerForFileType;
    case platform_util::OpenOperationResult::OPEN_FAILED_FILE_ERROR:
      return ItemLaunchFailureReason::kFileError;
  }
}

}  // namespace

// HoldingSpaceClientImpl ------------------------------------------------------

HoldingSpaceClientImpl::HoldingSpaceClientImpl(Profile* profile)
    : profile_(profile) {}

HoldingSpaceClientImpl::~HoldingSpaceClientImpl() = default;

const std::string& HoldingSpaceClientImpl::AddItemOfType(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) {
  return GetHoldingSpaceKeyedService(profile_)->AddItemOfType(type, file_path);
}

void HoldingSpaceClientImpl::CopyImageToClipboard(
    const HoldingSpaceItem& item,
    holding_space_metrics::EventSource event_source,
    SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      {&item}, holding_space_metrics::ItemAction::kCopy, event_source);

  std::string ext = item.file().file_path.Extension();
  std::string mime_type;
  if (ext.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(ext.substr(1), &mime_type) ||
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
                     item.file().file_path),
      base::BindOnce(
          [](SuccessCallback callback) {
            // We don't currently receive a signal regarding whether image
            // decoding was successful or not. For the time being, assume
            // success when the task runs until proven otherwise.
            std::move(callback).Run(/*success=*/true);
          },
          std::move(callback)));
}

base::FilePath HoldingSpaceClientImpl::CrackFileSystemUrl(
    const GURL& file_system_url) const {
  return file_manager::util::GetFileManagerFileSystemContext(profile_)
      ->CrackURLInFirstPartyContext(file_system_url)
      .path();
}

bool HoldingSpaceClientImpl::IsDriveDisabled() const {
  return profile_->GetPrefs()->GetBoolean(drive::prefs::kDisableDrive);
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
    holding_space_metrics::EventSource event_source,
    SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      items, holding_space_metrics::ItemAction::kLaunch, event_source);

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
    if (item->file().file_path.empty()) {
      holding_space_metrics::RecordItemLaunchFailure(
          item->type(), item->file().file_path,
          ItemLaunchFailureReason::kPathEmpty);
      *complete_success_ptr = false;
      barrier_closure.Run();
      continue;
    }
    if (!item->progress().IsComplete()) {
      const std::optional<ItemLaunchFailureReason> failure_to_launch_reason =
          OpenInProgressItem(profile_, *item, event_source);
      if (failure_to_launch_reason) {
        holding_space_metrics::RecordItemLaunchFailure(
            item->type(), item->file().file_path,
            failure_to_launch_reason.value());
      }
      *complete_success_ptr &= !failure_to_launch_reason.has_value();
      barrier_closure.Run();
      continue;
    }
    GetFileInfo(
        profile_, item->file().file_path,
        base::BindOnce(
            [](const base::WeakPtr<HoldingSpaceClientImpl>& weak_ptr,
               base::RepeatingClosure barrier_closure, bool* complete_success,
               const base::FilePath& file_path, HoldingSpaceItem::Type type,
               const std::optional<base::File::Info>& info) {
              if (!weak_ptr || !info.has_value()) {
                holding_space_metrics::RecordItemLaunchFailure(
                    type, file_path,
                    weak_ptr ? ItemLaunchFailureReason::kFileInfoError
                             : ItemLaunchFailureReason::kShutdown);
                *complete_success = false;
                barrier_closure.Run();
                return;
              }
              if (!info->size) {
                holding_space_metrics::RecordItemLaunchEmpty(type, file_path);
              }
              file_manager::util::OpenItem(
                  weak_ptr->profile_, file_path,
                  info.value().is_directory ? platform_util::OPEN_FOLDER
                                            : platform_util::OPEN_FILE,
                  base::BindOnce(
                      [](base::RepeatingClosure barrier_closure,
                         bool* complete_success, HoldingSpaceItem::Type type,
                         const base::FilePath& file_path,
                         platform_util::OpenOperationResult result) {
                        const bool success =
                            result == platform_util::OPEN_SUCCEEDED;
                        if (!success) {
                          holding_space_metrics::RecordItemLaunchFailure(
                              type, file_path,
                              ToItemLaunchFailureReason(result).value());
                          *complete_success = false;
                        }
                        barrier_closure.Run();
                      },
                      barrier_closure, complete_success, type, file_path));
            },
            weak_factory_.GetWeakPtr(), barrier_closure, complete_success_ptr,
            item->file().file_path, item->type()));
  }
}

void HoldingSpaceClientImpl::OpenMyFiles(SuccessCallback callback) {
  auto file_path = file_manager::util::GetMyFilesFolderForProfile(profile_);
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

void HoldingSpaceClientImpl::PinFiles(
    const std::vector<base::FilePath>& file_paths,
    holding_space_metrics::EventSource event_source) {
  std::vector<storage::FileSystemURL> file_system_urls;

  for (const base::FilePath& file_path : file_paths) {
    const GURL crack_url =
        holding_space_util::ResolveFileSystemUrl(profile_, file_path);
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileManagerFileSystemContext(profile_)
            ->CrackURLInFirstPartyContext(crack_url);
    file_system_urls.push_back(file_system_url);
  }

  if (!file_system_urls.empty()) {
    GetHoldingSpaceKeyedService(profile_)->AddPinnedFiles(file_system_urls,
                                                          event_source);
  }
}

void HoldingSpaceClientImpl::PinItems(
    const std::vector<const HoldingSpaceItem*>& items,
    holding_space_metrics::EventSource event_source) {
  std::vector<storage::FileSystemURL> file_system_urls;

  // NOTE: In-progress holding space items are neither pin- nor unpin-able.
  HoldingSpaceKeyedService* service = GetHoldingSpaceKeyedService(profile_);
  for (const HoldingSpaceItem* item : items) {
    if (!item->progress().IsComplete()) {
      continue;
    }
    const GURL& crack_url = item->file().file_system_url;
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileManagerFileSystemContext(profile_)
            ->CrackURLInFirstPartyContext(crack_url);
    if (!service->ContainsPinnedFile(file_system_url)) {
      file_system_urls.push_back(file_system_url);
    }
  }

  if (!file_system_urls.empty()) {
    service->AddPinnedFiles(file_system_urls, event_source);
  }
}

void HoldingSpaceClientImpl::RefreshSuggestions() {
  GetHoldingSpaceKeyedService(profile_)->RefreshSuggestions();
}

void HoldingSpaceClientImpl::RemoveSuggestions(
    const std::vector<base::FilePath>& absolute_file_paths) {
  GetHoldingSpaceKeyedService(profile_)->RemoveSuggestions(absolute_file_paths);
}

void HoldingSpaceClientImpl::ShowItemInFolder(
    const HoldingSpaceItem& item,
    holding_space_metrics::EventSource event_source,
    SuccessCallback callback) {
  holding_space_metrics::RecordItemAction(
      {&item}, holding_space_metrics::ItemAction::kShowInFolder, event_source);

  if (item.file().file_path.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  file_manager::util::ShowItemInFolder(
      profile_, item.file().file_path,
      base::BindOnce(
          [](SuccessCallback callback,
             platform_util::OpenOperationResult result) {
            const bool success = result == platform_util::OPEN_SUCCEEDED;
            std::move(callback).Run(success);
          },
          std::move(callback)));
}

void HoldingSpaceClientImpl::UnpinItems(
    const std::vector<const HoldingSpaceItem*>& items,
    holding_space_metrics::EventSource event_source) {
  std::vector<storage::FileSystemURL> file_system_urls;

  // NOTE: In-progress holding space items are neither pin- nor unpin-able.
  HoldingSpaceKeyedService* service = GetHoldingSpaceKeyedService(profile_);
  for (const HoldingSpaceItem* item : items) {
    if (!item->progress().IsComplete()) {
      continue;
    }
    const GURL& crack_url = item->file().file_system_url;
    const storage::FileSystemURL& file_system_url =
        file_manager::util::GetFileManagerFileSystemContext(profile_)
            ->CrackURLInFirstPartyContext(crack_url);
    if (service->ContainsPinnedFile(file_system_url)) {
      file_system_urls.push_back(file_system_url);
    }
  }

  if (!file_system_urls.empty()) {
    service->RemovePinnedFiles(file_system_urls, event_source);
  }
}

}  // namespace ash
