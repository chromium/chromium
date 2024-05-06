// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/ash/download_status/display_client.h"
#include "chromeos/crosapi/mojom/download_status_updater.mojom.h"

class Profile;

namespace ash::download_status {

struct DisplayMetadata;

// The client to display downloads in holding space. Created only when the
// downloads integration V2 feature is enabled.
class HoldingSpaceDisplayClient : public DisplayClient {
 public:
  explicit HoldingSpaceDisplayClient(Profile* profile);
  HoldingSpaceDisplayClient(const HoldingSpaceDisplayClient&) = delete;
  HoldingSpaceDisplayClient& operator=(const HoldingSpaceDisplayClient&) =
      delete;
  ~HoldingSpaceDisplayClient() override;

 private:
  // The data used during download updates.
  struct UpdateMetadata final {
    UpdateMetadata();
    UpdateMetadata(const UpdateMetadata&) = delete;
    UpdateMetadata& operator=(const UpdateMetadata&) = delete;
    ~UpdateMetadata();

    // The ID of the download's associated holding space item.
    std::string item_id;

    // The nullable icons that override the default holding space icon.
    crosapi::mojom::DownloadStatusIconsPtr icons;

    base::WeakPtr<UpdateMetadata> AsWeakPtr() {
      return weak_ptr_factory_.GetWeakPtr();
    }

   private:
    base::WeakPtrFactory<UpdateMetadata> weak_ptr_factory_{this};
  };

  // DisplayClient:
  void AddOrUpdate(const std::string& guid,
                   const DisplayMetadata& display_metadata) override;
  void Remove(const std::string& guid) override;

  // Maps update metadata by GUIDs.
  // Adds a mapping when displaying a new download.
  // Removes a mapping when a download does not update anymore, which includes:
  // 1. A displayed download is removed
  // 2. An in-progress download completes.
  std::map<std::string, UpdateMetadata> metadata_by_guids_;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_
