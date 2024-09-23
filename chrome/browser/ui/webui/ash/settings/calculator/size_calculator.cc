// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/calculator/size_calculator.h"

#include <cstdint>
#include <numeric>
#include <type_traits>

#include "ash/components/arc/disk_space/arc_disk_space_bridge.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/borealis/borealis_features.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_features.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/cryptohome/userdataauth_util.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/browsing_data/content/browsing_data_quota_helper.h"
#include "components/browsing_data/content/conditional_cache_counting_helper.h"
#include "components/browsing_data/content/cookie_helper.h"
#include "components/browsing_data/content/local_storage_helper.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/storage_partition.h"

namespace ash::settings {
namespace {

// Computes the size of MyFiles and Play files.
int64_t ComputeLocalFilesSize(const base::FilePath& my_files,
                              const base::FilePath& play_files) {
  // Compute size of MyFiles.
  int64_t size = base::ComputeDirectorySize(my_files);

  // Compute size of Play Files.
  if (int64_t play_size = base::ComputeDirectorySize(play_files);
      play_size > 0) {
    // Remove size of the Download folder, because it is already counted as part
    // of MyFiles.
    play_size -= base::ComputeDirectorySize(play_files.AppendASCII("Download"));
    if (play_size > 0) {
      size += play_size;
    }
  }

  return size;
}

}  // namespace

std::ostream& operator<<(std::ostream& out, SizeCalculator::CalculationType t) {
  switch (t) {
#define PRINT(s)                              \
  case SizeCalculator::CalculationType::k##s: \
    return out << #s;
    PRINT(Total)
    PRINT(Available)
    PRINT(MyFiles)
    PRINT(BrowsingData)
    PRINT(AppsExtensions)
    PRINT(DriveOfflineFiles)
    PRINT(Crostini)
    PRINT(OtherUsers)
    PRINT(System)
#undef PRINT
  }

  return out << "CalculationType("
             << static_cast<
                    std::underlying_type_t<SizeCalculator::CalculationType>>(t)
             << ")";
}

SizeCalculator::SizeCalculator(CalculationType calculation_type)
    : calculation_type_(calculation_type) {}

SizeCalculator::~SizeCalculator() {}

void SizeCalculator::StartCalculation() {
  if (calculating_) {
    return;
  }
  calculating_ = true;
  PerformCalculation();
}

void SizeCalculator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SizeCalculator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SizeCalculator::NotifySizeCalculated(const int64_t size) {
  calculating_ = false;

  LOG_IF(ERROR, size < 0) << "Got negative size " << size
                          << " while calculating " << calculation_type_;

  for (Observer& observer : observers_) {
    observer.OnSizeCalculated(calculation_type_, size);
  }
}

TotalDiskSpaceCalculator::TotalDiskSpaceCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kTotal), profile_(profile) {}

TotalDiskSpaceCalculator::~TotalDiskSpaceCalculator() = default;

void TotalDiskSpaceCalculator::PerformCalculation() {
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    GetTotalDiskSpace();
    return;
  }
  GetRootDeviceSize();
}

void TotalDiskSpaceCalculator::GetRootDeviceSize() {
  SpacedClient::Get()->GetRootDeviceSize(
      base::BindOnce(&TotalDiskSpaceCalculator::OnGetRootDeviceSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TotalDiskSpaceCalculator::OnGetRootDeviceSize(
    std::optional<int64_t> reply) {
  if (reply.has_value()) {
    NotifySizeCalculated(reply.value());
    return;
  }

  // FakeSpacedClient does not have a proper implementation of
  // GetRootDeviceSize. If SpacedClient::GetRootDeviceSize does not return a
  // value, use GetTotalDiskSpace as a fallback.
  VLOG(1) << "SpacedClient::OnGetRootDeviceSize gave an empty reply. "
             "Using GetTotalDiskSpace as fallback.";
  GetTotalDiskSpace();
}

void TotalDiskSpaceCalculator::GetTotalDiskSpace() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::SysInfo::AmountOfTotalDiskSpace,
                     file_manager::util::GetMyFilesFolderForProfile(profile_)),
      base::BindOnce(&TotalDiskSpaceCalculator::NotifySizeCalculated,
                     weak_ptr_factory_.GetWeakPtr()));
}

FreeDiskSpaceCalculator::FreeDiskSpaceCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kAvailable), profile_(profile) {}

FreeDiskSpaceCalculator::~FreeDiskSpaceCalculator() = default;

void FreeDiskSpaceCalculator::PerformCalculation() {
  if (user_manager::UserManager::Get()
          ->IsCurrentUserCryptohomeDataEphemeral()) {
    GetFreeDiskSpace();
    return;
  }
  GetUserFreeDiskSpace();
}

void FreeDiskSpaceCalculator::GetUserFreeDiskSpace() {
  SpacedClient::Get()->GetFreeDiskSpace(
      file_manager::util::GetMyFilesFolderForProfile(profile_).value(),
      base::BindOnce(&FreeDiskSpaceCalculator::OnGetUserFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FreeDiskSpaceCalculator::OnGetUserFreeDiskSpace(
    std::optional<int64_t> reply) {
  if (reply.has_value()) {
    NotifySizeCalculated(reply.value());
    return;
  }

  // FakeSpacedClient does not have a proper implementation of
  // GetFreeDiskSpace. If SpacedClient::GetFreeDiskSpace does not return a
  // value, use GetFreeDiskSpace as a fallback.
  VLOG(1) << "SpacedClient::GetFreeDiskSpace gave an empty reply. "
             "Using GetFreeDiskSpace as fallback.";
  GetFreeDiskSpace();
}

void FreeDiskSpaceCalculator::GetFreeDiskSpace() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     file_manager::util::GetMyFilesFolderForProfile(profile_)),
      base::BindOnce(&FreeDiskSpaceCalculator::NotifySizeCalculated,
                     weak_ptr_factory_.GetWeakPtr()));
}

DriveOfflineSizeCalculator::DriveOfflineSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kDriveOfflineFiles), profile_(profile) {}

DriveOfflineSizeCalculator::~DriveOfflineSizeCalculator() = default;

void DriveOfflineSizeCalculator::PerformCalculation() {
  drive::DriveIntegrationService* const service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!service) {
    NotifySizeCalculated(0);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&drive::util::ComputeDriveFsContentCacheSize,
                     service->GetDriveFsContentCachePath()),
      base::BindOnce(&DriveOfflineSizeCalculator::NotifySizeCalculated,
                     weak_ptr_factory_.GetWeakPtr()));
}

MyFilesSizeCalculator::MyFilesSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kMyFiles), profile_(profile) {}

MyFilesSizeCalculator::~MyFilesSizeCalculator() = default;

void MyFilesSizeCalculator::PerformCalculation() {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ComputeLocalFilesSize,
                     file_manager::util::GetMyFilesFolderForProfile(profile_),
                     file_manager::util::GetAndroidFilesPath()),
      base::BindOnce(&MyFilesSizeCalculator::NotifySizeCalculated,
                     weak_ptr_factory_.GetWeakPtr()));
}

BrowsingDataSizeCalculator::BrowsingDataSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kBrowsingData), profile_(profile) {}

BrowsingDataSizeCalculator::~BrowsingDataSizeCalculator() = default;

void BrowsingDataSizeCalculator::PerformCalculation() {
  has_browser_cache_size_ = false;
  has_browser_site_data_size_ = false;

  // Fetch the size of http cache in browsing data.
  browsing_data::ConditionalCacheCountingHelper::Count(
      profile_->GetDefaultStoragePartition(), base::Time(), base::Time::Max(),
      base::BindOnce(&BrowsingDataSizeCalculator::OnGetCacheSize,
                     weak_ptr_factory_.GetWeakPtr()));

  // Fetch the size of site data in browsing data.
  if (!site_data_size_collector_.get()) {
    content::StoragePartition* storage_partition =
        profile_->GetDefaultStoragePartition();
    site_data_size_collector_ = std::make_unique<SiteDataSizeCollector>(
        storage_partition->GetPath(),
        new browsing_data::CookieHelper(storage_partition,
                                        base::NullCallback()),
        new browsing_data::LocalStorageHelper(storage_partition),
        BrowsingDataQuotaHelper::Create(storage_partition));
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

    NotifySizeCalculated(browsing_data_size);
  }
}

AppsSizeCalculator::AppsSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kAppsExtensions), profile_(profile) {}

AppsSizeCalculator::~AppsSizeCalculator() {
  arc::ArcServiceManager::Get()
      ->arc_bridge_service()
      ->disk_space()
      ->RemoveObserver(this);
}

void AppsSizeCalculator::OnConnectionReady() {
  is_android_running_ = true;
  StartCalculation();
}

void AppsSizeCalculator::OnConnectionClosed() {
  is_android_running_ = false;
}

void AppsSizeCalculator::AddObserver(Observer* observer) {
  // Start observing arc mojo connection when the first observer is added, to
  // allow the calculation of android apps.
  if (observers_.empty()) {
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->disk_space()
        ->AddObserver(this);
  }
  SizeCalculator::AddObserver(observer);
}

void AppsSizeCalculator::RemoveObserver(Observer* observer) {
  SizeCalculator::RemoveObserver(observer);
  // Stop observing arc connection if all observers have been removed.
  if (observers_.empty()) {
    arc::ArcServiceManager::Get()
        ->arc_bridge_service()
        ->disk_space()
        ->RemoveObserver(this);
  }
}

void AppsSizeCalculator::PerformCalculation() {
  apps_extensions_size_ = 0;
  has_apps_extensions_size_ = false;
  android_apps_size_ = 0;
  has_android_apps_size_ = false;
  borealis_apps_size_ = 0;
  has_borealis_apps_size_ = false;

  UpdateAppsSize();
  UpdateAndroidAppsSize();
  UpdateBorealisAppsSize();
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
  auto* arc_disk_space_bridge =
      arc::ArcDiskSpaceBridge::GetForBrowserContext(profile_);
  if (arc_disk_space_bridge) {
    success = arc_disk_space_bridge->GetApplicationsSize(
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

void AppsSizeCalculator::UpdateBorealisAppsSize() {
  borealis::BorealisService* borealis_service =
      borealis::BorealisServiceFactory::GetForProfile(profile_);
  if (!borealis_service || !borealis_service->Features().IsEnabled()) {
    has_borealis_apps_size_ = true;
    return;
  }
  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  request.set_vm_name("borealis");
  ash::ConciergeClient::Get()->ListVmDisks(
      std::move(request),
      base::BindOnce(&AppsSizeCalculator::OnGetBorealisAppsSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppsSizeCalculator::OnGetBorealisAppsSize(
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get response from concierge";
    has_borealis_apps_size_ = true;
    UpdateAppsAndExtensionsSize();
    return;
  }
  if (!response->success()) {
    LOG(ERROR) << "concierge failed to list vm disks, returned error: " +
                      response->failure_reason();
    has_borealis_apps_size_ = true;
    UpdateAppsAndExtensionsSize();
    return;
  }
  auto image = base::ranges::find(response->images(), "borealis",
                                  &vm_tools::concierge::VmDiskInfo::name);
  if (image == response->images().end()) {
    LOG(ERROR) << "Couldn't find Borealis VM";
    has_borealis_apps_size_ = true;
    UpdateAppsAndExtensionsSize();
    return;
  }
  borealis_apps_size_ = image->size();
  has_borealis_apps_size_ = true;
  UpdateAppsAndExtensionsSize();
}

void AppsSizeCalculator::UpdateAppsAndExtensionsSize() {
  if (has_apps_extensions_size_ && has_android_apps_size_ &&
      has_borealis_apps_size_) {
    NotifySizeCalculated(apps_extensions_size_ + android_apps_size_ +
                         borealis_apps_size_);
  }
}

CrostiniSizeCalculator::CrostiniSizeCalculator(Profile* profile)
    : SizeCalculator(CalculationType::kCrostini), profile_(profile) {}

CrostiniSizeCalculator::~CrostiniSizeCalculator() = default;

void CrostiniSizeCalculator::PerformCalculation() {
  if (!crostini::CrostiniFeatures::Get()->IsEnabled(profile_)) {
    NotifySizeCalculated(
        profile_->GetPrefs()->GetInt64(crostini::prefs::kCrostiniLastDiskSize));
    return;
  }

  vm_tools::concierge::ListVmDisksRequest request;
  request.set_cryptohome_id(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_));
  request.set_storage_location(vm_tools::concierge::STORAGE_CRYPTOHOME_ROOT);
  ash::ConciergeClient::Get()->ListVmDisks(
      std::move(request),
      base::BindOnce(&CrostiniSizeCalculator::OnGetCrostiniSize,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CrostiniSizeCalculator::OnGetCrostiniSize(
    std::optional<vm_tools::concierge::ListVmDisksResponse> response) {
  if (!response) {
    LOG(ERROR) << "Failed to get list of VM disks. Empty response.";
    NotifySizeCalculated(
        profile_->GetPrefs()->GetInt64(crostini::prefs::kCrostiniLastDiskSize));
    return;
  }

  if (!response->success()) {
    LOG(ERROR) << "Failed to list VM disks: " << response->failure_reason();
    NotifySizeCalculated(
        profile_->GetPrefs()->GetInt64(crostini::prefs::kCrostiniLastDiskSize));
    return;
  }

  int64_t vm_disk_usage = response->total_size();

  // If Borealis is installed then we need to subtract its size from Crostini
  // in order for it to not be double counted.
  if (borealis::BorealisServiceFactory::GetForProfile(profile_)
          ->Features()
          .IsEnabled()) {
    auto image = base::ranges::find(response->images(), "borealis",
                                    &vm_tools::concierge::VmDiskInfo::name);
    if (image == response->images().end()) {
      LOG(ERROR) << "Couldn't find Borealis VM";
    } else {
      vm_disk_usage -= image->size();
    }
  }
  profile_->GetPrefs()->SetInt64(crostini::prefs::kCrostiniLastDiskSize,
                                 vm_disk_usage);
  NotifySizeCalculated(vm_disk_usage);
}

OtherUsersSizeCalculator::OtherUsersSizeCalculator()
    : SizeCalculator(CalculationType::kOtherUsers) {}

OtherUsersSizeCalculator::~OtherUsersSizeCalculator() = default;

void OtherUsersSizeCalculator::PerformCalculation() {
  other_users_.clear();
  user_sizes_.clear();
  const user_manager::UserList& users =
      user_manager::UserManager::Get()->GetUsers();
  for (user_manager::User* user : users) {
    if (user->is_active()) {
      continue;
    }
    other_users_.push_back(user);
    user_data_auth::GetAccountDiskUsageRequest request;
    *request.mutable_identifier() =
        cryptohome::CreateAccountIdentifierFromAccountId(user->GetAccountId());
    UserDataAuthClient::Get()->GetAccountDiskUsage(
        request, base::BindOnce(&OtherUsersSizeCalculator::OnGetOtherUserSize,
                                weak_ptr_factory_.GetWeakPtr()));
  }
  // We should show "0 B" if there is no other user.
  if (other_users_.empty()) {
    NotifySizeCalculated(0);
  }
}

void OtherUsersSizeCalculator::OnGetOtherUserSize(
    std::optional<user_data_auth::GetAccountDiskUsageReply> reply) {
  user_sizes_.push_back(
      user_data_auth::AccountDiskUsageReplyToUsageSize(reply));
  if (user_sizes_.size() != other_users_.size()) {
    return;
  }

  // If all the requests succeed, shows the total bytes in the UI.
  const int64_t other_users_total_bytes =
      base::Contains(user_sizes_, -1)
          ? -1
          : std::accumulate(user_sizes_.begin(), user_sizes_.end(), 0LL);
  NotifySizeCalculated(other_users_total_bytes);
}

}  // namespace ash::settings
