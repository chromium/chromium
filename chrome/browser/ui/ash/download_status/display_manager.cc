// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/download_status/display_manager.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chrome/browser/ui/ash/download_status/display_metadata.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

namespace ash::download_status {

namespace {

// Calculates the metadata to display the download update specified by
// `download_status`.
download_status::DisplayMetadata CalculateDisplayMetadata(
    const crosapi::mojom::DownloadStatus& download_status) {
  // TODO(http://b/307347158): Fill `display_metadata`.
  download_status::DisplayMetadata display_metadata;
  return display_metadata;
}

}  // namespace

DisplayManager::DisplayManager() {
  CHECK(features::IsSysUiDownloadsIntegrationV2Enabled());
}

DisplayManager::~DisplayManager() = default;

void DisplayManager::Update(
    const crosapi::mojom::DownloadStatus& download_status) {
  switch (download_status.state) {
    case crosapi::mojom::DownloadState::kCancelled:
    case crosapi::mojom::DownloadState::kInterrupted:
      for (auto& client : clients_) {
        client->Remove(download_status.guid);
      }
      return;
    case crosapi::mojom::DownloadState::kComplete:
    case crosapi::mojom::DownloadState::kInProgress: {
      const download_status::DisplayMetadata display_metadata =
          CalculateDisplayMetadata(download_status);
      for (auto& client : clients_) {
        client->AddOrUpdate(download_status.guid, display_metadata);
      }
      return;
    }
    case crosapi::mojom::DownloadState::kUnknown:
      return;
  }
}

}  // namespace ash::download_status
