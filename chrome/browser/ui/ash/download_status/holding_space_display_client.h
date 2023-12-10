// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_

#include <map>
#include <string>

#include "chrome/browser/ui/ash/download_status/display_client.h"

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
  // DisplayClient:
  void AddOrUpdate(const std::string& guid,
                   const DisplayMetadata& display_metadata) override;
  void Remove(const std::string& guid) override;

  // GUID to holding space item ID mappings.
  // Adds a mapping when displaying a new download.
  // Removes a mapping when:
  // 1. A displayed download is removed; OR
  // 2. An in-progress download completes.
  std::map<std::string, std::string> item_ids_by_guids_;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_HOLDING_SPACE_DISPLAY_CLIENT_H_
