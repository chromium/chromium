// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_

#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/session/arc_session_manager_observer.h"
#include "chrome/browser/ui/webui/settings/chromeos/calculator/size_calculator.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "third_party/re2/src/re2/re2.h"

class Profile;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

namespace chromeos {
namespace settings {

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
                       public chromeos::disks::DiskMountManager::Observer,
                       public calculator::SizeCalculator::Observer {
 public:
  StorageHandler(Profile* profile, content::WebUIDataSource* html_source);
  ~StorageHandler() override;

  // ::settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // arc::ArcSessionManagerObserver:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // chromeos::disks::DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;

  // chromeos::settings::calculator::SizeCalculator::Observer:
  void OnSizeCalculated(
      const calculator::SizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes,
      const base::Optional<int64_t>& available_bytes = base::nullopt) override;

  // Remove the handler from the list of observers of every observed instances.
  void StopObservingEvents();

 protected:
  // Round a given number of bytes up to the next power of 2.
  // Ex: 14 => 16, 150 => 256.
  int64_t RoundByteSize(int64_t bytes);

 private:
  // Handlers of JS messages.
  void HandleUpdateAndroidEnabled(const base::ListValue* unused_args);
  void HandleUpdateStorageInfo(const base::ListValue* unused_args);
  void HandleOpenMyFiles(const base::ListValue* unused_args);
  void HandleOpenArcStorage(const base::ListValue* unused_args);
  void HandleUpdateExternalStorages(const base::ListValue* unused_args);

  // Update storage sizes on the UI.
  void UpdateStorageItem(
      const calculator::SizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes);
  void UpdateSizeStat(
      const calculator::SizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes,
      int64_t available_bytes);

  // Marks the size of |item| as calculated. When all storage items have been
  // calculated, then "System" size can be calculated.
  void UpdateSystemSize(
      const calculator::SizeCalculator::CalculationType& calculation_type,
      int64_t total_bytes);

  // Updates list of external storages.
  void UpdateExternalStorages();

  // Returns true if the volume from |source_path| can be used as Android
  // storage.
  bool IsEligibleForAndroidStorage(std::string source_path);

  // Instances calculating the size of each storage items.
  calculator::SizeStatCalculator size_stat_calculator_;
  calculator::MyFilesSizeCalculator my_files_size_calculator_;
  calculator::BrowsingDataSizeCalculator browsing_data_size_calculator_;
  calculator::AppsSizeCalculator apps_size_calculator_;
  calculator::CrostiniSizeCalculator crostini_size_calculator_;
  calculator::OtherUsersSizeCalculator other_users_size_calculator_;

  // Controls if the size of each storage item has been calculated.
  std::bitset<calculator::SizeCalculator::kCalculationTypeCount>
      calculation_state_;

  // Keeps track of the size of each storage item.
  int64_t storage_items_total_bytes_
      [calculator::SizeCalculator::kCalculationTypeCount] = {0};

  Profile* const profile_;
  const std::string source_name_;
  base::ScopedObservation<arc::ArcSessionManager,
                          arc::ArcSessionManagerObserver>
      arc_observation_{this};
  const re2::RE2 special_volume_path_pattern_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
