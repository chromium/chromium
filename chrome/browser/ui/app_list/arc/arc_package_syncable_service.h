// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_
#define CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "base/macros.h"
#include "chrome/browser/sync/glue/sync_start_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "components/sync/model/syncable_service.h"
#include "components/sync/protocol/arc_package_specifics.pb.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

// Class that syncs ARC pakcages install/uninstall.
class ArcPackageSyncableService : public syncer::SyncableService,
                                  public KeyedService,
                                  public ArcAppListPrefs::Observer {
 public:
  struct SyncItem {
    SyncItem(const std::string& package_name,
             int32_t package_version,
             int64_t last_backup_android_id,
             int64_t last_backup_time);
    const std::string package_name;
    int32_t package_version;
    int64_t last_backup_android_id;
    int64_t last_backup_time;
  };

  ~ArcPackageSyncableService() override;

  static ArcPackageSyncableService* Create(Profile* profile,
                                           ArcAppListPrefs* prefs);
  static ArcPackageSyncableService* Get(content::BrowserContext* context);

  // Returns true if requested package has pending sync request.
  bool IsPackageSyncing(const std::string& package_name) const;

  // syncer::SyncableService:
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> error_handler) override;
  void StopSyncing(syncer::ModelType type) override;
  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override;
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  bool SyncStarted();

 private:
  using SyncItemMap =
      std::unordered_map<std::string, std::unique_ptr<SyncItem>>;

  ArcPackageSyncableService(Profile* profile, ArcAppListPrefs* prefs);

  // ArcAppListPrefs::Observer:
  void OnPackageInstalled(const mojom::ArcPackageInfo& package_info) override;
  void OnPackageModified(const mojom::ArcPackageInfo& package_info) override;
  void OnPackageRemoved(const std::string& package_name,
                        bool uninstalled) override;
  void OnPackageListInitialRefreshed() override;

  // Sends adds/updates sync change to sync server.
  void SendSyncChange(
      const mojom::ArcPackageInfo& package_info,
      const syncer::SyncChange::SyncChangeType& sync_change_type);

  // Creates or updates local syncItem with data change from sync server. Sends
  // request to install/update package to Android.
  bool ProcessSyncItemSpecifics(const sync_pb::ArcPackageSpecifics& specifics);

  // Deletes local syncItem corresponding to data change from sync server.
  // Sends request to uninstall package to Android.
  bool DeleteSyncItemSpecifics(const sync_pb::ArcPackageSpecifics& specifics);

  // Sends install notification for given package to Android.
  void InstallPackage(const SyncItem* sync_item);

  // Sends uninstall notification for given package to Android.
  void UninstallPackage(const SyncItem* sync_item);

  // Returns if a package should be synced.
  // TODO(lgcheng@) Support may need to be added in this function for different
  // use cases.
  bool ShouldSyncPackage(const std::string& package_name) const;

  Profile* const profile_;
  base::OnceClosure wait_until_ready_to_sync_cb_;
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_handler_;

  // Items which are synced.
  SyncItemMap sync_items_;

  // Items new from sync service, waiting for confirmation of installation.
  // These items may never be approved for installation and this structure is
  // used to ensure syncer::SyncDataList GetAllSyncData(syncer::ModelType type)
  // returns consistent results from different devices.
  // API to re-install pending_install_items_ can be created when needed.
  SyncItemMap pending_install_items_;

  // Items to delete from sync service, waiting for confirmation of
  // uninstallation. These items will no longer be counted in
  // syncer::SyncDataList GetAllSyncData(syncer::ModelType type).
  // API to re-uninstall pending_uninstall_items_ can be created when needed.
  SyncItemMap pending_uninstall_items_;

  // Run()ning tells sync to try and start soon, because syncable changes
  // have started happening. It will cause sync to call us back
  // asynchronously via MergeDataAndStartSyncing as soon as possible.
  syncer::SyncableService::StartSyncFlare flare_;

  ArcAppListPrefs* const prefs_;

  DISALLOW_COPY_AND_ASSIGN(ArcPackageSyncableService);
};

}  // namespace arc

#endif  // CHROME_BROWSER_UI_APP_LIST_ARC_ARC_PACKAGE_SYNCABLE_SERVICE_H_
