// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/device_info/device_info_sync_bridge.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "components/sync/base/time.h"
#include "components/sync/device_info/device_info_util.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/protocol/sync.pb.h"

namespace syncer {

using base::Time;
using base::TimeDelta;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;

using Record = ModelTypeStore::Record;
using RecordList = ModelTypeStore::RecordList;
using WriteBatch = ModelTypeStore::WriteBatch;

namespace {

// Find the timestamp for the last time this |device_info| was edited.
Time GetLastUpdateTime(const DeviceInfoSpecifics& specifics) {
  if (specifics.has_last_updated_timestamp()) {
    return ProtoTimeToTime(specifics.last_updated_timestamp());
  } else {
    return Time();
  }
}

// Converts DeviceInfoSpecifics into a freshly allocated DeviceInfo.
std::unique_ptr<DeviceInfo> SpecificsToModel(
    const DeviceInfoSpecifics& specifics) {
  return std::make_unique<DeviceInfo>(
      specifics.cache_guid(), specifics.client_name(),
      specifics.chrome_version(), specifics.sync_user_agent(),
      specifics.device_type(), specifics.signin_scoped_device_id());
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<EntityData> CopyToEntityData(
    const DeviceInfoSpecifics& specifics) {
  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_device_info() = specifics;
  entity_data->non_unique_name = specifics.client_name();
  return entity_data;
}

// Converts DeviceInfo into a freshly allocated DeviceInfoSpecifics. Takes
// |last_updated_timestamp| to set because the model object does not contain
// this concept.
std::unique_ptr<DeviceInfoSpecifics> ModelToSpecifics(
    const DeviceInfo& info,
    int64_t last_updated_timestamp) {
  auto specifics = std::make_unique<DeviceInfoSpecifics>();
  specifics->set_cache_guid(info.guid());
  specifics->set_client_name(info.client_name());
  specifics->set_chrome_version(info.chrome_version());
  specifics->set_sync_user_agent(info.sync_user_agent());
  specifics->set_device_type(info.device_type());
  specifics->set_signin_scoped_device_id(info.signin_scoped_device_id());
  specifics->set_last_updated_timestamp(last_updated_timestamp);
  return specifics;
}

}  // namespace

DeviceInfoSyncBridge::DeviceInfoSyncBridge(
    LocalDeviceInfoProvider* local_device_info_provider,
    OnceModelTypeStoreFactory store_factory,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      local_device_info_provider_(local_device_info_provider),
      weak_ptr_factory_(this) {
  DCHECK(local_device_info_provider);

  // This is not threadsafe, but presuably the provider initializes on the same
  // thread as us so we're okay.
  if (local_device_info_provider->GetLocalDeviceInfo()) {
    OnProviderInitialized();
  } else {
    subscription_ = local_device_info_provider->RegisterOnInitializedCallback(
        base::BindRepeating(&DeviceInfoSyncBridge::OnProviderInitialized,
                            base::Unretained(this)));
  }

  std::move(store_factory)
      .Run(DEVICE_INFO, base::BindOnce(&DeviceInfoSyncBridge::OnStoreCreated,
                                       weak_ptr_factory_.GetWeakPtr()));
}

DeviceInfoSyncBridge::~DeviceInfoSyncBridge() {}

std::unique_ptr<MetadataChangeList>
DeviceInfoSyncBridge::CreateMetadataChangeList() {
  return WriteBatch::CreateMetadataChangeList();
}

base::Optional<ModelError> DeviceInfoSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(has_provider_initialized_);
  DCHECK(change_processor()->IsTrackingMetadata());
  const DeviceInfo* local_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  // If our dependency was yanked out from beneath us, we cannot correctly
  // handle this request, and all our data will be deleted soon.
  if (local_info == nullptr) {
    return {};
  }

  // Local data should typically be near empty, with the only possible value
  // corresponding to this device. This is because on signout all device info
  // data is blown away. However, this simplification is being ignored here and
  // a full difference is going to be calculated to explore what other bridge
  // implementations may look like.
  std::set<std::string> local_guids_to_put;
  for (const auto& kv : all_data_) {
    local_guids_to_put.insert(kv.first);
  }

  bool has_changes = false;
  std::string local_guid = local_info->guid();
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const auto& change : entity_data) {
    const DeviceInfoSpecifics& specifics =
        change.data().specifics.device_info();
    DCHECK_EQ(change.storage_key(), specifics.cache_guid());
    if (specifics.cache_guid() == local_guid) {
      // Don't Put local data if it's the same as the remote copy.
      if (local_info->Equals(*SpecificsToModel(specifics))) {
        local_guids_to_put.erase(local_guid);
      }
    } else {
      // Remote data wins conflicts.
      local_guids_to_put.erase(specifics.cache_guid());
      has_changes = true;
      StoreSpecifics(std::make_unique<DeviceInfoSpecifics>(specifics),
                     batch.get());
    }
  }

  for (const std::string& guid : local_guids_to_put) {
    change_processor()->Put(guid, CopyToEntityData(*all_data_[guid]),
                            metadata_change_list.get());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitAndNotify(std::move(batch), has_changes);
  return {};
}

base::Optional<ModelError> DeviceInfoSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK(has_provider_initialized_);
  const DeviceInfo* local_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  // If our dependency was yanked out from beneath us, we cannot correctly
  // handle this request, and all our data will be deleted soon.
  if (local_info == nullptr) {
    return {};
  }

  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  bool has_changes = false;
  for (EntityChange& change : entity_changes) {
    const std::string guid = change.storage_key();
    // Each device is the authoritative source for itself, ignore any remote
    // changes that have our local cache guid.
    if (guid == local_info->guid()) {
      continue;
    }

    if (change.type() == EntityChange::ACTION_DELETE) {
      has_changes |= DeleteSpecifics(guid, batch.get());
    } else {
      const DeviceInfoSpecifics& specifics =
          change.data().specifics.device_info();
      DCHECK(guid == specifics.cache_guid());
      StoreSpecifics(std::make_unique<DeviceInfoSpecifics>(specifics),
                     batch.get());
      has_changes = true;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitAndNotify(std::move(batch), has_changes);
  return {};
}

void DeviceInfoSyncBridge::GetData(StorageKeyList storage_keys,
                                   DataCallback callback) {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& key : storage_keys) {
    const auto& iter = all_data_.find(key);
    if (iter != all_data_.end()) {
      DCHECK_EQ(key, iter->second->cache_guid());
      batch->Put(key, CopyToEntityData(*iter->second));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void DeviceInfoSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto batch = std::make_unique<MutableDataBatch>();
  for (const auto& kv : all_data_) {
    batch->Put(kv.first, CopyToEntityData(*kv.second));
  }
  std::move(callback).Run(std::move(batch));
}

std::string DeviceInfoSyncBridge::GetClientTag(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return DeviceInfoUtil::SpecificsToTag(entity_data.specifics.device_info());
}

std::string DeviceInfoSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_device_info());
  return entity_data.specifics.device_info().cache_guid();
}

ModelTypeSyncBridge::StopSyncResponse
DeviceInfoSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<MetadataChangeList> delete_metadata_change_list) {
  // TODO(skym, crbug.com/659263): Would it be reasonable to pulse_timer_.Stop()
  // or subscription_.reset() here?

  // Remove all local data, if sync is being disabled, the user has expressed
  // their desire to not have knowledge about other devices.
  if (delete_metadata_change_list) {
    store_->DeleteAllDataAndMetadata(base::DoNothing());
    if (!all_data_.empty()) {
      all_data_.clear();
      NotifyObservers();
    }
  }
  return StopSyncResponse::kModelStillReadyToSync;
}

bool DeviceInfoSyncBridge::IsSyncing() const {
  return !all_data_.empty();
}

std::unique_ptr<DeviceInfo> DeviceInfoSyncBridge::GetDeviceInfo(
    const std::string& client_id) const {
  const ClientIdToSpecifics::const_iterator iter = all_data_.find(client_id);
  if (iter == all_data_.end()) {
    return std::unique_ptr<DeviceInfo>();
  }
  return SpecificsToModel(*iter->second);
}

std::vector<std::unique_ptr<DeviceInfo>>
DeviceInfoSyncBridge::GetAllDeviceInfo() const {
  std::vector<std::unique_ptr<DeviceInfo>> list;
  for (auto iter = all_data_.begin(); iter != all_data_.end(); ++iter) {
    list.push_back(SpecificsToModel(*iter->second));
  }
  return list;
}

void DeviceInfoSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void DeviceInfoSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

int DeviceInfoSyncBridge::CountActiveDevices() const {
  return CountActiveDevices(Time::Now());
}

// static
std::unique_ptr<ModelTypeStore>
DeviceInfoSyncBridge::DestroyAndStealStoreForTest(
    std::unique_ptr<DeviceInfoSyncBridge> bridge) {
  return std::move(bridge->store_);
}

bool DeviceInfoSyncBridge::IsPulseTimerRunningForTest() const {
  return pulse_timer_.IsRunning();
}

void DeviceInfoSyncBridge::ForcePulseForTest() {
  SendLocalData();
}

void DeviceInfoSyncBridge::NotifyObservers() {
  for (auto& observer : observers_)
    observer.OnDeviceInfoChange();
}

void DeviceInfoSyncBridge::StoreSpecifics(
    std::unique_ptr<DeviceInfoSpecifics> specifics,
    WriteBatch* batch) {
  const std::string guid = specifics->cache_guid();
  batch->WriteData(guid, specifics->SerializeAsString());
  all_data_[guid] = std::move(specifics);
}

bool DeviceInfoSyncBridge::DeleteSpecifics(const std::string& guid,
                                           WriteBatch* batch) {
  ClientIdToSpecifics::const_iterator iter = all_data_.find(guid);
  if (iter != all_data_.end()) {
    batch->DeleteData(guid);
    all_data_.erase(iter);
    return true;
  } else {
    return false;
  }
}

void DeviceInfoSyncBridge::OnProviderInitialized() {
  // Now that the provider has initialized, remove the subscription. The bridge
  // should only need to give the processor metadata upon initialization. If
  // sync is disabled and enabled, our provider will try to retrigger this
  // event, but we do not want to send any more metadata to the processor.
  // TODO(skym, crbug.com/672600): Handle re-initialization and start the pulse
  // timer.
  subscription_.reset();

  has_provider_initialized_ = true;
  LoadMetadataIfReady();
}

void DeviceInfoSyncBridge::OnStoreCreated(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(base::BindOnce(&DeviceInfoSyncBridge::OnReadAllData,
                                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceInfoSyncBridge::OnReadAllData(
    const base::Optional<syncer::ModelError>& error,
    std::unique_ptr<RecordList> record_list) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const Record& r : *record_list) {
    std::unique_ptr<DeviceInfoSpecifics> specifics =
        std::make_unique<DeviceInfoSpecifics>();
    if (specifics->ParseFromString(r.value)) {
      all_data_[specifics->cache_guid()] = std::move(specifics);
    } else {
      change_processor()->ReportError(
          {FROM_HERE, "Failed to deserialize specifics."});
      return;
    }
  }

  has_data_loaded_ = true;
  LoadMetadataIfReady();
}

void DeviceInfoSyncBridge::LoadMetadataIfReady() {
  if (has_data_loaded_ && has_provider_initialized_) {
    store_->ReadAllMetadata(
        base::BindOnce(&DeviceInfoSyncBridge::OnReadAllMetadata,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void DeviceInfoSyncBridge::OnReadAllMetadata(
    const base::Optional<ModelError>& error,
    std::unique_ptr<MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  ReconcileLocalAndStored();
}

void DeviceInfoSyncBridge::OnCommit(
    const base::Optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void DeviceInfoSyncBridge::ReconcileLocalAndStored() {
  // On initial syncing we will have a change processor here, but it will not be
  // tracking changes. We need to persist a copy of our local device info to
  // disk, but the Put call to the processor will be ignored. That should be
  // fine however, as the discrepancy will be picked up later in merge. We don't
  // bother trying to track this case and act intelligently because simply not
  // much of a benefit in doing so.
  DCHECK(has_provider_initialized_);

  const DeviceInfo* current_info =
      local_device_info_provider_->GetLocalDeviceInfo();
  // Must ensure |pulse_timer_| is started even if sync is in the process of
  // being disabled. TODO(skym, crbug.com/672600): Remove this timer Start(), as
  // it should be started when the provider re-initializes instead.
  if (current_info == nullptr) {
    pulse_timer_.Start(FROM_HERE, DeviceInfoUtil::kPulseInterval,
                       base::BindRepeating(&DeviceInfoSyncBridge::SendLocalData,
                                           base::Unretained(this)));
    return;
  }
  auto iter = all_data_.find(current_info->guid());

  // Convert to DeviceInfo for Equals function.
  if (iter != all_data_.end() &&
      current_info->Equals(*SpecificsToModel(*iter->second))) {
    const TimeDelta pulse_delay(DeviceInfoUtil::CalculatePulseDelay(
        GetLastUpdateTime(*iter->second), Time::Now()));
    if (!pulse_delay.is_zero()) {
      pulse_timer_.Start(
          FROM_HERE, pulse_delay,
          base::BindRepeating(&DeviceInfoSyncBridge::SendLocalData,
                              base::Unretained(this)));
      return;
    }
  }
  SendLocalData();
}

void DeviceInfoSyncBridge::SendLocalData() {
  DCHECK(has_provider_initialized_);

  // It is possible that the provider no longer has data for us, such as when
  // the user signs out. No-op this pulse, but keep the timer going in case sync
  // is enabled later.
  if (local_device_info_provider_->GetLocalDeviceInfo() != nullptr) {
    std::unique_ptr<DeviceInfoSpecifics> specifics =
        ModelToSpecifics(*local_device_info_provider_->GetLocalDeviceInfo(),
                         TimeToProtoTime(Time::Now()));
    std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();

    if (change_processor()->IsTrackingMetadata()) {
      change_processor()->Put(specifics->cache_guid(),
                              CopyToEntityData(*specifics),
                              batch->GetMetadataChangeList());
    }

    StoreSpecifics(std::move(specifics), batch.get());
    CommitAndNotify(std::move(batch), true);
  }

  pulse_timer_.Start(FROM_HERE, DeviceInfoUtil::kPulseInterval,
                     base::BindRepeating(&DeviceInfoSyncBridge::SendLocalData,
                                         base::Unretained(this)));
}

void DeviceInfoSyncBridge::CommitAndNotify(std::unique_ptr<WriteBatch> batch,
                                           bool should_notify) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&DeviceInfoSyncBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  if (should_notify) {
    NotifyObservers();
  }
}

int DeviceInfoSyncBridge::CountActiveDevices(const Time now) const {
  return std::count_if(all_data_.begin(), all_data_.end(),
                       [now](ClientIdToSpecifics::const_reference pair) {
                         return DeviceInfoUtil::IsActive(
                             GetLastUpdateTime(*pair.second), now);
                       });
}

}  // namespace syncer
