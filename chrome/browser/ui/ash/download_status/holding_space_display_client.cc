// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"
#include "url/gurl.h"

namespace ash::download_status {

namespace {

// Returns the command ID corresponding to the given command type.
// NOTE: It is fine to map both `CommandType::kOpenFile` and
// `CommandType::kShowInBrowser` to `kOpenItem`, because `kOpenItem` is not
// accessible from a holding space chip's context menu.
HoldingSpaceCommandId ConvertCommandTypeToId(CommandType type) {
  switch (type) {
    case CommandType::kCancel:
      return HoldingSpaceCommandId::kCancelItem;
    case CommandType::kOpenFile:
      return HoldingSpaceCommandId::kOpenItem;
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

}  // namespace

HoldingSpaceDisplayClient::HoldingSpaceDisplayClient(Profile* profile)
    : DisplayClient(profile) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
}

HoldingSpaceDisplayClient::~HoldingSpaceDisplayClient() = default;

void HoldingSpaceDisplayClient::AddOrUpdate(
    const std::string& guid,
    const DisplayMetadata& display_metadata) {
  // Point to the mapping from `guid` to the holding space item ID.
  auto item_id_by_guid = item_ids_by_guids_.find(guid);

  HoldingSpaceKeyedService* const service =
      HoldingSpaceKeyedServiceFactory::GetInstance()->GetService(profile());

  // Create a `HoldingSpaceProgress` instance from a `Progress` instance.
  const Progress& download_progress = display_metadata.progress;
  const HoldingSpaceProgress progress(
      download_progress.received_bytes(), download_progress.total_bytes(),
      download_progress.complete(), download_progress.hidden());

  if (item_id_by_guid == item_ids_by_guids_.end() ||
      !HoldingSpaceController::Get()->model()->GetItem(
          item_id_by_guid->second)) {
    // Create a holding space item when displaying a new download. A download is
    // considered new if:
    // 1. The key `guid` does not exist in `item_ids_by_guids_`; OR
    // 2. The item specified by the ID associated with `guid` is not found.
    // NOTE: Adding a new download holding space item may not always be
    // successful. For example, item additions should be avoided during service
    // suspension.
    std::string id =
        service->AddItemOfType(HoldingSpaceItem::Type::kLacrosDownload,
                               display_metadata.file_path, progress);
    item_id_by_guid = id.empty() ? item_ids_by_guids_.end()
                                 : item_ids_by_guids_.insert_or_assign(
                                       item_id_by_guid, guid, std::move(id));
  }

  if (item_id_by_guid == item_ids_by_guids_.end()) {
    return;
  }

  // Generate in-progress commands from `display_metadata`.
  std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands;
  for (const auto& command_info : display_metadata.command_infos) {
    if (const HoldingSpaceCommandId id =
            ConvertCommandTypeToId(command_info.type);
        holding_space_util::IsInProgressCommand(id)) {
      in_progress_commands.emplace_back(
          id, command_info.text_id, command_info.icon,
          base::IgnoreArgs<const HoldingSpaceItem*, HoldingSpaceCommandId,
                           holding_space_metrics::EventSource>(
              command_info.command_callback));
    }
  }

  // Specify the backing file.
  const base::FilePath& file_path = display_metadata.file_path;
  const GURL file_system_url =
      holding_space_util::ResolveFileSystemUrl(profile(), file_path);

  service->UpdateItem(item_id_by_guid->second)
      ->SetBackingFile(HoldingSpaceFile(
          file_path,
          holding_space_util::ResolveFileSystemType(profile(), file_system_url),
          file_system_url))
      .SetInProgressCommands(std::move(in_progress_commands))
      .SetProgress(progress)
      .SetSecondaryText(display_metadata.secondary_text)
      .SetText(display_metadata.text);

  // Since `item_ids_by_guids_` no longer needs `guid` after the download
  // specified by `guid` completes, remove `guid` from `item_ids_by_guids_`.
  if (progress.IsComplete()) {
    item_ids_by_guids_.erase(item_id_by_guid);
  }
}

void HoldingSpaceDisplayClient::Remove(const std::string& guid) {
  if (auto iter = item_ids_by_guids_.find(guid);
      iter != item_ids_by_guids_.end()) {
    HoldingSpaceKeyedServiceFactory::GetInstance()
        ->GetService(profile())
        ->RemoveItem(iter->second);
    item_ids_by_guids_.erase(iter);
  }
}

}  // namespace ash::download_status
