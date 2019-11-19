// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_package_syncable_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/arc/arc_util.h"
#include "components/arc/session/connection_holder.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/model/sync_merge_result.h"
#include "components/sync/protocol/sync.pb.h"

namespace arc {

namespace {

using syncer::SyncChange;
using ArcSyncItem = ArcPackageSyncableService::SyncItem;

constexpr int64_t kNoAndroidID = 0;

std::unique_ptr<ArcSyncItem> CreateSyncItemFromSyncSpecifics(
    const sync_pb::ArcPackageSpecifics& specifics) {
  return std::make_unique<ArcSyncItem>(
      specifics.package_name(), specifics.package_version(),
      specifics.last_backup_android_id(), specifics.last_backup_time());
}

std::unique_ptr<ArcSyncItem> CreateSyncItemFromSyncData(
    const syncer::SyncData& sync_data) {
  const sync_pb::EntitySpecifics& entity_specifics = sync_data.GetSpecifics();
  DCHECK(entity_specifics.has_arc_package());
  const sync_pb::ArcPackageSpecifics& specifics =
      entity_specifics.arc_package();

  return CreateSyncItemFromSyncSpecifics(specifics);
}

void UpdateSyncSpecificsFromSyncItem(const ArcSyncItem* item,
                                     sync_pb::ArcPackageSpecifics* specifics) {
  DCHECK(item);
  DCHECK(specifics);
  specifics->set_package_name(item->package_name);
  specifics->set_package_version(item->package_version);
  specifics->set_last_backup_android_id(item->last_backup_android_id);
  specifics->set_last_backup_time(item->last_backup_time);
}

syncer::SyncData GetSyncDataFromSyncItem(const ArcSyncItem* item) {
  DCHECK(item);
  sync_pb::EntitySpecifics specifics;
  UpdateSyncSpecificsFromSyncItem(item, specifics.mutable_arc_package());
  return syncer::SyncData::CreateLocalData(item->package_name,
                                           item->package_name, specifics);
}

// Creates new syncItem from ArcAppListPrefs::PackageInfo
std::unique_ptr<ArcSyncItem> CreateSyncItemFromPrefs(
    std::unique_ptr<ArcAppListPrefs::PackageInfo> package_info) {
  DCHECK(package_info);
  return std::make_unique<ArcSyncItem>(
      package_info->package_name, package_info->package_version,
      package_info->last_backup_android_id, package_info->last_backup_time);
}

}  // namespace

// ArcPackageSyncableService::SyncItem
ArcSyncItem::SyncItem(const std::string& package_name,
                      int32_t package_version,
                      int64_t last_backup_android_id,
                      int64_t last_backup_time)
    : package_name(package_name),
      package_version(package_version),
      last_backup_android_id(last_backup_android_id),
      last_backup_time(last_backup_time) {}

// ArcPackageSyncableService public
ArcPackageSyncableService::ArcPackageSyncableService(Profile* profile,
                                                     ArcAppListPrefs* prefs)
    : profile_(profile),
      sync_processor_(nullptr),
      sync_error_handler_(nullptr),
      prefs_(prefs) {
  if (prefs_)
    prefs_->AddObserver(this);
}

ArcPackageSyncableService::~ArcPackageSyncableService() {
  if (prefs_)
    prefs_->RemoveObserver(this);
}

// static
ArcPackageSyncableService* ArcPackageSyncableService::Create(
    Profile* profile,
    ArcAppListPrefs* prefs) {
  return new ArcPackageSyncableService(profile, prefs);
}

// static
ArcPackageSyncableService* ArcPackageSyncableService::Get(
    content::BrowserContext* context) {
  return ArcPackageSyncableServiceFactory::GetForBrowserContext(context);
}

bool ArcPackageSyncableService::IsPackageSyncing(
    const std::string& package_name) const {
  return pending_install_items_.find(package_name) !=
      pending_install_items_.end();
}

void ArcPackageSyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(!wait_until_ready_to_sync_cb_);

  if (prefs_->package_list_initial_refreshed()) {
    std::move(done).Run();
    return;
  }

  // Wait until the initial list is loaded, handled in
  // OnPackageListInitialRefreshed().
  wait_until_ready_to_sync_cb_ = std::move(done);
}

syncer::SyncMergeResult ArcPackageSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());
  DCHECK_EQ(type, syncer::ARC_PACKAGE);
  DCHECK(!sync_processor_.get());
  DCHECK(!IsArcAppSyncFlowDisabled());
  DCHECK(prefs_->package_list_initial_refreshed());

  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(error_handler);

  syncer::SyncMergeResult result = syncer::SyncMergeResult(type);

  const std::vector<std::string> local_packages =
      prefs_->GetPackagesFromPrefs();
  const std::unordered_set<std::string> local_package_set(
      local_packages.begin(), local_packages.end());

  // Creates sync items from synced data.
  for (const syncer::SyncData& sync_data : initial_sync_data) {
    std::unique_ptr<ArcSyncItem> sync_item(
        CreateSyncItemFromSyncData(sync_data));
    const std::string& package_name = sync_item->package_name;

    if (!ShouldSyncPackage(package_name))
      continue;

    if (!base::Contains(local_package_set, package_name)) {
      pending_install_items_[package_name] = std::move(sync_item);
      InstallPackage(pending_install_items_[package_name].get());
    } else {
      // TODO(lgcheng@) may need to handle update exsiting package here.
      sync_items_[package_name] = std::move(sync_item);
    }
  }

  // Creates sync items for local unsynced packages.
  syncer::SyncChangeList change_list;
  for (const auto& local_package_name : local_packages) {
    if (base::Contains(sync_items_, local_package_name))
      continue;

    if (!ShouldSyncPackage(local_package_name))
      continue;

    std::unique_ptr<ArcSyncItem> sync_item(
        CreateSyncItemFromPrefs(prefs_->GetPackage(local_package_name)));
    change_list.push_back(SyncChange(FROM_HERE, SyncChange::ACTION_ADD,
                                     GetSyncDataFromSyncItem(sync_item.get())));
    sync_items_[local_package_name] = std::move(sync_item);
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, change_list);
  return result;
}

void ArcPackageSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_EQ(type, syncer::ARC_PACKAGE);

  sync_processor_.reset();
  sync_error_handler_.reset();
  flare_.Reset();

  sync_items_.clear();
  pending_install_items_.clear();
  pending_uninstall_items_.clear();
}

syncer::SyncDataList ArcPackageSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_EQ(type, syncer::ARC_PACKAGE);

  syncer::SyncDataList list;
  for (const auto& item : sync_items_)
    list.emplace_back(GetSyncDataFromSyncItem(item.second.get()));

  for (const auto& item : pending_install_items_)
    list.emplace_back(GetSyncDataFromSyncItem(item.second.get()));

  return list;
}

syncer::SyncError ArcPackageSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (!sync_processor_.get()) {
    return syncer::SyncError(FROM_HERE, syncer::SyncError::DATATYPE_ERROR,
                             "ARC package syncable service is not started.",
                             syncer::ARC_PACKAGE);
  }

  for (const auto& change : change_list) {
    const std::string package_name =
        change.sync_data().GetSpecifics().arc_package().package_name();
    VLOG(2) << this << "  Change: " << package_name << " ("
            << change.change_type() << ")";
    if (!ShouldSyncPackage(package_name)) {
      VLOG(2) << this << package_name
              << " is default app, ignore remote update.";
      continue;
    }

    if (change.change_type() == SyncChange::ACTION_ADD ||
        change.change_type() == SyncChange::ACTION_UPDATE) {
      ProcessSyncItemSpecifics(change.sync_data().GetSpecifics().arc_package());
    } else if (change.change_type() == SyncChange::ACTION_DELETE) {
      DeleteSyncItemSpecifics(change.sync_data().GetSpecifics().arc_package());
    } else {
      VLOG(2) << "Invalid sync change";
    }
  }

  return syncer::SyncError();
}

bool ArcPackageSyncableService::SyncStarted() {
  if (sync_processor_.get())
    return true;

  if (flare_.is_null()) {
    VLOG(2) << this << ": SyncStarted: Flare.";
    flare_ = sync_start_util::GetFlareForSyncableService(profile_->GetPath());
    flare_.Run(syncer::ARC_PACKAGE);
  }
  return false;
}

// ArcPackageSyncableService private
void ArcPackageSyncableService::OnPackageRemoved(
    const std::string& package_name,
    bool uninstalled) {
  if (!uninstalled)
    return;

  if (!ShouldSyncPackage(package_name))
    return;

  SyncItemMap::iterator delete_iter =
      pending_uninstall_items_.find(package_name);

  // Pending uninstall item. Confirm uninstall.
  if (delete_iter != pending_uninstall_items_.end()) {
    pending_uninstall_items_.erase(delete_iter);
    return;
  }

  SyncItemMap::iterator iter = sync_items_.find(package_name);
  if (iter == sync_items_.end()) {
    VLOG(2) << "Request to remove package which does not exist.";
    return;
  }

  if (!SyncStarted()) {
    VLOG(2) << "Request to send sync change when sync service is not started.";
    return;
  }

  SyncChange sync_change(FROM_HERE, SyncChange::ACTION_DELETE,
                         GetSyncDataFromSyncItem(iter->second.get()));
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      syncer::SyncChangeList(1, sync_change));

  sync_items_.erase(iter);
}

void ArcPackageSyncableService::OnPackageInstalled(
    const mojom::ArcPackageInfo& package_info) {
  const std::string& package_name = package_info.package_name;

  if (!ShouldSyncPackage(package_name))
    return;

  SyncItemMap::iterator install_iter =
      pending_install_items_.find(package_name);

  // Pending install item. Confirm install.
  if (install_iter != pending_install_items_.end()) {
    if (install_iter->second->last_backup_android_id == kNoAndroidID &&
        package_info.last_backup_android_id != kNoAndroidID) {
      pending_install_items_.erase(install_iter);
      SendSyncChange(package_info, SyncChange::ACTION_UPDATE);
      return;
    }

    sync_items_[package_name] = std::move(install_iter->second);
    pending_install_items_.erase(install_iter);
    return;
  }

  SyncItemMap::iterator iter = sync_items_.find(package_name);
  if (iter != sync_items_.end()) {
    VLOG(2) << "Request sync service to add new package which does exist.";
    return;
  }

  SendSyncChange(package_info, SyncChange::ACTION_ADD);
}

void ArcPackageSyncableService::OnPackageModified(
    const mojom::ArcPackageInfo& package_info) {
  const std::string& package_name = package_info.package_name;

  if (!ShouldSyncPackage(package_name))
    return;

  SyncItemMap::iterator iter = sync_items_.find(package_name);
  if (iter == sync_items_.end()) {
    VLOG(2) << "Request sync service to update package which does not exist.";
    return;
  }

  SendSyncChange(package_info, SyncChange::ACTION_UPDATE);
}

void ArcPackageSyncableService::OnPackageListInitialRefreshed() {
  if (wait_until_ready_to_sync_cb_)
    std::move(wait_until_ready_to_sync_cb_).Run();
}

void ArcPackageSyncableService::SendSyncChange(
    const mojom::ArcPackageInfo& package_info,
    const syncer::SyncChange::SyncChangeType& sync_change_type) {
  const std::string& package_name = package_info.package_name;

  if (!SyncStarted()) {
    VLOG(2) << "Request to send sync change when sync service is not started.";
    return;
  }

  std::unique_ptr<ArcSyncItem> sync_item = std::make_unique<ArcSyncItem>(
      package_info.package_name, package_info.package_version,
      package_info.last_backup_android_id, package_info.last_backup_time);

  SyncChange sync_change(FROM_HERE, sync_change_type,
                         GetSyncDataFromSyncItem(sync_item.get()));
  sync_processor_->ProcessSyncChanges(FROM_HERE,
                                      syncer::SyncChangeList(1, sync_change));

  sync_items_[package_name] = std::move(sync_item);
}

bool ArcPackageSyncableService::ProcessSyncItemSpecifics(
    const sync_pb::ArcPackageSpecifics& specifics) {
  const std::string& package_name = specifics.package_name();
  if (package_name.empty()) {
    VLOG(2) << "Add/Update ARC package item with empty package name";
    return false;
  }

  SyncItemMap::const_iterator iter = sync_items_.find(package_name);
  if (iter != sync_items_.end()) {
    // TODO(lgcheng@) may need to create update exsiting package logic here.
    return true;
  }

  SyncItemMap::const_iterator pending_iter =
      pending_install_items_.find(package_name);
  if (pending_iter != pending_install_items_.end()) {
    // TODO(lgcheng@) may need to create update pending install package
    // logic here.
    return true;
  }

  std::unique_ptr<ArcSyncItem> sync_item(
      CreateSyncItemFromSyncSpecifics(specifics));
  pending_install_items_[package_name] = std::move(sync_item);
  InstallPackage(pending_install_items_[package_name].get());
  return true;
}

bool ArcPackageSyncableService::DeleteSyncItemSpecifics(
    const sync_pb::ArcPackageSpecifics& specifics) {
  const std::string& package_name = specifics.package_name();
  if (package_name.empty()) {
    VLOG(2) << "Delete ARC package item with empty package name";
    return false;
  }

  SyncItemMap::const_iterator delete_iter =
      pending_install_items_.find(package_name);
  // Ignore this this delete sync change. Package is pending uninstall.
  if (delete_iter != pending_install_items_.end()) {
    pending_install_items_.erase(delete_iter);
    return true;
  }

  SyncItemMap::iterator iter = sync_items_.find(package_name);
  if (iter != sync_items_.end()) {
    pending_uninstall_items_[package_name] = std::move(iter->second);
    UninstallPackage(pending_uninstall_items_[package_name].get());
    sync_items_.erase(iter);
    return true;
  }
  // TODO(lgcheng@) may need to handle the situation that the package is
  // pending install.
  return true;
}

void ArcPackageSyncableService::InstallPackage(const ArcSyncItem* sync_item) {
  DCHECK(sync_item);
  if (!prefs_) {
    VLOG(2) << "Request to install package when bridge service is not ready: "
            << sync_item->package_name << ".";
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(prefs_->app_connection_holder(),
                                               InstallPackage);
  if (!instance)
    return;

  mojom::ArcPackageInfo package;
  package.package_name = sync_item->package_name;
  package.package_version = sync_item->package_version;
  package.last_backup_android_id = sync_item->last_backup_android_id;
  package.last_backup_time = sync_item->last_backup_time;
  package.sync = true;
  instance->InstallPackage(package.Clone());
}

void ArcPackageSyncableService::UninstallPackage(const ArcSyncItem* sync_item) {
  DCHECK(sync_item);
  if (!prefs_) {
    VLOG(2) << "Request to uninstall package when bridge service is not ready: "
            << sync_item->package_name << ".";
    return;
  }

  auto* instance = ARC_GET_INSTANCE_FOR_METHOD(prefs_->app_connection_holder(),
                                               UninstallPackage);
  if (!instance)
    return;

  // Make a copy of the package name string instead of handing out a reference
  // to |sync_item->package_name|. The reason is that |sync_item| may get
  // destroyed as a result of this call, making any references to it invalid.
  // See crbug.com/970063.
  std::string package_name = sync_item->package_name;
  instance->UninstallPackage(package_name);
}

bool ArcPackageSyncableService::ShouldSyncPackage(
    const std::string& package_name) const {
  // Don't sync default apps.
  if (prefs_->IsDefaultPackage(package_name))
    return false;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package(
      prefs_->GetPackage(package_name));
  if (package.get())
    return package->should_sync;

  // A non default package from remote should be synced.
  return true;
}

}  // namespace arc
