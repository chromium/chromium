// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_H_

#include <array>
#include <bitset>
#include <memory>
#include <ostream>
#include <vector>

#include "ash/components/arc/disk_space/arc_disk_space_bridge.h"
#include "ash/components/arc/mojom/disk_space.mojom.h"
#include "ash/components/arc/session/connection_observer.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list_types.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "components/user_manager/user.h"

class Profile;

namespace vm_tools::concierge {
class ListVmDisksResponse;
}  // namespace vm_tools::concierge

namespace ash::settings {

// Base class for the calculation of a specific storage item. Instances of this
// class rely on their observers calling StartCalculation, and are designed to
// notify observers about the calculated sizes.
class SizeCalculator {
 public:
  // Enumeration listing the items displayed on the storage page.
  enum class CalculationType {
    kTotal = 0,
    kAvailable,
    kMyFiles,
    kBrowsingData,
    kAppsExtensions,
    kDriveOfflineFiles,
    kCrostini,
    kOtherUsers,
    kLastCalculationItem = kOtherUsers,
    kSystem,
  };

  // Implement this interface to be notified about item size callbacks.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSizeCalculated(const CalculationType& item_id,
                                  int64_t total_bytes) = 0;
  };

  // Total number of storage items.
  static constexpr int kCalculationTypeCount =
      static_cast<int>(CalculationType::kLastCalculationItem) + 1;

  explicit SizeCalculator(CalculationType calculation_type);
  virtual ~SizeCalculator();

  // Starts the size calculation of a given storage item.
  void StartCalculation();

  // Adds an observer.
  virtual void AddObserver(Observer* observer);

  // Removes an observer.
  virtual void RemoveObserver(Observer* observer);

 protected:
  // Performs the size calculation.
  virtual void PerformCalculation() = 0;

  // Notify the StorageHandler about the calculated storage item size
  void NotifySizeCalculated(int64_t size);

  // Item id.
  const CalculationType calculation_type_;

  // Flag indicating that fetch operations for storage size are ongoing.
  bool calculating_ = false;

  // Observers being notified about storage items size changes.
  base::ObserverList<Observer> observers_;
};

std::ostream& operator<<(std::ostream& out, SizeCalculator::CalculationType t);

// Class handling the calculation of the total disk space on the system.
class TotalDiskSpaceCalculator : public SizeCalculator {
 public:
  explicit TotalDiskSpaceCalculator(Profile* profile);

  TotalDiskSpaceCalculator(const TotalDiskSpaceCalculator&) = delete;
  TotalDiskSpaceCalculator& operator=(const TotalDiskSpaceCalculator&) = delete;

  ~TotalDiskSpaceCalculator() override;

 private:
  friend class TotalDiskSpaceTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  void GetRootDeviceSize();

  void OnGetRootDeviceSize(std::optional<int64_t> reply);

  void GetTotalDiskSpace();

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<TotalDiskSpaceCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the amount free usable disk space.
class FreeDiskSpaceCalculator : public SizeCalculator {
 public:
  explicit FreeDiskSpaceCalculator(Profile* profile);

  FreeDiskSpaceCalculator(const FreeDiskSpaceCalculator&) = delete;
  FreeDiskSpaceCalculator& operator=(const FreeDiskSpaceCalculator&) = delete;

  ~FreeDiskSpaceCalculator() override;

 private:
  friend class FreeDiskSpaceTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  void GetUserFreeDiskSpace();

  void OnGetUserFreeDiskSpace(std::optional<int64_t> reply);

  void GetFreeDiskSpace();

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<FreeDiskSpaceCalculator> weak_ptr_factory_{this};
};

class DriveOfflineSizeCalculator : public SizeCalculator {
 public:
  explicit DriveOfflineSizeCalculator(Profile* profile);

  DriveOfflineSizeCalculator(const DriveOfflineSizeCalculator&) = delete;
  DriveOfflineSizeCalculator& operator=(const DriveOfflineSizeCalculator&) =
      delete;

  ~DriveOfflineSizeCalculator() override;

 private:
  friend class DriveOfflineSizeTestAPI;

  void PerformCalculation() override;

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<DriveOfflineSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the size of the user's personal files: My
// files + Android Play files.
class MyFilesSizeCalculator : public SizeCalculator {
 public:
  explicit MyFilesSizeCalculator(Profile* profile);

  MyFilesSizeCalculator(const MyFilesSizeCalculator&) = delete;
  MyFilesSizeCalculator& operator=(const MyFilesSizeCalculator&) = delete;

  ~MyFilesSizeCalculator() override;

 private:
  friend class MyFilesSizeTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<MyFilesSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of browsing data and cache.
class BrowsingDataSizeCalculator : public SizeCalculator {
 public:
  explicit BrowsingDataSizeCalculator(Profile* profile);

  BrowsingDataSizeCalculator(const BrowsingDataSizeCalculator&) = delete;
  BrowsingDataSizeCalculator& operator=(const BrowsingDataSizeCalculator&) =
      delete;

  ~BrowsingDataSizeCalculator() override;

 private:
  friend class BrowsingDataSizeTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  // Callback to receive the cache size.
  void OnGetCacheSize(bool is_upper_limit, int64_t size);

  // Callback to update the size of browsing data.
  void OnGetBrowsingDataSize(bool is_site_data, int64_t size);

  // Total size of cache data in browsing data.
  int64_t browser_cache_size_ = -1;

  // True if we have already received the size of http cache.
  bool has_browser_cache_size_ = false;

  // Total size of site data in browsing data.
  int64_t browser_site_data_size_ = -1;

  // True if we have already received the size of http cache.
  bool has_browser_site_data_size_ = false;

  // Helper to compute the total size of all types of site date.
  std::unique_ptr<SiteDataSizeCollector> site_data_size_collector_;

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<BrowsingDataSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of the size of the user's apps and extensions.
class AppsSizeCalculator
    : public SizeCalculator,
      public arc::ConnectionObserver<arc::mojom::DiskSpaceInstance> {
 public:
  explicit AppsSizeCalculator(Profile* profile);

  AppsSizeCalculator(const AppsSizeCalculator&) = delete;
  AppsSizeCalculator& operator=(const AppsSizeCalculator&) = delete;

  ~AppsSizeCalculator() override;

  // arc::ConnectionObserver<arc::mojom::DiskSpaceInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // Adds an observer. When the first observer is added, start observing the arc
  // mojo connection UpdateAndroidAppsSize relies on.
  void AddObserver(Observer* observer) override;

  // Removes an observer. When the last observer is removed, stop observing the
  // arc mojo connection.
  void RemoveObserver(Observer* observer) override;

 private:
  friend class AppsSizeTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  // Requests updating the size of web store apps and extensions.
  void UpdateAppsSize();

  // Callback to update web store apps and extensions size.
  void OnGetAppsSize(int64_t total_bytes);

  // Requests updating the size of android apps.
  void UpdateAndroidAppsSize();

  // Callback to update Android apps and cache.
  void OnGetAndroidAppsSize(bool succeeded,
                            arc::mojom::ApplicationsSizePtr size);

  // Requests updating the size of Borealis apps.
  void UpdateBorealisAppsSize();

  // Callback to update Borealis apps and cache.
  void OnGetBorealisAppsSize(
      std::optional<vm_tools::concierge::ListVmDisksResponse> response);

  // Updates apps and extensions size.
  void UpdateAppsAndExtensionsSize();

  // Total size of apps and extensions
  int64_t apps_extensions_size_ = 0;

  // True if we have already received the size of apps and extensions.
  bool has_apps_extensions_size_ = false;

  // Total size of android apps
  int64_t android_apps_size_ = 0;

  // True if we have already received the size of Android apps.
  bool has_android_apps_size_ = false;

  // A flag for keeping track of the mojo connection status to the ARC
  // container.
  bool is_android_running_ = false;

  // Total size of Borealis apps (bytes).
  int64_t borealis_apps_size_ = 0;

  // True if we have already received the size of Borealis apps.
  bool has_borealis_apps_size_ = false;

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<AppsSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of crostini VM size.
class CrostiniSizeCalculator : public SizeCalculator {
 public:
  explicit CrostiniSizeCalculator(Profile* profile);

  CrostiniSizeCalculator(const CrostiniSizeCalculator&) = delete;
  CrostiniSizeCalculator& operator=(const CrostiniSizeCalculator&) = delete;

  ~CrostiniSizeCalculator() override;

 private:
  friend class CrostiniSizeTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  // Callback to update the size of Crostini VMs.
  void OnGetCrostiniSize(
      std::optional<vm_tools::concierge::ListVmDisksResponse>);

  raw_ptr<Profile> profile_;
  base::WeakPtrFactory<CrostiniSizeCalculator> weak_ptr_factory_{this};
};

// Class handling the calculation of other users' cryptohomes.
class OtherUsersSizeCalculator : public SizeCalculator {
 public:
  OtherUsersSizeCalculator();

  OtherUsersSizeCalculator(const OtherUsersSizeCalculator&) = delete;
  OtherUsersSizeCalculator& operator=(const OtherUsersSizeCalculator&) = delete;

  ~OtherUsersSizeCalculator() override;

 private:
  friend class OtherUsersSizeTestAPI;

  // SizeCalculator:
  void PerformCalculation() override;

  // Callback to update the sizes of the other users.
  void OnGetOtherUserSize(
      std::optional<::user_data_auth::GetAccountDiskUsageReply> reply);

  // The list of other users whose directory sizes will be accumulated as the
  // size of "Other users".
  user_manager::UserList other_users_;

  // Fetched sizes of user directories.
  std::vector<int64_t> user_sizes_;

  base::WeakPtrFactory<OtherUsersSizeCalculator> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_H_
