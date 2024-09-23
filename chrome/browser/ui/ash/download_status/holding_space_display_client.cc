// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"

#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_metrics.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "url/gurl.h"

namespace ash::download_status {

namespace {

// Returns the command ID corresponding to the given command type if any. If
// there is no such command ID, returns `std::nullopt`.
// NOTE: It is fine to map both `CommandType::kOpenFile` and
// `CommandType::kShowInBrowser` to `kOpenItem`, because `kOpenItem` is not
// accessible from a holding space chip's context menu.
std::optional<HoldingSpaceCommandId> ConvertCommandTypeToId(CommandType type) {
  switch (type) {
    case CommandType::kCancel:
      return HoldingSpaceCommandId::kCancelItem;
    case CommandType::kCopyToClipboard:
      return std::nullopt;
    case CommandType::kEditWithMediaApp:
      return std::nullopt;
    case CommandType::kOpenFile:
      return HoldingSpaceCommandId::kOpenItem;
    case CommandType::kOpenWithMediaApp:
      return std::nullopt;
    case CommandType::kPause:
      return HoldingSpaceCommandId::kPauseItem;
    case CommandType::kResume:
      return HoldingSpaceCommandId::kResumeItem;
    case CommandType::kShowInBrowser:
      return HoldingSpaceCommandId::kOpenItem;
    case CommandType::kShowInFolder:
      return HoldingSpaceCommandId::kShowInFolder;
    case CommandType::kViewDetailsInBrowser:
      return HoldingSpaceCommandId::kViewItemDetailsInBrowser;
  }
}

// Returns the holding space item action corresponding to `type` if any. If
// there is no such action, returns `std::nullopt`.
std::optional<holding_space_metrics::ItemAction> ConvertCommandTypeToAction(
    CommandType type) {
  using ItemAction = holding_space_metrics::ItemAction;
  switch (type) {
    case CommandType::kCancel:
      return ItemAction::kCancel;
    case CommandType::kCopyToClipboard:
      return std::nullopt;
    case CommandType::kEditWithMediaApp:
      return std::nullopt;
    case CommandType::kOpenFile:
      return ItemAction::kLaunch;
    case CommandType::kOpenWithMediaApp:
      return std::nullopt;
    case CommandType::kPause:
      return ItemAction::kPause;
    case CommandType::kResume:
      return ItemAction::kResume;
    case CommandType::kShowInBrowser:
      return ItemAction::kShowInBrowser;
    case CommandType::kShowInFolder:
      return ItemAction::kShowInFolder;
    case CommandType::kViewDetailsInBrowser:
      return ItemAction::kViewDetailsInBrowser;
  }
}

// Creates a holding space icon of `size` based on `icon`.
gfx::ImageSkia CreateHoldingSpaceIcon(const gfx::ImageSkia& icon,
                                      const gfx::Size& size) {
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      image_util::CreateEmptyImage(size),
      gfx::ImageSkiaOperations::CreateResizedImage(
          icon, skia::ImageOperations::ResizeMethod::RESIZE_GOOD,
          gfx::Size(kHoldingSpaceIconSize, kHoldingSpaceIconSize)));
}

}  // namespace

// HoldingSpaceDisplayClient::UpdateMetadata -----------------------------------

HoldingSpaceDisplayClient::UpdateMetadata::UpdateMetadata() = default;

HoldingSpaceDisplayClient::UpdateMetadata::~UpdateMetadata() = default;

// HoldingSpaceDisplayClient ---------------------------------------------------

HoldingSpaceDisplayClient::HoldingSpaceDisplayClient(Profile* profile)
    : DisplayClient(profile) {}

HoldingSpaceDisplayClient::~HoldingSpaceDisplayClient() = default;

void HoldingSpaceDisplayClient::AddOrUpdate(
    const std::string& guid,
    const DisplayMetadata& display_metadata) {
  // Find the mapping from `guid` to an `UpdateMetadata` instance if any.
  auto metadata_by_guid = metadata_by_guids_.find(guid);

  HoldingSpaceKeyedService* const service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile());

  // Create a `HoldingSpaceProgress` instance from a `Progress` instance.
  const Progress& download_progress = display_metadata.progress;
  const HoldingSpaceProgress progress(
      download_progress.received_bytes(), download_progress.total_bytes(),
      download_progress.complete(), download_progress.hidden());

  if (metadata_by_guid == metadata_by_guids_.end() ||
      !HoldingSpaceController::Get()->model()->GetItem(
          metadata_by_guid->second.item_id)) {
    // Create an `UpdateMetadata` associated with `guid` if not having one.
    if (metadata_by_guid == metadata_by_guids_.end()) {
      metadata_by_guid =
          metadata_by_guids_
              .emplace(std::piecewise_construct, std::forward_as_tuple(guid),
                       std::forward_as_tuple())
              .first;
    }

    // Create a holding space item when displaying a new download. A download is
    // considered new if:
    // 1. The key `guid` does not exist in `metadata_by_guids_`; OR
    // 2. The item specified by the ID associated with `guid` is not found.
    // NOTE: Adding a new download holding space item may not always be
    // successful. For example, item additions should be avoided during service
    // suspension.
    std::string id = service->AddItemOfType(
        HoldingSpaceItem::Type::kLacrosDownload, display_metadata.file_path,
        progress,
        base::BindRepeating(
            [](const base::WeakPtr<UpdateMetadata>& update_metadata,
               const base::FilePath& file_path, const gfx::Size& size,
               const std::optional<bool>& dark_background,
               const std::optional<bool>& is_folder) {
              if (const crosapi::mojom::DownloadStatusIcons* const icons =
                      update_metadata ? update_metadata->icons.get()
                                      : nullptr) {
                const gfx::ImageSkia icon =
                    dark_background.value_or(
                        DarkLightModeController::Get()->IsDarkModeEnabled())
                        ? icons->dark_mode
                        : icons->light_mode;
                return CreateHoldingSpaceIcon(icon, size);
              }

              return HoldingSpaceImage::
                  CreateDefaultPlaceholderImageSkiaResolver()
                      .Run(file_path, size, dark_background, is_folder);
            },
            metadata_by_guid->second.AsWeakPtr()));

    // Delete the mapping referred to by `metadata_by_guid` if failing to create
    // a holding space item; otherwise, update the item ID.
    if (id.empty()) {
      metadata_by_guids_.erase(metadata_by_guid);
      metadata_by_guid = metadata_by_guids_.end();
    } else {
      metadata_by_guid->second.item_id = std::move(id);
    }
  }

  if (metadata_by_guid == metadata_by_guids_.end()) {
    return;
  }

  // Update the icons cached by `UpdateMetadata`.
  UpdateMetadata& update_metadata = metadata_by_guid->second;
  bool invalidate_image = false;
  if (const auto& new_icons = display_metadata.icons) {
    update_metadata.icons = new_icons.Clone();
    invalidate_image = true;
  } else if (auto& cached_icons = update_metadata.icons) {
    cached_icons = nullptr;
    invalidate_image = true;
  }

  // Generate in-progress commands from `display_metadata`.
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  for (const auto& command_info : display_metadata.command_infos) {
    const std::optional<HoldingSpaceCommandId> id =
        ConvertCommandTypeToId(command_info.type);
    const std::optional<holding_space_metrics::ItemAction> item_action =
        ConvertCommandTypeToAction(command_info.type);

    // Skip `command_info` if:
    // 1. It does not have a corresponding ID; OR
    // 2. Its corresponding ID is not for an in-progress command; OR
    // 3. It does not have a corresponding item action.
    if (!id || !holding_space_util::IsInProgressCommand(*id) || !item_action) {
      continue;
    }

    in_progress_commands.emplace_back(
        *id, command_info.text_id, command_info.icon,
        base::BindRepeating(
            [](holding_space_metrics::ItemAction action,
               const base::RepeatingClosure& command_callback,
               const HoldingSpaceItem* item, HoldingSpaceCommandId command_id,
               holding_space_metrics::EventSource event_source) {
              command_callback.Run();
              holding_space_metrics::RecordItemAction(
                  /*items=*/{item}, action, event_source);
            },
            *item_action, command_info.command_callback));
  }

  // Specify the backing file.
  const base::FilePath& file_path = display_metadata.file_path;
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile(), file_path);

  service->UpdateItem(update_metadata.item_id)
      ->SetBackingFile(HoldingSpaceFile(
          file_path,
          holding_space_util::ResolveFileSystemType(profile(), file_system_url),
          file_system_url))
      .SetInProgressCommands(std::move(in_progress_commands))
      .SetInvalidateImage(invalidate_image)
      .SetProgress(progress)
      .SetSecondaryText(display_metadata.secondary_text)
      .SetText(display_metadata.text);

  // After a download has completed, we do not expect to receive its updates.
  // Therefore, remove the completed download's corresponding `UpdateMetadata`.
  if (progress.IsComplete()) {
    metadata_by_guids_.erase(metadata_by_guid);
  }
}

void HoldingSpaceDisplayClient::Remove(const std::string& guid) {
  if (auto iter = metadata_by_guids_.find(guid);
      iter != metadata_by_guids_.end()) {
    HoldingSpaceKeyedServiceFactory::GetInstance()
        ->GetService(profile())
        ->RemoveItem(iter->second.item_id);
    metadata_by_guids_.erase(iter);
  }
}

}  // namespace ash::download_status
