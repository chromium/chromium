// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_client_impl.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_thumbnail_loader.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class GURL;

namespace base {
class FilePath;
}  // namespace base

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ash {

// Browser context keyed service that:
// *   Manages the temporary holding space per-profile data model.
// *   Serves as an entry point to add holding space items from Chrome.
class HoldingSpaceKeyedService : public KeyedService,
                                 public ProfileManagerObserver,
                                 public chromeos::PowerManagerClient::Observer {
 public:
  HoldingSpaceKeyedService(Profile* profile, const AccountId& account_id);
  HoldingSpaceKeyedService(const HoldingSpaceKeyedService& other) = delete;
  HoldingSpaceKeyedService& operator=(const HoldingSpaceKeyedService& other) =
      delete;
  ~HoldingSpaceKeyedService() override;

  // Registers profile preferences for holding space.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Adds multiple pinned file items identified by the provided file system
  // URLs.
  void AddPinnedFiles(
      const std::vector<storage::FileSystemURL>& file_system_urls);

  // Removes multiple pinned file items identified by the provided file system
  // URLs. No-ops for files that are not present in the holding space.
  void RemovePinnedFiles(
      const std::vector<storage::FileSystemURL>& file_system_urls);

  // Returns whether the holding space contains a pinned file identified by a
  // file system URL.
  bool ContainsPinnedFile(const storage::FileSystemURL& file_system_url) const;

  // Returns the list of pinned files in the holding space. It returns the files
  // files system URLs as GURLs.
  std::vector<GURL> GetPinnedFiles() const;

  // Adds a screenshot item backed by the provided absolute file path.
  // The path is expected to be under a mount point path recognized by the file
  // manager app (otherwise, the item will be dropped silently).
  void AddScreenshot(const base::FilePath& screenshot_path);

  // Adds a download item backed by the provided absolute file path.
  void AddDownload(const base::FilePath& download_path);

  // Adds a nearby share item backed by the provided absolute file path.
  void AddNearbyShare(const base::FilePath& nearby_share_path);

  // Adds a screen recording item backed by the provided absolute file path.
  void AddScreenRecording(const base::FilePath& screen_recording_path);

  // Adds the specified `item` to the holding space model.
  void AddItem(std::unique_ptr<HoldingSpaceItem> item);

  // Adds multiple `items` to the holding space model.
  void AddItems(std::vector<std::unique_ptr<HoldingSpaceItem>> items);

  const HoldingSpaceClient* client_for_testing() const {
    return &holding_space_client_;
  }

  const HoldingSpaceModel* model_for_testing() const {
    return &holding_space_model_;
  }

  HoldingSpaceThumbnailLoader* thumbnail_loader_for_testing() {
    return &thumbnail_loader_;
  }

 private:
  // KeyedService:
  void Shutdown() override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;

  // PowerManagerClient::Observer
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Invoked when the associated profile is ready.
  void OnProfileReady();

  // Creates and initializes holding space delegates. Called when the associated
  // profile finishes initialization, or when device suspend ends (the delegates
  // are shutdown during suspend).
  void InitializeDelegates();

  // Shuts down and destroys existing holding space delegates. Called on
  // profile shutdown, or when device suspend starts.
  void ShutdownDelegates();

  // Invoked when holding space persistence has been restored.
  void OnPersistenceRestored();

  // Pin a drive file for offline access.
  void MakeDriveItemAvailableOffline(
      const storage::FileSystemURL& file_system_url);

  Profile* const profile_;
  const AccountId account_id_;

  HoldingSpaceClientImpl holding_space_client_;
  HoldingSpaceModel holding_space_model_;

  HoldingSpaceThumbnailLoader thumbnail_loader_;

  // The `HoldingSpaceKeyedService` owns a collection of `delegates_` which are
  // each tasked with an independent area of responsibility on behalf of the
  // service. They operate autonomously of one another.
  std::vector<std::unique_ptr<HoldingSpaceKeyedServiceDelegate>> delegates_;

  base::ScopedObservation<ProfileManager, ProfileManagerObserver>
      profile_manager_observer_{this};

  base::WeakPtrFactory<HoldingSpaceKeyedService> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_KEYED_SERVICE_H_
