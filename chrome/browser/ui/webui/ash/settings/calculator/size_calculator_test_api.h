// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_TEST_API_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_TEST_API_H_

#include <optional>
#include <utility>

#include "chrome/browser/ui/webui/ash/settings/calculator/size_calculator.h"
#include "chrome/browser/ui/webui/ash/settings/pages/storage/device_storage_handler.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"

namespace ash::settings {

class TotalDiskSpaceTestAPI {
 public:
  TotalDiskSpaceTestAPI(StorageHandler* handler, Profile* profile)
      : total_disk_space_calculator_(profile) {
    total_disk_space_calculator_.AddObserver(handler);
  }

  void StartCalculation() { total_disk_space_calculator_.StartCalculation(); }

  void SimulateOnGetRootDeviceSize(std::optional<uint64_t> reply) {
    total_disk_space_calculator_.OnGetRootDeviceSize(reply);
  }

  void SimulateOnGetTotalDiskSpace(int64_t total_bytes) {
    total_disk_space_calculator_.NotifySizeCalculated(total_bytes);
  }

 private:
  TotalDiskSpaceCalculator total_disk_space_calculator_;
};

class FreeDiskSpaceTestAPI {
 public:
  FreeDiskSpaceTestAPI(StorageHandler* handler, Profile* profile)
      : free_disk_space_calculator_(profile) {
    free_disk_space_calculator_.AddObserver(handler);
  }

  void StartCalculation() { free_disk_space_calculator_.StartCalculation(); }

  void SimulateOnGetUserFreeDiskSpace(std::optional<int64_t> reply) {
    free_disk_space_calculator_.OnGetUserFreeDiskSpace(reply);
  }

  void SimulateOnGetFreeDiskSpace(int64_t available_bytes) {
    free_disk_space_calculator_.NotifySizeCalculated(available_bytes);
  }

 private:
  FreeDiskSpaceCalculator free_disk_space_calculator_;
};

class DriveOfflineSizeTestAPI {
 public:
  DriveOfflineSizeTestAPI(StorageHandler* handler, Profile* profile)
      : drive_offline_size_calculator_(profile) {
    drive_offline_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { drive_offline_size_calculator_.StartCalculation(); }

  void SimulateOnGetOfflineItemsSize(int64_t offline_bytes) {
    drive_offline_size_calculator_.NotifySizeCalculated(offline_bytes);
  }

 private:
  DriveOfflineSizeCalculator drive_offline_size_calculator_;
};

class MyFilesSizeTestAPI {
 public:
  MyFilesSizeTestAPI(StorageHandler* handler, Profile* profile)
      : my_files_size_calculator_(profile) {
    my_files_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { my_files_size_calculator_.StartCalculation(); }

  void SimulateOnGetTotalBytes(int64_t total_bytes) {
    my_files_size_calculator_.NotifySizeCalculated(total_bytes);
  }

 private:
  MyFilesSizeCalculator my_files_size_calculator_;
};

class BrowsingDataSizeTestAPI {
 public:
  BrowsingDataSizeTestAPI(StorageHandler* handler, Profile* profile)
      : browsing_data_size_calculator_(profile) {
    browsing_data_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { browsing_data_size_calculator_.StartCalculation(); }

  void SimulateOnGetBrowsingDataSize(bool is_site_data, int64_t size) {
    browsing_data_size_calculator_.OnGetBrowsingDataSize(is_site_data, size);
  }

 private:
  BrowsingDataSizeCalculator browsing_data_size_calculator_;
};

class AppsSizeTestAPI {
 public:
  AppsSizeTestAPI(StorageHandler* handler, Profile* profile)
      : apps_size_calculator_(profile) {
    apps_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { apps_size_calculator_.StartCalculation(); }

  void SimulateOnGetAppsSize(int64_t total_bytes) {
    apps_size_calculator_.OnGetAppsSize(total_bytes);
  }

  void SimulateOnGetAndroidAppsSize(bool succeeded,
                                    uint64_t total_code_bytes,
                                    uint64_t total_data_bytes,
                                    uint64_t total_cache_bytes) {
    arc::mojom::ApplicationsSizePtr result(
        ::arc::mojom::ApplicationsSize::New());
    result->total_code_bytes = total_code_bytes;
    result->total_data_bytes = total_data_bytes;
    result->total_cache_bytes = total_cache_bytes;
    apps_size_calculator_.OnGetAndroidAppsSize(succeeded, std::move(result));
  }

  void SimulateOnGetBorealisAppsSize(
      bool succeeded,
      vm_tools::concierge::ListVmDisksResponse response) {
    response.set_success(succeeded);
    apps_size_calculator_.OnGetBorealisAppsSize(std::move(response));
  }

 private:
  AppsSizeCalculator apps_size_calculator_;
};

class CrostiniSizeTestAPI {
 public:
  CrostiniSizeTestAPI(StorageHandler* handler, Profile* profile)
      : crostini_size_calculator_(profile) {
    crostini_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { crostini_size_calculator_.StartCalculation(); }

  void SimulateOnGetCrostiniSize(
      bool succeeded,
      vm_tools::concierge::ListVmDisksResponse response) {
    response.set_success(succeeded);
    crostini_size_calculator_.OnGetCrostiniSize(std::move(response));
  }

 private:
  CrostiniSizeCalculator crostini_size_calculator_;
};

class OtherUsersSizeTestAPI {
 public:
  explicit OtherUsersSizeTestAPI(StorageHandler* handler) {
    other_users_size_calculator_.AddObserver(handler);
  }

  void StartCalculation() { other_users_size_calculator_.StartCalculation(); }

  void InitializeOtherUserSize(int user_count) {
    // When calling OnGetOtherUserSize, a callback is fired when
    // user_sizes_.size() == other_users_.size(). We want to control the size of
    // |other_users_|, rather than its content. This function initializes
    // other_users_ as a list of |user_count| nullptrs.
    other_users_size_calculator_.other_users_ =
        user_manager::UserList(user_count);
  }

  void SimulateOnGetOtherUserSize(
      std::optional<user_data_auth::GetAccountDiskUsageReply> reply) {
    other_users_size_calculator_.OnGetOtherUserSize(reply);
  }

 private:
  OtherUsersSizeCalculator other_users_size_calculator_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_CALCULATOR_SIZE_CALCULATOR_TEST_API_H_
