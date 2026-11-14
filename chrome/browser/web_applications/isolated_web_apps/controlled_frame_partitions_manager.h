// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CONTROLLED_FRAME_PARTITIONS_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CONTROLLED_FRAME_PARTITIONS_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class Profile;

namespace content {
class StoragePartitionConfig;
}  // namespace content

namespace web_app {

class IsolatedWebAppUrlInfo;
class WebAppProvider;

// Tracks active in-memory (ephemeral) Controlled Frame
// `StoragePartitionConfig`s per Isolated Web App, cleared on app
// uninstallation (or expiring on browser shutdown).
//
// Owned by `WebAppIsolationDelegateImpl`, registered on
// `WebAppProvider::unowned_user_data_host()` (retrieved via
// `ControlledFramePartitionsManager::From`), and must only be accessed on the
// UI thread.
class ControlledFramePartitionsManager {
 public:
  DECLARE_USER_DATA(ControlledFramePartitionsManager);

  static ControlledFramePartitionsManager& From(WebAppProvider& provider);
  static const ControlledFramePartitionsManager& From(
      const WebAppProvider& provider);

  ControlledFramePartitionsManager(Profile& profile,
                                   ui::UnownedUserDataHost& host);
  ControlledFramePartitionsManager(const ControlledFramePartitionsManager&) =
      delete;
  ControlledFramePartitionsManager& operator=(
      const ControlledFramePartitionsManager&) = delete;
  ~ControlledFramePartitionsManager();

  // Saves a record of `partition_name` in
  // `in_memory_controlled_frame_partitions_` and returns the
  // `StoragePartitionConfig` of the in-memory Controlled Frame partition, or
  // `std::nullopt` if `url_info` does not refer to an installed Isolated Web
  // App.
  std::optional<content::StoragePartitionConfig>
  SaveAndGetInMemoryPartitionConfig(const IsolatedWebAppUrlInfo& url_info,
                                    const std::string& partition_name);

  // Returns the `StoragePartitionConfig`s of all recorded in-memory Controlled
  // Frame partitions for `url_info`.
  std::vector<content::StoragePartitionConfig> GetInMemoryPartitionConfigs(
      const IsolatedWebAppUrlInfo& url_info) const;

  // Removes any recorded in-memory Controlled Frame partitions for `app_id`.
  void ClearInMemoryPartitions(const webapps::AppId& app_id);

  base::Value AsDebugValue() const;

  base::WeakPtr<ControlledFramePartitionsManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  const raw_ref<Profile> profile_;

  // Keeps a record of in-memory (non-persistent) Storage Partitions created by
  // Isolated Web Apps' Controlled Frames. Entries are cleared after browsing
  // data removal completes on app uninstallation, or expire on browser
  // shutdown along with in-memory Storage Partitions.
  base::flat_map<webapps::AppId, base::flat_set<std::string>>
      in_memory_controlled_frame_partitions_;

  ui::ScopedUnownedUserData<ControlledFramePartitionsManager>
      scoped_data_holder_;

  base::WeakPtrFactory<ControlledFramePartitionsManager> weak_factory_{this};
};

// Returns the `StoragePartitionConfig` of all `StoragePartition`s used by
// `app_id` if `app_id` is an Isolated Web App in `provider`; both the primary
// and any `<controlledframe>` `StoragePartition`s (persisted and in-memory)
// will be returned. Returns an empty vector if Isolated Web Apps are disabled
// or the app is not an Isolated Web App.
std::vector<content::StoragePartitionConfig>
GetIsolatedWebAppStoragePartitionConfigs(const WebAppProvider& provider,
                                         const webapps::AppId& app_id);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_CONTROLLED_FRAME_PARTITIONS_MANAGER_H_
