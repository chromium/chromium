// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

class Profile;

namespace crosapi::mojom {
class DownloadStatus;
}  // namespace crosapi::mojom

namespace ash::download_status {

class DisplayClient;

// Acts as an intermediary between Lacros download updates and Ash displayed
// download updates by:
// 1. Translating the Lacros download update metadata into display metadata.
// 2. Notifying clients of the latest display metadata.
// NOTE: This class is created only when the downloads integration V2 feature
// is enabled.
// TODO(http://b/307353486): `DisplayManager` should delegate download actions,
// such as pausing the download, to `DownloadStatusUpdaterAsh` for handling.
class DisplayManager {
 public:
  explicit DisplayManager(Profile* profile);
  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;
  ~DisplayManager();

  // Updates the displayed download specified by `download_status`.
  void Update(const crosapi::mojom::DownloadStatus& download_status);

 private:
  // Removes the displayed download specified by `guid` from all clients. No op
  // if the specified download is not displayed.
  void Remove(const std::string& guid);

  // Responsible for displaying download updates.
  // All clients are ready when `DisplayManager` is created to ensure
  // consistency in the received display metadata among clients.
  std::vector<std::unique_ptr<DisplayClient>> clients_;
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_
