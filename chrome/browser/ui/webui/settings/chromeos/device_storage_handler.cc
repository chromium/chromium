// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_features_util.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/disks/disk.h"
#include "components/arc/arc_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

namespace chromeos {
namespace settings {

namespace {

constexpr char kAndroidEnabled[] = "androidEnabled";

const char* CalculationTypeToEventName(
    calculator::SizeCalculator::CalculationType x) {
  switch (x) {
    case calculator::SizeCalculator::CalculationType::kSystem:
      return "storage-system-size-changed";
    case calculator::SizeCalculator::CalculationType::kInUse:
      return "storage-size-stat-changed";
    case calculator::SizeCalculator::CalculationType::kMyFiles:
      return "storage-my-files-size-changed";
    case calculator::SizeCalculator::CalculationType::kBrowsingData:
      return "storage-browsing-data-size-changed";
    case calculator::SizeCalculator::CalculationType::kAppsExtensions:
      return "storage-apps-size-changed";
    case calculator::SizeCalculator::CalculationType::kCrostini:
      return "storage-crostini-size-changed";
    case calculator::SizeCalculator::CalculationType::kOtherUsers:
      return "storage-other-users-size-changed";
  }
  NOTREACHED();
  return "";
}

}  // namespace

StorageHandler::StorageHandler(Profile* profile,
                               content::WebUIDataSource* html_source)
    : size_stat_calculator_(profile),
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
                          features::ShouldShowExternalStorageSettings(profile));
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
  size_stat_calculator_.AddObserver(this);
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

int64_t StorageHandler::RoundByteSize(int64_t bytes) {
  if (bytes < 0) {
    NOTREACHED() << "Negative bytes value";
    return -1;
  }

  if (bytes == 0)
    return 0;

  // Subtract one to the original number of bytes.
  bytes--;
  // Set all the lower bits to 1.
  bytes |= bytes >> 1;
  bytes |= bytes >> 2;
  bytes |= bytes >> 4;
  bytes |= bytes >> 8;
  bytes |= bytes >> 16;
  bytes |= bytes >> 32;
  // Add one. The one bit beyond the highest set bit is set to 1. All the lower
  // bits are set to 0.
  bytes++;

  return bytes;
}

void StorageHandler::HandleUpdateAndroidEnabled(
    const base::ListValue* unused_args) {
  // OnJavascriptAllowed() calls ArcSessionManager::AddObserver() later.
  AllowJavascript();
}

void StorageHandler::HandleUpdateStorageInfo(const base::ListValue* args) {
  AllowJavascript();

  size_stat_calculator_.StartCalculation();
  my_files_size_calculator_.StartCalculation();
  browsing_data_size_calculator_.StartCalculation();
  apps_size_calculator_.StartCalculation();
  crostini_size_calculator_.StartCalculation();
  other_users_size_calculator_.StartCalculation();
}

void StorageHandler::HandleOpenMyFiles(const base::ListValue* unused_args) {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  platform_util::OpenItem(profile_, my_files_path, platform_util::OPEN_FOLDER,
                          platform_util::OpenOperationCallback());
}

void StorageHandler::HandleOpenArcStorage(
    const base::ListValue* unused_args) {
  auto* arc_storage_manager =
      arc::ArcStorageManager::GetForBrowserContext(profile_);
  if (arc_storage_manager)
    arc_storage_manager->OpenPrivateVolumeSettings();
}

void StorageHandler::HandleUpdateExternalStorages(
    const base::ListValue* unused_args) {
  UpdateExternalStorages();
}

void StorageHandler::UpdateExternalStorages() {
  base::Value devices(base::Value::Type::LIST);
  for (const auto& itr : DiskMountManager::GetInstance()->mount_points()) {
    const DiskMountManager::MountPointInfo& mount_info = itr.second;
    if (!IsEligibleForAndroidStorage(mount_info.source_path))
      continue;

    const chromeos::disks::Disk* disk =
        DiskMountManager::GetInstance()->FindDiskBySourcePath(
            mount_info.source_path);
    if (!disk)
      continue;

    std::string label = disk->device_label();
    if (label.empty()) {
      // To make volume labels consistent with Files app, we follow how Files
      // generates a volume label when the volume doesn't have specific label.
      // That is, we use the base name of mount path instead in such cases.
      // TODO(fukino): Share the implementation to compute the volume name with
      // Files app. crbug.com/1002535.
      label = base::FilePath(mount_info.mount_path).BaseName().AsUTF8Unsafe();
    }
    base::Value device(base::Value::Type::DICTIONARY);
    device.SetKey("uuid", base::Value(disk->fs_uuid()));
    device.SetKey("label", base::Value(label));
    devices.Append(std::move(device));
  }
  FireWebUIListener("onExternalStoragesUpdated", devices);
}

void StorageHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetKey(
      kAndroidEnabled,
      base::Value(features::ShouldShowExternalStorageSettings(profile_)));
  content::WebUIDataSource::Update(profile_, source_name_, std::move(update));
}

void StorageHandler::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE)
    return;

  if (!IsEligibleForAndroidStorage(mount_info.source_path))
    return;

  UpdateExternalStorages();
}

void StorageHandler::OnSizeCalculated(
    const calculator::SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes,
    const base::Optional<int64_t>& available_bytes) {
  if (available_bytes) {
    UpdateSizeStat(calculation_type, total_bytes, available_bytes.value());
  } else {
    UpdateStorageItem(calculation_type, total_bytes);
  }
}

void StorageHandler::StopObservingEvents() {
  // Stop observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->RemoveObserver(this);

  // Stop observing calculators.
  size_stat_calculator_.RemoveObserver(this);
  my_files_size_calculator_.RemoveObserver(this);
  browsing_data_size_calculator_.RemoveObserver(this);
  apps_size_calculator_.RemoveObserver(this);
  crostini_size_calculator_.RemoveObserver(this);
  other_users_size_calculator_.RemoveObserver(this);
}

void StorageHandler::UpdateStorageItem(
    const calculator::SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes) {
  // When the system size has been calculated, UpdateSystemSize calls this
  // method with the calculation type kSystem. This check prevents an infinite
  // loop.
  if (calculation_type != calculator::SizeCalculator::CalculationType::kSystem)
    UpdateSystemSize(calculation_type, total_bytes);

  std::u16string message;
  if (total_bytes < 0) {
    message = l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  } else {
    message = ui::FormatBytes(total_bytes);
  }

  if (calculation_type ==
      calculator::SizeCalculator::CalculationType::kOtherUsers) {
    bool no_other_users = (total_bytes == 0);
    FireWebUIListener(CalculationTypeToEventName(calculation_type),
                      base::Value(message), base::Value(no_other_users));
  } else {
    FireWebUIListener(CalculationTypeToEventName(calculation_type),
                      base::Value(message));
  }
}

void StorageHandler::UpdateSizeStat(
    const calculator::SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes,
    int64_t available_bytes) {
  int64_t rounded_total_bytes = RoundByteSize(total_bytes);
  int64_t in_use_total_bytes_ = rounded_total_bytes - available_bytes;

  UpdateSystemSize(calculation_type, in_use_total_bytes_);

  base::DictionaryValue size_stat;
  size_stat.SetString("availableSize", ui::FormatBytes(available_bytes));
  size_stat.SetString("usedSize", ui::FormatBytes(in_use_total_bytes_));
  size_stat.SetDouble("usedRatio", static_cast<double>(in_use_total_bytes_) /
                                       rounded_total_bytes);
  int storage_space_state =
      static_cast<int>(StorageSpaceState::kStorageSpaceNormal);
  if (available_bytes < kSpaceCriticallyLowBytes)
    storage_space_state =
        static_cast<int>(StorageSpaceState::kStorageSpaceCriticallyLow);
  else if (available_bytes < kSpaceLowBytes)
    storage_space_state = static_cast<int>(StorageSpaceState::kStorageSpaceLow);
  size_stat.SetInteger("spaceState", storage_space_state);

  FireWebUIListener(CalculationTypeToEventName(calculation_type), size_stat);
}

void StorageHandler::UpdateSystemSize(
    const calculator::SizeCalculator::CalculationType& calculation_type,
    int64_t total_bytes) {
  const int item_index = static_cast<int>(calculation_type);
  storage_items_total_bytes_[item_index] = total_bytes > 0 ? total_bytes : 0;
  calculation_state_.set(item_index);

  // Update system size. We only display the total system size when the size of
  // all categories has been updated. If some size calculations are pending,
  // return early and wait for all calculations to complete.
  if (!calculation_state_.all())
    return;

  int64_t system_bytes = 0;
  for (int i = 0; i < calculator::SizeCalculator::kCalculationTypeCount; ++i) {
    int64_t total_bytes_for_current_item = storage_items_total_bytes_[i];
    // If the storage is in use, add to the system's total storage.
    if (i ==
        static_cast<int>(calculator::SizeCalculator::CalculationType::kInUse)) {
      system_bytes += total_bytes_for_current_item;
      continue;
    }
    // Otherwise, this storage amount counts against the total storage
    // amount.
    system_bytes -= total_bytes_for_current_item;
  }

  OnSizeCalculated(calculator::SizeCalculator::CalculationType::kSystem,
                   system_bytes);
}

bool StorageHandler::IsEligibleForAndroidStorage(std::string source_path) {
  // Android's StorageManager volume concept relies on assumption that it is
  // local filesystem. Hence, special volumes like DriveFS should not be
  // listed on the Settings.
  return !RE2::FullMatch(source_path, special_volume_path_pattern_);
}

}  // namespace settings
}  // namespace chromeos
