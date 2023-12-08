// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_controller.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/check.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

namespace ash::download_status {

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
  const HoldingSpaceProgress progress(display_metadata.received_bytes,
                                      display_metadata.total_bytes);

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

  // TODO(http://b/307347158): Update the holding space item specified by
  // `holding_space_item_id` with `display_metadata`.
  service->UpdateItem(item_id_by_guid->second)
      ->SetProgress(progress)
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
