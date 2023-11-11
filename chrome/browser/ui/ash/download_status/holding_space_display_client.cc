// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/holding_space_display_client.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_progress.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

namespace ash::download_status {

HoldingSpaceDisplayClient::HoldingSpaceDisplayClient(Profile* profile)
    : profile_(profile) {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
  CHECK(profile_);

  profile_observation_.Observe(profile);
}

HoldingSpaceDisplayClient::~HoldingSpaceDisplayClient() = default;

void HoldingSpaceDisplayClient::AddOrUpdate(
    const std::string& guid,
    const DisplayMetadata& display_metadata) {
  if (!base::Contains(item_ids_by_guids_, guid)) {
    // Create a holding space item when displaying a new download.
    std::string id =
        HoldingSpaceKeyedServiceFactory::GetInstance()
            ->GetService(profile_)
            ->AddItemOfType(HoldingSpaceItem::Type::kLacrosDownload,
                            display_metadata.file_path);
    if (!id.empty()) {
      item_ids_by_guids_.emplace(guid, std::move(id));
    }
  }

  // TODO(http://b/308213441): Handle the case where the holding space item
  // specified by `guid` already exists.

  // TODO(http://b/307347158): Update the holding space item specified by `guid`
  // with `display_metadata`.

  // Since `item_ids_by_guids_` no longer needs `guid` after the download
  // specified by `guid` completes, remove `guid` from `item_ids_by_guids_`.
  if (HoldingSpaceProgress(display_metadata.received_bytes,
                           display_metadata.total_bytes)
          .IsComplete()) {
    item_ids_by_guids_.erase(guid);
  }
}

void HoldingSpaceDisplayClient::Remove(const std::string& guid) {
  if (auto iter = item_ids_by_guids_.find(guid);
      iter != item_ids_by_guids_.end()) {
    HoldingSpaceKeyedServiceFactory::GetInstance()
        ->GetService(profile_)
        ->RemoveItem(iter->second);
    item_ids_by_guids_.erase(iter);
  }
}

void HoldingSpaceDisplayClient::OnProfileWillBeDestroyed(Profile* profile) {
  profile_observation_.Reset();
  profile_ = nullptr;
}

}  // namespace ash::download_status
