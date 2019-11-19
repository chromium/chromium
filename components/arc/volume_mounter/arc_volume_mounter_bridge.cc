// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/volume_mounter/arc_volume_mounter_bridge.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/disks/disk.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

using chromeos::disks::DiskMountManager;

namespace arc {

namespace {

// TODO(crbug.com/929031): Move MyFiles constants to a common place.
constexpr char kMyFilesPath[] = "/home/chronos/user/MyFiles";
// Dummy UUID for MyFiles volume.
constexpr char kMyFilesUuid[] = "0000000000000000000000000000CAFEF00D2019";
// Dummy UUID for testing.
constexpr char kDummyUuid[] = "00000000000000000000000000000000DEADBEEF";

// Singleton factory for ArcVolumeMounterBridge.
class ArcVolumeMounterBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcVolumeMounterBridge,
          ArcVolumeMounterBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcVolumeMounterBridgeFactory";

  static ArcVolumeMounterBridgeFactory* GetInstance() {
    return base::Singleton<ArcVolumeMounterBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcVolumeMounterBridgeFactory>;
  ArcVolumeMounterBridgeFactory() = default;
  ~ArcVolumeMounterBridgeFactory() override = default;
};

}  // namespace

// static
ArcVolumeMounterBridge* ArcVolumeMounterBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcVolumeMounterBridgeFactory::GetForBrowserContext(context);
}

// static
ArcVolumeMounterBridge* ArcVolumeMounterBridge::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcVolumeMounterBridgeFactory::GetForBrowserContextForTesting(context);
}

ArcVolumeMounterBridge::ArcVolumeMounterBridge(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service),
      pref_service_(user_prefs::UserPrefs::Get(context)) {
  DCHECK(pref_service_);
  arc_bridge_service_->volume_mounter()->AddObserver(this);
  arc_bridge_service_->volume_mounter()->SetHost(this);
  DCHECK(DiskMountManager::GetInstance());
  DiskMountManager::GetInstance()->AddObserver(this);

  change_registerar_.Init(pref_service_);
  // Start monitoring |kArcVisibleExternalStorages| changes. Note that the
  // registerar automatically stops monitoring the pref in its dtor.
  change_registerar_.Add(
      prefs::kArcVisibleExternalStorages,
      base::BindRepeating(&ArcVolumeMounterBridge::OnVisibleStoragesChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

ArcVolumeMounterBridge::~ArcVolumeMounterBridge() {
  DiskMountManager::GetInstance()->RemoveObserver(this);
  arc_bridge_service_->volume_mounter()->SetHost(nullptr);
  arc_bridge_service_->volume_mounter()->RemoveObserver(this);
}

// Sends MountEvents of all existing MountPoints in cros-disks.
void ArcVolumeMounterBridge::SendAllMountEvents() {
  SendMountEventForMyFiles();

  for (const auto& keyValue : DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                 chromeos::MountError::MOUNT_ERROR_NONE, keyValue.second);
  }
}

// Notifies ARC of MyFiles volume by sending a mount event.
void ArcVolumeMounterBridge::SendMountEventForMyFiles() {
  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->volume_mounter(),
                                  OnMountEvent);

  if (!volume_mounter_instance)
    return;

  std::string device_label =
      l10n_util::GetStringUTF8(IDS_FILE_BROWSER_MY_FILES_ROOT_LABEL);

  // TODO(niwa): Add a new DeviceType enum value for MyFiles.
  chromeos::DeviceType device_type = chromeos::DeviceType::DEVICE_TYPE_SD;

  volume_mounter_instance->OnMountEvent(mojom::MountPointInfo::New(
      DiskMountManager::MOUNTING, kMyFilesPath, kMyFilesPath, kMyFilesUuid,
      device_label, device_type, false));
}

bool ArcVolumeMounterBridge::IsVisibleToAndroidApps(
    const std::string& uuid) const {
  const base::ListValue* uuid_list =
      pref_service_->GetList(prefs::kArcVisibleExternalStorages);
  for (auto& value : uuid_list->GetList()) {
    if (value.is_string() && value.GetString() == uuid)
      return true;
  }
  return false;
}

void ArcVolumeMounterBridge::OnVisibleStoragesChanged() {
  // Remount all external mount points when the list of visible storage changes.
  for (const auto& key_value :
       DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::UNMOUNTING,
                 chromeos::MountError::MOUNT_ERROR_NONE, key_value.second);
  }
  for (const auto& key_value :
       DiskMountManager::GetInstance()->mount_points()) {
    OnMountEvent(DiskMountManager::MountEvent::MOUNTING,
                 chromeos::MountError::MOUNT_ERROR_NONE, key_value.second);
  }
}

void ArcVolumeMounterBridge::OnConnectionReady() {
  // Deferring the SendAllMountEvents as a task to current thread to not
  // block the mojo request since SendAllMountEvents might take non trivial
  // amount of time.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ArcVolumeMounterBridge::SendAllMountEvents,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ArcVolumeMounterBridge::OnMountEvent(
    DiskMountManager::MountEvent event,
    chromeos::MountError error_code,
    const DiskMountManager::MountPointInfo& mount_info) {
  // ArcVolumeMounter is limited for local storage, as Android's StorageManager
  // volume concept relies on assumption that it is local filesystem. Hence,
  // special volumes like DriveFS should not come through this path.
  if (RE2::FullMatch(mount_info.source_path, "[a-z]+://.*")) {
    DVLOG(1) << "Ignoring mount event for source_path: "
             << mount_info.source_path;
    return;
  }
  if (error_code != chromeos::MountError::MOUNT_ERROR_NONE) {
    DVLOG(1) << "Error " << error_code << "occurs during MountEvent " << event;
    return;
  }

  // Get disks informations that are needed by Android MountService.
  const chromeos::disks::Disk* disk =
      DiskMountManager::GetInstance()->FindDiskBySourcePath(
          mount_info.source_path);
  std::string fs_uuid, device_label;
  chromeos::DeviceType device_type = chromeos::DeviceType::DEVICE_TYPE_UNKNOWN;
  // There are several cases where disk can be null:
  // 1. The disk is removed physically before being ejected/unmounted.
  // 2. The disk is inserted, but then immediately removed physically. The
  //    disk removal will race with mount event in this case.
  if (disk) {
    fs_uuid = disk->fs_uuid();
    device_label = disk->device_label();
    device_type = disk->device_type();
  } else {
    // This is needed by ChromeOS autotest (cheets_RemovableMedia) because it
    // creates a diskless volume (hence, no uuid) and Android expects the volume
    // to have a uuid.
    fs_uuid = kDummyUuid;
    DVLOG(1) << "Disk at " << mount_info.source_path
             << " is null during MountEvent " << event;
  }

  mojom::VolumeMounterInstance* volume_mounter_instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->volume_mounter(),
                                  OnMountEvent);

  if (!volume_mounter_instance)
    return;

  const bool visible = IsVisibleToAndroidApps(fs_uuid);
  volume_mounter_instance->OnMountEvent(mojom::MountPointInfo::New(
      event, mount_info.source_path, mount_info.mount_path, fs_uuid,
      device_label, device_type, visible));
  if (event == DiskMountManager::MountEvent::MOUNTING &&
      (device_type == chromeos::DeviceType::DEVICE_TYPE_USB ||
       device_type == chromeos::DeviceType::DEVICE_TYPE_SD)) {
    // Record visibilities of the mounted devices only when they are removable
    // storages (e.g. USB sticks or SD cards).
    base::UmaHistogramBoolean("Arc.ExternalStorage.MountedMediaVisibility",
                              visible);
  }
}

void ArcVolumeMounterBridge::RequestAllMountPoints() {
  // Deferring the SendAllMountEvents as a task to current thread to not
  // block the mojo request since SendAllMountEvents might take non trivial
  // amount of time.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ArcVolumeMounterBridge::SendAllMountEvents,
                                weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace arc
