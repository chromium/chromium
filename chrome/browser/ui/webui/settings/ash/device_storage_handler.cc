// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/ash/device_storage_handler.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/arc/arc_features.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/settings/ash/device_storage_util.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_features_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/disks/disk.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

namespace ash::settings {

namespace {

using disks::Disk;
using disks::DiskMountManager;

constexpr char kAndroidEnabled[] = "androidEnabled";
// Dummy UUID for testing. The UUID is taken from
// ash/components/arc/volume_mounter/arc_volume_mounter_bridge.cc.
constexpr char kDummyUuid[] = "00000000000000000000000000000000DEADBEEF";

const char* CalculationTypeToEventName(SizeCalculator::CalculationType x) {
  switch (x) {
    case SizeCalculator::CalculationType::kTotal:
      return "storage-size-stat-changed";
    case SizeCalculator::CalculationType::kMyFiles:
      return "storage-my-files-size-changed";
    case SizeCalculator::CalculationType::kBrowsingData:
      return "storage-browsing-data-size-changed";
    case SizeCalculator::CalculationType::kAppsExtensions:
      return "storage-apps-size-changed";
    case SizeCalculator::CalculationType::kCrostini:
      return "storage-crostini-size-changed";
    case SizeCalculator::CalculationType::kOtherUsers:
      return "storage-other-users-size-changed";
    case SizeCalculator::CalculationType::kSystem:
      return "storage-system-size-changed";
    default:
      NOTREACHED();
      return "";
  }
}

}  // namespace

StorageHandler::StorageHandler(Profile* profile,
                               content::WebUIDataSource* html_source)
    : total_disk_space_calculator_(profile),
      free_disk_space_calculator_(profile),
      my_files_size_calculator_(profile),
      browsing_data_size_calculator_(profile),
      apps_size_calculator_(profile),
      crostini_size_calculator_(profile),
      other_users_size_calculator_(),
      profile_(profile),
      source_name_(html_source->GetSource()),
      special_volume_path_pattern_("[a-z]+://.*") {
  // TODO(khorimoto): Set kAndroidEnabled within DeviceSection, and
  // updates this value accordingly (see OnArcPlayStoreEnabledChanged()).
  html_source->AddBoolean(kAndroidEnabled,
                          ShouldShowExternalStorageSettings(profile));
}

StorageHandler::~StorageHandler() {
  StopObservingEvents();
}

void StorageHandler::RegisterMessages() {
  DCHECK(web_ui());

  web_ui()->RegisterMessageCallback(
      "updateAndroidEnabled",
      base::BindRepeating(&StorageHandler::HandleUpdateAndroidEnabled,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateStorageInfo",
      base::BindRepeating(&StorageHandler::HandleUpdateStorageInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openMyFiles", base::BindRepeating(&StorageHandler::HandleOpenMyFiles,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openArcStorage",
      base::BindRepeating(&StorageHandler::HandleOpenArcStorage,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "updateExternalStorages",
      base::BindRepeating(&StorageHandler::HandleUpdateExternalStorages,
                          base::Unretained(this)));
}

void StorageHandler::OnJavascriptAllowed() {
  if (base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature))
    arc_observation_.Observe(arc::ArcSessionManager::Get());

  // Start observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->AddObserver(this);

  // Start observing calculators.
  total_disk_space_calculator_.AddObserver(this);
  free_disk_space_calculator_.AddObserver(this);
  my_files_size_calculator_.AddObserver(this);
  browsing_data_size_calculator_.AddObserver(this);
  apps_size_calculator_.AddObserver(this);
  crostini_size_calculator_.AddObserver(this);
  other_users_size_calculator_.AddObserver(this);
}

void StorageHandler::OnJavascriptDisallowed() {
  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature)) {
    DCHECK(arc_observation_.IsObservingSource(arc::ArcSessionManager::Get()));
    arc_observation_.Reset();
  }

  StopObservingEvents();
}

void StorageHandler::HandleUpdateAndroidEnabled(
    const base::Value::List& unused_args) {
  // OnJavascriptAllowed() calls ArcSessionManager::AddObserver() later.
  AllowJavascript();
}

void StorageHandler::HandleUpdateStorageInfo(const base::Value::List& args) {
  AllowJavascript();
  total_disk_space_calculator_.StartCalculation();
  free_disk_space_calculator_.StartCalculation();
  my_files_size_calculator_.StartCalculation();
  browsing_data_size_calculator_.StartCalculation();
  apps_size_calculator_.StartCalculation();
  crostini_size_calculator_.StartCalculation();
  other_users_size_calculator_.StartCalculation();
}

void StorageHandler::HandleOpenMyFiles(const base::Value::List& unused_args) {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  platform_util::OpenItem(profile_, my_files_path, platform_util::OPEN_FOLDER,
                          platform_util::OpenOperationCallback());
}

void StorageHandler::HandleOpenArcStorage(
    const base::Value::List& unused_args) {
  auto* arc_storage_manager =
      arc::ArcStorageManager::GetForBrowserContext(profile_);
  if (arc_storage_manager)
    arc_storage_manager->OpenPrivateVolumeSettings();
}

void StorageHandler::HandleUpdateExternalStorages(
    const base::Value::List& unused_args) {
  UpdateExternalStorages();
}

void StorageHandler::UpdateExternalStorages() {
  base::Value::List devices;
  for (const auto& mount_point :
       DiskMountManager::GetInstance()->mount_points()) {
    if (!IsEligibleForAndroidStorage(mount_point.source_path))
      continue;

    const Disk* disk = DiskMountManager::GetInstance()->FindDiskBySourcePath(
        mount_point.source_path);

    // Assigning a dummy UUID for diskless volume for testing.
    const std::string uuid = disk ? disk->fs_uuid() : kDummyUuid;
    std::string label = disk ? disk->device_label() : std::string();
    if (label.empty()) {
      // To make volume labels consistent with Files app, we follow how Files
      // generates a volume label when the volume doesn't have specific label.
      // That is, we use the base name of mount path instead in such cases.
      // TODO(fukino): Share the implementation to compute the volume name with
      // Files app. crbug.com/1002535.
      label = base::FilePath(mount_point.mount_path).BaseName().AsUTF8Unsafe();
    }
    base::Value::Dict device;
    device.Set("uuid", uuid);
    device.Set("label", label);
    devices.Append(std::move(device));
  }
  FireWebUIListener("onExternalStoragesUpdated", devices);
}

void StorageHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  base::Value::Dict update;
  update.Set(kAndroidEnabled, ShouldShowExternalStorageSettings(profile_));
  content::WebUIDataSource::Update(profile_, source_name_, std::move(update));
}

void StorageHandler::OnMountEvent(
    DiskMountManager::MountEvent event,
    MountError error_code,
    const DiskMountManager::MountPoint& mount_info) {
  if (error_code != MountError::kSuccess)
    return;

  if (!IsEligibleForAndroidStorage(mount_info.source_path))
    return;

  UpdateExternalStorages();
}

void StorageHandler::OnSizeCalculated(
    const SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes) {
  // The total disk space is rounded to the next power of 2.
  if (calculation_type == SizeCalculator::CalculationType::kTotal) {
    total_bytes = RoundByteSize(total_bytes);
  }

  // Store calculated item's size.
  const int item_index = static_cast<int>(calculation_type);
  storage_items_total_bytes_[item_index] = total_bytes;

  // Mark item as calculated.
  calculation_state_.set(item_index);

  // Update proper UI item on the storage page.
  switch (calculation_type) {
    case SizeCalculator::CalculationType::kTotal:
    case SizeCalculator::CalculationType::kAvailable:
      UpdateOverallStatistics();
      break;
    case SizeCalculator::CalculationType::kMyFiles:
    case SizeCalculator::CalculationType::kBrowsingData:
    case SizeCalculator::CalculationType::kAppsExtensions:
    case SizeCalculator::CalculationType::kCrostini:
    case SizeCalculator::CalculationType::kOtherUsers:
      UpdateStorageItem(calculation_type);
      break;
    default:
      NOTREACHED() << "Unexpected calculation type: " << item_index;
  }
  UpdateSystemSizeItem();
}

void StorageHandler::StopObservingEvents() {
  // Stop observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->RemoveObserver(this);

  // Stop observing calculators.
  total_disk_space_calculator_.RemoveObserver(this);
  free_disk_space_calculator_.RemoveObserver(this);
  my_files_size_calculator_.RemoveObserver(this);
  browsing_data_size_calculator_.RemoveObserver(this);
  apps_size_calculator_.RemoveObserver(this);
  crostini_size_calculator_.RemoveObserver(this);
  other_users_size_calculator_.RemoveObserver(this);
}

void StorageHandler::UpdateStorageItem(
    const SizeCalculator::CalculationType& calculation_type) {
  const int item_index = static_cast<int>(calculation_type);
  const int64_t total_bytes = storage_items_total_bytes_[item_index];

  std::u16string message;
  if (total_bytes < 0) {
    message = l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  } else {
    message = ui::FormatBytes(total_bytes);
  }

  if (calculation_type == SizeCalculator::CalculationType::kOtherUsers) {
    bool no_other_users = (total_bytes == 0);
    FireWebUIListener(CalculationTypeToEventName(calculation_type),
                      base::Value(message), base::Value(no_other_users));
  } else {
    FireWebUIListener(CalculationTypeToEventName(calculation_type),
                      base::Value(message));
  }
}

void StorageHandler::UpdateOverallStatistics() {
  const int total_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kTotal);
  const int free_disk_space_index =
      static_cast<int>(SizeCalculator::CalculationType::kAvailable);

  if (!calculation_state_.test(total_space_index) ||
      !calculation_state_.test(free_disk_space_index)) {
    return;
  }

  // Update the total disk space by rounding it to the next power of 2.

  int64_t total_bytes = storage_items_total_bytes_[total_space_index];
  int64_t available_bytes = storage_items_total_bytes_[free_disk_space_index];
  int64_t in_use_bytes = total_bytes - available_bytes;

  if (total_bytes <= 0 || available_bytes < 0) {
    // We can't get useful information from the storage page if total_bytes <= 0
    // or available_bytes is less than 0. This is not expected to happen.
    NOTREACHED() << "Unable to retrieve total or available disk space";
    return;
  }

  base::Value::Dict size_stat;
  size_stat.Set("availableSize", ui::FormatBytes(available_bytes));
  size_stat.Set("usedSize", ui::FormatBytes(in_use_bytes));
  size_stat.Set("usedRatio", static_cast<double>(in_use_bytes) / total_bytes);
  int storage_space_state =
      static_cast<int>(StorageSpaceState::kStorageSpaceNormal);
  if (available_bytes < kSpaceCriticallyLowBytes)
    storage_space_state =
        static_cast<int>(StorageSpaceState::kStorageSpaceCriticallyLow);
  else if (available_bytes < kSpaceLowBytes)
    storage_space_state = static_cast<int>(StorageSpaceState::kStorageSpaceLow);
  size_stat.Set("spaceState", storage_space_state);

  FireWebUIListener(
      CalculationTypeToEventName(SizeCalculator::CalculationType::kTotal),
      size_stat);
}

void StorageHandler::UpdateSystemSizeItem() {
  // If some size calculations are pending, return early and wait for all
  // calculations to complete.
  if (!calculation_state_.all())
    return;

  int64_t system_bytes = 0;
  for (int i = 0; i < SizeCalculator::kCalculationTypeCount; ++i) {
    const int64_t total_bytes_for_current_item =
        std::max(storage_items_total_bytes_[i], static_cast<int64_t>(0));
    // The total amount of disk space counts positively towards system's size.
    if (i == static_cast<int>(SizeCalculator::CalculationType::kTotal)) {
      if (total_bytes_for_current_item <= 0)
        return;
      system_bytes += total_bytes_for_current_item;
      continue;
    }
    // All other items are subtracted from the total amount of disk space.
    if (i == static_cast<int>(SizeCalculator::CalculationType::kAvailable) &&
        total_bytes_for_current_item < 0)
      return;
    system_bytes -= total_bytes_for_current_item;
  }

  // Update UI.
  std::u16string message;
  if (system_bytes < 0) {
    message = l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  } else {
    message = ui::FormatBytes(system_bytes);
  }
  FireWebUIListener(
      CalculationTypeToEventName(SizeCalculator::CalculationType::kSystem),
      base::Value(message));
}

bool StorageHandler::IsEligibleForAndroidStorage(std::string source_path) {
  // Android's StorageManager volume concept relies on assumption that it is
  // local filesystem. Hence, special volumes like DriveFS should not be
  // listed on the Settings.
  return !RE2::FullMatch(source_path, special_volume_path_pattern_);
}

}  // namespace ash::settings
