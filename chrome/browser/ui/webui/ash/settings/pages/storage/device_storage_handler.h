// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_STORAGE_DEVICE_STORAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_STORAGE_DEVICE_STORAGE_HANDLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/webui/ash/settings/calculator/size_calculator.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "third_party/re2/src/re2/re2.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

namespace ash::settings {

// Enumeration for device state about remaining space. These values must be
// kept in sync with settings.StorageSpaceState in JS code.
enum class StorageSpaceState {
  kStorageSpaceNormal = 0,
  kStorageSpaceLow = 1,
  kStorageSpaceCriticallyLow = 2,
};

// Threshold to show a message indicating space is critically low (512 MB).
const int64_t kSpaceCriticallyLowBytes = 512 * 1024 * 1024;

// Threshold to show a message indicating space is low (1 GB).
const int64_t kSpaceLowBytes = 1 * 1024 * 1024 * 1024;

class StorageHandler : public ::settings::SettingsPageUIHandler,
                       public arc::ArcSessionManagerObserver,
                       public disks::DiskMountManager::Observer,
                       public SizeCalculator::Observer {
 public:
  StorageHandler(Profile* profile, content::WebUIDataSource* html_source);

  StorageHandler(const StorageHandler&) = delete;
  StorageHandler& operator=(const StorageHandler&) = delete;

  ~StorageHandler() override;

  // ::settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // disks::DiskMountManager::Observer:
  void OnMountEvent(
      disks::DiskMountManager::MountEvent event,
      MountError error_code,
      const disks::DiskMountManager::MountPoint& mount_info) override;

  // SizeCalculator::Observer:
  void OnSizeCalculated(const SizeCalculator::CalculationType& calculation_type,
                        int64_t total_bytes) override;

  // Removes the handler from the list of observers of every observed instances.
  void StopObservingEvents();

 private:
  // Handlers of JS messages.
  void HandleUpdateAndroidEnabled(const base::Value::List& unused_args);
  void HandleUpdateStorageInfo(const base::Value::List& unused_args);
  void HandleOpenMyFiles(const base::Value::List& unused_args);
  void HandleUpdateExternalStorages(const base::Value::List& unused_args);
  void HandleOpenBrowsingDataSettings(const base::Value::List& unused_args);
  void HandleGetStorageEncryption(const base::Value::List& args);

  // Updates storage row on the UI.
  void UpdateStorageItem(
      const SizeCalculator::CalculationType& calculation_type);

  // Updates global storage statistics: total, in use and available space.
  void UpdateOverallStatistics();

  // Checks whether all storage items have been calculated. If so, calculates
  // and updates the "System" size.
  void UpdateSystemSizeItem();

  // Updates list of external storages.
  void UpdateExternalStorages();

  // Returns true if the volume from |source_path| can be used as Android
  // storage.
  bool IsEligibleForAndroidStorage(std::string source_path);

  // Update encryption type whenever it is fetched.
  void OnGetVaultProperties(
      const std::string& callback_id,
      std::optional<user_data_auth::GetVaultPropertiesReply> reply);

  // Instances calculating the size of each storage items.
  TotalDiskSpaceCalculator total_disk_space_calculator_;
  FreeDiskSpaceCalculator free_disk_space_calculator_;
  DriveOfflineSizeCalculator drive_offline_size_calculator_;
  MyFilesSizeCalculator my_files_size_calculator_;
  BrowsingDataSizeCalculator browsing_data_size_calculator_;
  AppsSizeCalculator apps_size_calculator_;
  CrostiniSizeCalculator crostini_size_calculator_;
  OtherUsersSizeCalculator other_users_size_calculator_;

  // Controls if the size of each storage item has been calculated.
  std::bitset<SizeCalculator::kCalculationTypeCount> calculation_state_;

  // Keeps track of the size of each storage item.
  int64_t storage_items_total_bytes_[SizeCalculator::kCalculationTypeCount] = {
      0};

  const raw_ptr<Profile> profile_;
  const std::string source_name_;
  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_observation_{this};
  const re2::RE2 special_volume_path_pattern_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_STORAGE_DEVICE_STORAGE_HANDLER_H_
