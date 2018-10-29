// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/browsing_data/site_data_size_collector.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "components/arc/storage_manager/arc_storage_manager.h"
#include "components/user_manager/user.h"

class Profile;

namespace crostini {
enum class CrostiniResult;
}  // namespace crostini

namespace chromeos {
namespace settings {

class StorageHandler : public ::settings::SettingsPageUIHandler {
 public:
  // Enumeration for device state about remaining space. These values must be
  // kept in sync with settings.StorageSpaceState in JS code.
  enum StorageSpaceState {
    STORAGE_SPACE_NORMAL = 0,
    STORAGE_SPACE_LOW = 1,
    STORAGE_SPACE_CRITICALLY_LOW = 2,
  };

  explicit StorageHandler(Profile* profile);
  ~StorageHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override {}

 private:
  // Handlers of JS messages.
  void HandleUpdateStorageInfo(const base::ListValue* unused_args);
  void HandleOpenDownloads(const base::ListValue* unused_args);
  void HandleOpenArcStorage(const base::ListValue* unused_args);
  void HandleClearDriveCache(const base::ListValue* unused_args);

  // Callback called when clearing Drive cache is done.
  void OnClearDriveCacheDone(bool success);

  // Requests updating disk space information.
  void UpdateSizeStat();

  // Callback to update the UI about disk space information.
  void OnGetSizeStat(int64_t* total_size, int64_t* available_size);

  // Requests updating the size of Downloads directory.
  void UpdateDownloadsSize();

  // Callback to update the UI about the size of Downloads directory.
  void OnGetDownloadsSize(int64_t size);

  // Requests updating the size of Drive Cache.
  void UpdateDriveCacheSize();

  // Callback to update the UI about the size of Drive Cache.
  void OnGetDriveCacheSize(int64_t size);

  // Requests updating the size of browsing data.
  void UpdateBrowsingDataSize();

  // Callback to receive the cache size.
  void OnGetCacheSize(bool is_upper_limit, int64_t size);

  // Callback to update the UI about the size of browsing data.
  void OnGetBrowsingDataSize(bool is_site_data, int64_t size);

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
  bool updating_drive_cache_size_;
  bool updating_browsing_data_size_;
  bool updating_android_size_;
  bool updating_crostini_size_;
  bool updating_other_users_size_;

  Profile* const profile_;
  base::WeakPtrFactory<StorageHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(StorageHandler);
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_DEVICE_STORAGE_HANDLER_H_
