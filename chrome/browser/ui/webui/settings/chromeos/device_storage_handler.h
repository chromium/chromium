// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/disks/disk_mount_manager.h"
#include "components/arc/mojom/storage_manager.mojom.h"
#include "components/arc/session/connection_observer.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/user_manager/user.h"
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

class StorageHandler
    : public ::settings::SettingsPageUIHandler,
      public arc::ConnectionObserver<arc::mojom::StorageManagerInstance>,
      public arc::ArcSessionManager::Observer,
      public chromeos::disks::DiskMountManager::Observer {
 public:
  // Enumeration for device state about remaining space. These values must be
  // kept in sync with settings.StorageSpaceState in JS code.
  enum StorageSpaceState {
    STORAGE_SPACE_NORMAL = 0,
    STORAGE_SPACE_LOW = 1,
    STORAGE_SPACE_CRITICALLY_LOW = 2,
  };

  StorageHandler(Profile* profile, content::WebUIDataSource* html_source);
  ~StorageHandler() override;

  // ::settings::SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // arc::ConnectionObserver<arc::mojom::StorageManagerInstance>:
  void OnConnectionReady() override;
  void OnConnectionClosed() override;

  // arc::ArcSessionManager::Observer:
  void OnArcPlayStoreEnabledChanged(bool enabled) override;

  // chromeos::disks::DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override;

 private:
  // Handlers of JS messages.
  void HandleUpdateAndroidEnabled(const base::ListValue* unused_args);
  void HandleUpdateStorageInfo(const base::ListValue* unused_args);
  void HandleOpenDownloads(const base::ListValue* unused_args);
  void HandleOpenArcStorage(const base::ListValue* unused_args);
  void HandleUpdateExternalStorages(const base::ListValue* unused_args);

  // Requests updating disk space information.
  void UpdateSizeStat();

  // Callback to update the UI about disk space information.
  void OnGetSizeStat(int64_t* total_size, int64_t* available_size);

  // Requests updating the size of Downloads directory.
  void UpdateDownloadsSize();

  // Callback to update the UI about the size of Downloads directory.
  void OnGetDownloadsSize(int64_t size);

  // Requests updating the size of browsing data.
  void UpdateBrowsingDataSize();

  // Callback to receive the cache size.
  void OnGetCacheSize(bool is_upper_limit, int64_t size);

  // Callback to update the UI about the size of browsing data.
  void OnGetBrowsingDataSize(bool is_site_data, int64_t size);

  // Requests updating the flag that hides the Android size UI.
  void UpdateAndroidRunning();

  // Requests updating the space size used by Android apps and cache.
  void UpdateAndroidSize();

  // Callback to update the UI about Android apps and cache.
  void OnGetAndroidSize(bool succeeded, arc::mojom::ApplicationsSizePtr size);

  // Requests updating the space size used by Crostini VMs and their apps and
  // cache.
  void UpdateCrostiniSize();

  // Callback to update the UI about Crostini VMs and their apps and cache.
  void OnGetCrostiniSize(crostini::CrostiniResult result, int64_t size);

  // Requests updating the total size of other users' data.
  void UpdateOtherUsersSize();

  // Callback to save the fetched user sizes and update the UI.
  void OnGetOtherUserSize(base::Optional<cryptohome::BaseReply> reply);

  // Updates list of external storages.
  void UpdateExternalStorages();

  // Returns true if the volume from |source_path| can be used as Android
  // storage.
  bool IsEligibleForAndroidStorage(std::string source_path);

  // Total size of cache data in browsing data.
  int64_t browser_cache_size_;

  // True if we have already received the size of http cache.
  bool has_browser_cache_size_;

  // Total size of site data in browsing data.
  int64_t browser_site_data_size_;

  // True if we have already received the size of site data.
  bool has_browser_site_data_size_;

  // Helper to compute the total size of all types of site date.
  std::unique_ptr<SiteDataSizeCollector> site_data_size_collector_;

  // The list of other users whose directory sizes will be accumulated as the
  // size of "Other users".
  user_manager::UserList other_users_;

  // Fetched sizes of user directories.
  std::vector<int64_t> user_sizes_;

  // Flags indicating fetch operations for storage sizes are ongoing.
  bool updating_downloads_size_;
  bool updating_browsing_data_size_;
  bool updating_android_size_;
  bool updating_crostini_size_;
  bool updating_other_users_size_;

  // A flag for keeping track of the mojo connection status to the ARC
  // container.
  bool is_android_running_;

  Profile* const profile_;
  const std::string source_name_;
  ScopedObserver<arc::ArcSessionManager, arc::ArcSessionManager::Observer>
      arc_observer_;
  const re2::RE2 special_volume_path_pattern_;

  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
