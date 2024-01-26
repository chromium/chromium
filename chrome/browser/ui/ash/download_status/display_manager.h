// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_
#define CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_

#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"

namespace crosapi {

class DownloadStatusUpdaterAsh;

namespace mojom {
class DownloadStatus;
}  // namespace mojom

}  // namespace crosapi

namespace ash::download_status {

enum class CommandType;
class DisplayClient;
struct DisplayMetadata;

// Acts as an intermediary between Lacros download updates and Ash displayed
// download updates by:
// 1. Translating the Lacros download update metadata into display metadata.
// 2. Notifying clients of the latest display metadata.
// NOTE: This class is created only when the downloads integration V2 feature
// is enabled.
class DisplayManager : public ProfileObserver {
 public:
  DisplayManager(Profile* profile,
                 crosapi::DownloadStatusUpdaterAsh* download_status_updater);
  DisplayManager(const DisplayManager&) = delete;
  DisplayManager& operator=(const DisplayManager&) = delete;
  ~DisplayManager() override;

  // Updates the displayed download specified by `download_status`.
  void Update(const crosapi::mojom::DownloadStatus& download_status);

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // Calculates the metadata to display the download update specified by
  // `download_status`. This function should be called only when the specified
  // download can be displayed.
  DisplayMetadata CalculateDisplayMetadata(
      const crosapi::mojom::DownloadStatus& download_status);

  // Performs `command` on the download using the specific `param`.
  void PerformCommand(
      CommandType command,
      const std::variant</*guid=*/std::string, base::FilePath>& param);

  // Removes the displayed download specified by `guid` from all clients. No op
  // if the specified download is not displayed.
  void Remove(const std::string& guid);

  // Reset when `OnProfileWillBeDestroyed()` is called to prevent the dangling
  // pointer issue.
  raw_ptr<Profile> profile_ = nullptr;

  // Used to handle download actions, including pausing, resuming, and canceling
  // downloads. NOTE: `download_status_updater_` owns this instance.
  const raw_ptr<crosapi::DownloadStatusUpdaterAsh> download_status_updater_;

  // Responsible for displaying download updates.
  // All clients are ready when `DisplayManager` is created to ensure
  // consistency in the received display metadata among clients.
  std::vector<std::unique_ptr<DisplayClient>> clients_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  base::WeakPtrFactory<DisplayManager> weak_ptr_factory_{this};
};

}  // namespace ash::download_status

#endif  // CHROME_BROWSER_UI_ASH_DOWNLOAD_STATUS_DISPLAY_MANAGER_H_
