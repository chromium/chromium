// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/device_storage_handler.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_appcache_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cache_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data/browsing_data_database_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_helper.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/browsing_data/browsing_data_indexed_db_helper.h"
#include "chrome/browser/browsing_data/browsing_data_local_storage_helper.h"
#include "chrome/browser/browsing_data/browsing_data_service_worker_helper.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/disks/disk.h"
#include "components/arc/arc_features.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

using chromeos::disks::Disk;
using chromeos::disks::DiskMountManager;

namespace chromeos {
namespace settings {
namespace {

void GetSizeStatBlocking(const base::FilePath& mount_path,
                         int64_t* total_size,
                         int64_t* available_size) {
  int64_t size = base::SysInfo::AmountOfTotalDiskSpace(mount_path);
  if (size >= 0)
    *total_size = size;
  size = base::SysInfo::AmountOfFreeDiskSpace(mount_path);
  if (size >= 0)
    *available_size = size;
}

// Threshold to show a message indicating space is critically low (512 MB).
const int64_t kSpaceCriticallyLowBytes = 512 * 1024 * 1024;

// Threshold to show a message indicating space is low (1 GB).
const int64_t kSpaceLowBytes = 1 * 1024 * 1024 * 1024;

constexpr char kAndroidEnabled[] = "androidEnabled";

}  // namespace

StorageHandler::StorageHandler(Profile* profile,
                               content::WebUIDataSource* html_source)
    : browser_cache_size_(-1),
      has_browser_cache_size_(false),
      browser_site_data_size_(-1),
      has_browser_site_data_size_(false),
      updating_downloads_size_(false),
      updating_browsing_data_size_(false),
      updating_android_size_(false),
      updating_crostini_size_(false),
      updating_other_users_size_(false),
      is_android_running_(false),
      profile_(profile),
      source_name_(html_source->GetSource()),
      arc_observer_(this),
      special_volume_path_pattern_("[a-z]+://.*") {
  html_source->AddBoolean(
      kAndroidEnabled,
      base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature) &&
          arc::IsArcPlayStoreEnabledForProfile(profile));
}

StorageHandler::~StorageHandler() {
  DiskMountManager::GetInstance()->RemoveObserver(this);
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->storage_manager()
      ->RemoveObserver(this);
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
      "openDownloads", base::BindRepeating(&StorageHandler::HandleOpenDownloads,
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
    arc_observer_.Add(arc::ArcSessionManager::Get());

  // Start observing the mojo connection UpdateAndroidSize() relies on. Note
  // that OnConnectionReady() will be called immediately if the connection has
  // already been established.
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->storage_manager()
      ->AddObserver(this);

  // Start observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->AddObserver(this);
}

void StorageHandler::OnJavascriptDisallowed() {
  // Ensure that pending callbacks do not complete and cause JS to be evaluated.
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Stop observing mount/unmount events to update the connected device list.
  DiskMountManager::GetInstance()->RemoveObserver(this);

  // Stop observing the mojo connection so that OnConnectionReady() and
  // OnConnectionClosed() that use FireWebUIListener() won't be called while JS
  // is disabled.
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->storage_manager()
      ->RemoveObserver(this);

  if (base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature))
    arc_observer_.Remove(arc::ArcSessionManager::Get());
}

void StorageHandler::HandleUpdateAndroidEnabled(
    const base::ListValue* unused_args) {
  // OnJavascriptAllowed() calls ArcSessionManager::AddObserver() later.
  AllowJavascript();
}

void StorageHandler::HandleUpdateStorageInfo(const base::ListValue* args) {
  AllowJavascript();

  UpdateSizeStat();
  UpdateDownloadsSize();
  UpdateBrowsingDataSize();
  UpdateAndroidRunning();
  UpdateAndroidSize();
  UpdateCrostiniSize();
  UpdateOtherUsersSize();
}

void StorageHandler::HandleOpenDownloads(
    const base::ListValue* unused_args) {
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_);
  platform_util::OpenItem(profile_, downloads_path, platform_util::OPEN_FOLDER,
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

void StorageHandler::UpdateSizeStat() {
  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_);

  int64_t* total_size = new int64_t(0);
  int64_t* available_size = new int64_t(0);
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::Bind(&GetSizeStatBlocking, downloads_path, total_size,
                 available_size),
      base::Bind(&StorageHandler::OnGetSizeStat, weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(total_size), base::Owned(available_size)));
}

void StorageHandler::OnGetSizeStat(int64_t* total_size,
                                   int64_t* available_size) {
  int64_t used_size = *total_size - *available_size;
  base::DictionaryValue size_stat;
  size_stat.SetString("totalSize", ui::FormatBytes(*total_size));
  size_stat.SetString("availableSize", ui::FormatBytes(*available_size));
  size_stat.SetString("usedSize", ui::FormatBytes(used_size));
  size_stat.SetDouble("usedRatio",
                      static_cast<double>(used_size) / *total_size);
  int storage_space_state = STORAGE_SPACE_NORMAL;
  if (*available_size < kSpaceCriticallyLowBytes)
    storage_space_state = STORAGE_SPACE_CRITICALLY_LOW;
  else if (*available_size < kSpaceLowBytes)
    storage_space_state = STORAGE_SPACE_LOW;
  size_stat.SetInteger("spaceState", storage_space_state);

  FireWebUIListener("storage-size-stat-changed", size_stat);
}

void StorageHandler::UpdateDownloadsSize() {
  if (updating_downloads_size_)
    return;
  updating_downloads_size_ = true;

  const base::FilePath downloads_path =
      file_manager::util::GetDownloadsFolderForProfile(profile_);

  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::Bind(&base::ComputeDirectorySize, downloads_path),
      base::Bind(&StorageHandler::OnGetDownloadsSize,
                 weak_ptr_factory_.GetWeakPtr()));
}

void StorageHandler::OnGetDownloadsSize(int64_t size) {
  updating_downloads_size_ = false;
  FireWebUIListener("storage-downloads-size-changed",
                    base::Value(ui::FormatBytes(size)));
}

void StorageHandler::UpdateBrowsingDataSize() {
  if (updating_browsing_data_size_)
    return;
  updating_browsing_data_size_ = true;

  has_browser_cache_size_ = false;
  has_browser_site_data_size_ = false;
  // Fetch the size of http cache in browsing data.
  browsing_data::ConditionalCacheCountingHelper::Count(
      content::BrowserContext::GetDefaultStoragePartition(profile_),
      base::Time(), base::Time::Max(),
      base::BindOnce(&StorageHandler::OnGetCacheSize,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetch the size of site data in browsing data.
  if (!site_data_size_collector_.get()) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile_);
    site_data_size_collector_ = std::make_unique<SiteDataSizeCollector>(
        storage_partition->GetPath(),
        new BrowsingDataCookieHelper(storage_partition),
        new BrowsingDataDatabaseHelper(profile_),
        new BrowsingDataLocalStorageHelper(profile_),
        new BrowsingDataAppCacheHelper(storage_partition->GetAppCacheService()),
        new BrowsingDataIndexedDBHelper(
            storage_partition->GetIndexedDBContext()),
        BrowsingDataFileSystemHelper::Create(
            storage_partition->GetFileSystemContext()),
        new BrowsingDataServiceWorkerHelper(
            storage_partition->GetServiceWorkerContext()),
        new BrowsingDataCacheStorageHelper(
            storage_partition->GetCacheStorageContext()),
        BrowsingDataFlashLSOHelper::Create(profile_));
  }
  site_data_size_collector_->Fetch(
      base::Bind(&StorageHandler::OnGetBrowsingDataSize,
                 weak_ptr_factory_.GetWeakPtr(), true));
}

void StorageHandler::OnGetCacheSize(bool is_upper_limit, int64_t size) {
  DCHECK(!is_upper_limit);
  OnGetBrowsingDataSize(false, size);
}

void StorageHandler::OnGetBrowsingDataSize(bool is_site_data, int64_t size) {
  if (is_site_data) {
    has_browser_site_data_size_ = true;
    browser_site_data_size_ = size;
  } else {
    has_browser_cache_size_ = true;
    browser_cache_size_ = size;
  }
  if (has_browser_cache_size_ && has_browser_site_data_size_) {
    base::string16 size_string;
    if (browser_cache_size_ >= 0 && browser_site_data_size_ >= 0) {
      size_string = ui::FormatBytes(
          browser_site_data_size_ + browser_cache_size_);
    } else {
      size_string =
          l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
    }
    updating_browsing_data_size_ = false;
    FireWebUIListener("storage-browsing-data-size-changed",
                      base::Value(size_string));
  }
}

void StorageHandler::UpdateAndroidRunning() {
  FireWebUIListener("storage-android-running-changed",
                    base::Value(is_android_running_));
}

void StorageHandler::UpdateAndroidSize() {
  if (!is_android_running_)
    return;

  if (updating_android_size_)
    return;
  updating_android_size_ = true;

  bool success = false;
  auto* arc_storage_manager =
      arc::ArcStorageManager::GetForBrowserContext(profile_);
  if (arc_storage_manager) {
    success = arc_storage_manager->GetApplicationsSize(base::BindOnce(
        &StorageHandler::OnGetAndroidSize, weak_ptr_factory_.GetWeakPtr()));
  }
  if (!success)
    updating_android_size_ = false;
}

void StorageHandler::OnGetAndroidSize(bool succeeded,
                                      arc::mojom::ApplicationsSizePtr size) {
  base::string16 size_string;
  if (succeeded) {
    uint64_t total_bytes = size->total_code_bytes + size->total_data_bytes +
                           size->total_cache_bytes;
    size_string = ui::FormatBytes(total_bytes);
  } else {
    size_string = l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
  }
  updating_android_size_ = false;
  FireWebUIListener("storage-android-size-changed", base::Value(size_string));
}

void StorageHandler::UpdateCrostiniSize() {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
    return;
  }

  if (updating_crostini_size_)
    return;
  updating_crostini_size_ = true;

  crostini::CrostiniManager::GetForProfile(profile_)->ListVmDisks(
      base::BindOnce(&StorageHandler::OnGetCrostiniSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StorageHandler::OnGetCrostiniSize(crostini::CrostiniResult result,
                                       int64_t size) {
  updating_crostini_size_ = false;
  FireWebUIListener("storage-crostini-size-changed",
                    base::Value(ui::FormatBytes(size)));
}

void StorageHandler::UpdateOtherUsersSize() {
  if (updating_other_users_size_)
    return;
  updating_other_users_size_ = true;

  other_users_.clear();
  user_sizes_.clear();
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  for (auto* user : users) {
    if (user->is_active())
      continue;
    other_users_.push_back(user);
    CryptohomeClient::Get()->GetAccountDiskUsage(
        cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId()),
        base::BindOnce(&StorageHandler::OnGetOtherUserSize,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  // We should show "0 B" if there is no other user.
  if (other_users_.empty()) {
    updating_other_users_size_ = false;
    FireWebUIListener("storage-other-users-size-changed",
                      base::Value(ui::FormatBytes(0)));
  }
}

void StorageHandler::OnGetOtherUserSize(
    base::Optional<cryptohome::BaseReply> reply) {
  user_sizes_.push_back(cryptohome::AccountDiskUsageReplyToUsageSize(reply));
  if (user_sizes_.size() == other_users_.size()) {
    base::string16 size_string;
    // If all the requests succeed, shows the total bytes in the UI.
    if (std::count(user_sizes_.begin(), user_sizes_.end(), -1) == 0) {
      size_string = ui::FormatBytes(
          std::accumulate(user_sizes_.begin(), user_sizes_.end(), 0LL));
    } else {
      size_string =
          l10n_util::GetStringUTF16(IDS_SETTINGS_STORAGE_SIZE_UNKNOWN);
    }
    updating_other_users_size_ = false;
    FireWebUIListener("storage-other-users-size-changed",
                      base::Value(size_string));
  }
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

void StorageHandler::OnConnectionReady() {
  is_android_running_ = true;
  UpdateAndroidRunning();
  UpdateAndroidSize();
}

void StorageHandler::OnConnectionClosed() {
  is_android_running_ = false;
  UpdateAndroidRunning();
}

void StorageHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  FireWebUIListener("storage-android-enabled-changed", base::Value(enabled));
  auto update = std::make_unique<base::DictionaryValue>();
  update->SetKey(kAndroidEnabled, base::Value(enabled));
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

bool StorageHandler::IsEligibleForAndroidStorage(std::string source_path) {
  // Android's StorageManager volume concept relies on assumption that it is
  // local filesystem. Hence, special volumes like DriveFS should not be
  // listed on the Settings.
  return !RE2::FullMatch(source_path, special_volume_path_pattern_);
}

}  // namespace settings
}  // namespace chromeos
