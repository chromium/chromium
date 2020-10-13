// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/calculator/size_calculator.h"

#include <numeric>

#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/browsing_data_flash_lso_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/browsing_data/content/appcache_helper.h"
#include "components/browsing_data/content/cache_storage_helper.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/database_helper.h"
#include "components/browsing_data/content/file_system_helper.h"
#include "components/browsing_data/content/indexed_db_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/browsing_data/content/service_worker_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"

namespace chromeos {
namespace settings {
namespace calculator {

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

}  // namespace

SizeCalculator::SizeCalculator(const CalculationType& calculation_type)
    : calculation_type_(calculation_type) {}

SizeCalculator::~SizeCalculator() {}

void SizeCalculator::StartCalculation() {
  if (calculating_)
    return;
  calculating_ = true;
  PerformCalculation();
}

void SizeCalculator::AddObserver(SizeCalculator::Observer* observer) {
  observers_.AddObserver(observer);
}

void SizeCalculator::RemoveObserver(SizeCalculator::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SizeCalculator::NotifySizeCalculated(
    int64_t total_bytes,
    const base::Optional<int64_t>& available_bytes) {
  calculating_ = false;
  for (SizeCalculator::Observer& observer : observers_) {
    observer.OnSizeCalculated(calculation_type_, total_bytes, available_bytes);
  }
}

SizeStatCalculator::SizeStatCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kInUse), profile_(profile) {}

SizeStatCalculator::~SizeStatCalculator() = default;
void SizeStatCalculator::PerformCalculation() {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  int64_t* total_size = new int64_t(0);
  int64_t* available_size = new int64_t(0);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetSizeStatBlocking, my_files_path, total_size,
                     available_size),
      base::BindOnce(&SizeStatCalculator::OnGetSizeStat,
                     weak_ptr_factory_.GetWeakPtr(), base::Owned(total_size),
                     base::Owned(available_size)));
}

void SizeStatCalculator::OnGetSizeStat(int64_t* total_bytes,
                                       int64_t* available_bytes) {
  NotifySizeCalculated(*total_bytes, *available_bytes);
}

MyFilesSizeCalculator::MyFilesSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kMyFiles), profile_(profile) {}

MyFilesSizeCalculator::~MyFilesSizeCalculator() = default;

void MyFilesSizeCalculator::PerformCalculation() {
  const base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);

  const base::FilePath android_files_path =
      base::FilePath(file_manager::util::GetAndroidFilesPath());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&MyFilesSizeCalculator::ComputeLocalFilesSize,
                     base::Unretained(this), my_files_path, android_files_path),
      base::BindOnce(&MyFilesSizeCalculator::OnGetMyFilesSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

int64_t MyFilesSizeCalculator::ComputeLocalFilesSize(
    const base::FilePath& my_files_path,
    const base::FilePath& android_files_path) {
  int64_t size = 0;

  // Compute directory size of My Files.
  size += base::ComputeDirectorySize(my_files_path);

  // Compute directory size of Play Files.
  size += base::ComputeDirectorySize(android_files_path);

  // Remove size of Download. If Android is enabled, the size of the Download
  // folder is counted in both My Files and Play files. If Android is disabled,
  // the Download folder doesn't exist and the returned size is 0.
  const base::FilePath download_files_path =
      android_files_path.AppendASCII("Download");
  size -= base::ComputeDirectorySize(download_files_path);

  return size;
}

void MyFilesSizeCalculator::OnGetMyFilesSize(int64_t total_bytes) {
  NotifySizeCalculated(total_bytes);
}

BrowsingDataSizeCalculator::BrowsingDataSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kBrowsingData), profile_(profile) {}

BrowsingDataSizeCalculator::~BrowsingDataSizeCalculator() = default;

void BrowsingDataSizeCalculator::PerformCalculation() {
  has_browser_cache_size_ = false;
  has_browser_site_data_size_ = false;

  // Fetch the size of http cache in browsing data.
  browsing_data::ConditionalCacheCountingHelper::Count(
      content::BrowserContext::GetDefaultStoragePartition(profile_),
      base::Time(), base::Time::Max(),
      base::BindOnce(&BrowsingDataSizeCalculator::OnGetCacheSize,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetch the size of site data in browsing data.
  if (!site_data_size_collector_.get()) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetDefaultStoragePartition(profile_);
    site_data_size_collector_ = std::make_unique<SiteDataSizeCollector>(
        storage_partition->GetPath(),
        new browsing_data::CookieHelper(storage_partition,
                                        base::NullCallback()),
        new browsing_data::DatabaseHelper(profile_),
        new browsing_data::LocalStorageHelper(profile_),
        new browsing_data::AppCacheHelper(
            storage_partition->GetAppCacheService()),
        new browsing_data::IndexedDBHelper(storage_partition),
        browsing_data::FileSystemHelper::Create(
            storage_partition->GetFileSystemContext(),
            browsing_data_file_system_util::GetAdditionalFileSystemTypes()),
        new browsing_data::ServiceWorkerHelper(
            storage_partition->GetServiceWorkerContext()),
        new browsing_data::CacheStorageHelper(
            storage_partition->GetCacheStorageContext()),
        BrowsingDataFlashLSOHelper::Create(profile_));
  }
  site_data_size_collector_->Fetch(
      base::BindOnce(&BrowsingDataSizeCalculator::OnGetBrowsingDataSize,
                     weak_ptr_factory_.GetWeakPtr(), /*is_site_data=*/true));
}

void BrowsingDataSizeCalculator::OnGetCacheSize(bool is_upper_limit,
                                                int64_t size) {
  DCHECK(!is_upper_limit);
  OnGetBrowsingDataSize(/*is_site_data=*/false, size);
}

void BrowsingDataSizeCalculator::OnGetBrowsingDataSize(bool is_site_data,
                                                       int64_t size) {
  if (is_site_data) {
    has_browser_site_data_size_ = true;
    browser_site_data_size_ = size;
  } else {
    has_browser_cache_size_ = true;
    browser_cache_size_ = size;
  }
  if (has_browser_cache_size_ && has_browser_site_data_size_) {
    int64_t browsing_data_size;
    if (browser_cache_size_ >= 0 && browser_site_data_size_ >= 0) {
      browsing_data_size = browser_site_data_size_ + browser_cache_size_;
    } else {
      browsing_data_size = -1;
    }
    calculating_ = false;
    NotifySizeCalculated(browsing_data_size);
  }
}

AppsSizeCalculator::AppsSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kAppsExtensions), profile_(profile) {}

AppsSizeCalculator::~AppsSizeCalculator() {
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->storage_manager()
      ->RemoveObserver(this);
}

void AppsSizeCalculator::OnConnectionReady() {
  is_android_running_ = true;
  StartCalculation();
}

void AppsSizeCalculator::OnConnectionClosed() {
  is_android_running_ = false;
}

void AppsSizeCalculator::AddObserver(SizeCalculator::Observer* observer) {
  // Start observing arc mojo connection when the first observer is added, to
  // allow the calculation of android apps.
  if (!observers_.might_have_observers()) {
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->storage_manager()
        ->AddObserver(this);
  }
  observers_.AddObserver(observer);
}

void AppsSizeCalculator::RemoveObserver(SizeCalculator::Observer* observer) {
  observers_.RemoveObserver(observer);
  // Stop observing arc connection if all observers have been removed.
  if (!observers_.might_have_observers()) {
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->storage_manager()
        ->RemoveObserver(this);
  }
}

void AppsSizeCalculator::PerformCalculation() {
  apps_extensions_size_ = 0;
  has_apps_extensions_size_ = false;
  android_apps_size_ = 0;
  has_android_apps_size_ = false;

  UpdateAppsSize();
  UpdateAndroidAppsSize();
}

void AppsSizeCalculator::UpdateAppsSize() {
  // Apps and extensions installed from the web store located in
  // [user-hash]/Extensions.
  const base::FilePath extensions_path =
      profile_->GetPath().AppendASCII("Extensions");

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&base::ComputeDirectorySize, extensions_path),
      base::BindOnce(&AppsSizeCalculator::OnGetAppsSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppsSizeCalculator::OnGetAppsSize(int64_t total_bytes) {
  apps_extensions_size_ = total_bytes;
  has_apps_extensions_size_ = true;
  UpdateAppsAndExtensionsSize();
}

void AppsSizeCalculator::UpdateAndroidAppsSize() {
  if (!is_android_running_) {
    has_android_apps_size_ = true;
    UpdateAppsAndExtensionsSize();
    return;
  }

  bool success = false;
  auto* arc_storage_manager =
      arc::ArcStorageManager::GetForBrowserContext(profile_);
  if (arc_storage_manager) {
    success = arc_storage_manager->GetApplicationsSize(
        base::BindOnce(&AppsSizeCalculator::OnGetAndroidAppsSize,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  if (!success) {
    has_android_apps_size_ = true;
    UpdateAppsAndExtensionsSize();
  }
}

void AppsSizeCalculator::OnGetAndroidAppsSize(
    bool succeeded,
    arc::mojom::ApplicationsSizePtr size) {
  has_android_apps_size_ = true;
  if (succeeded) {
    android_apps_size_ = size->total_code_bytes + size->total_data_bytes +
                         size->total_cache_bytes;
  }
  UpdateAppsAndExtensionsSize();
}

void AppsSizeCalculator::UpdateAppsAndExtensionsSize() {
  if (has_apps_extensions_size_ && has_android_apps_size_) {
    calculating_ = false;
    NotifySizeCalculated(apps_extensions_size_ + android_apps_size_);
  }
}

CrostiniSizeCalculator::CrostiniSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kCrostini), profile_(profile) {}

CrostiniSizeCalculator::~CrostiniSizeCalculator() = default;

void CrostiniSizeCalculator::PerformCalculation() {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
    NotifySizeCalculated(0);
    return;
  }

  crostini::CrostiniManager::GetForProfile(profile_)->ListVmDisks(
      base::BindOnce(&CrostiniSizeCalculator::OnGetCrostiniSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniSizeCalculator::OnGetCrostiniSize(crostini::CrostiniResult result,
                                               int64_t total_bytes) {
  calculating_ = false;
  NotifySizeCalculated(total_bytes);
}

OtherUsersSizeCalculator::OtherUsersSizeCalculator()
    : SizeCalculator(CalculationType::kOtherUsers) {}

OtherUsersSizeCalculator::~OtherUsersSizeCalculator() = default;

void OtherUsersSizeCalculator::PerformCalculation() {
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
        base::BindOnce(&OtherUsersSizeCalculator::OnGetOtherUserSize,
                       weak_ptr_factory_.GetWeakPtr()));
  }
  // We should show "0 B" if there is no other user.
  if (other_users_.empty()) {
    NotifySizeCalculated(0);
  }
}

void OtherUsersSizeCalculator::OnGetOtherUserSize(
    base::Optional<cryptohome::BaseReply> reply) {
  user_sizes_.push_back(cryptohome::AccountDiskUsageReplyToUsageSize(reply));
  if (user_sizes_.size() != other_users_.size())
    return;
  int64_t other_users_total_bytes;
  // If all the requests succeed, shows the total bytes in the UI.
  if (std::count(user_sizes_.begin(), user_sizes_.end(), -1) == 0) {
    other_users_total_bytes =
        std::accumulate(user_sizes_.begin(), user_sizes_.end(), 0LL);
  } else {
    other_users_total_bytes = -1;
  }
  NotifySizeCalculated(other_users_total_bytes);
}

}  // namespace calculator
}  // namespace settings
}  // namespace chromeos
