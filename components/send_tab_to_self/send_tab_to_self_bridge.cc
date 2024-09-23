// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace send_tab_to_self {

namespace {

using syncer::DataTypeStore;

const base::TimeDelta kDedupeTime = base::Seconds(5);

const base::TimeDelta kDeviceExpiration = base::Days(10);

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(proto_t));
}

// Allocate a EntityData and copies |specifics| into it.
std::unique_ptr<syncer::EntityData> CopyToEntityData(
    const sync_pb::SendTabToSelfSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_send_tab_to_self() = specifics;
  entity_data->name = specifics.url();
  entity_data->creation_time = ProtoTimeToTime(specifics.shared_time_usec());
  return entity_data;
}

// Parses the content of |record_list| into |*initial_data|. The output
// parameter is first for binding purposes.
std::optional<syncer::ModelError> ParseLocalEntriesOnBackendSequence(
    base::Time now,
    std::map<std::string, std::unique_ptr<SendTabToSelfEntry>>* entries,
    std::string* local_personalizable_device_name,
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  DCHECK(entries);
  DCHECK(entries->empty());
  DCHECK(local_personalizable_device_name);
  DCHECK(record_list);

  *local_personalizable_device_name =
      syncer::GetPersonalizableDeviceNameBlocking();

  for (const syncer::DataTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<SendTabToSelfLocal>();
    if (specifics->ParseFromString(r.value)) {
      (*entries)[specifics->specifics().guid()] =
          SendTabToSelfEntry::FromLocalProto(*specifics, now);
    } else {
      return syncer::ModelError(FROM_HERE, "Failed to deserialize specifics.");
    }
  }

  return std::nullopt;
}

}  // namespace

SendTabToSelfBridge::SendTabToSelfBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    base::Clock* clock,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    history::HistoryService* history_service,
    syncer::DeviceInfoTracker* device_info_tracker)
    : DataTypeSyncBridge(std::move(change_processor)),
      clock_(clock),
      history_service_(history_service),
      device_info_tracker_(device_info_tracker),
      mru_entry_(nullptr) {
  DCHECK(clock_);
  DCHECK(device_info_tracker_);
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }
  device_info_tracker_observation_.Observe(device_info_tracker);

  ComputeTargetDeviceInfoSortedList();

  std::move(create_store_callback)
      .Run(syncer::SEND_TAB_TO_SELF,
           base::BindOnce(&SendTabToSelfBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

SendTabToSelfBridge::~SendTabToSelfBridge() {
  if (history_service_) {
    history_service_observation_.Reset();
  }
}

std::unique_ptr<syncer::MetadataChangeList>
SendTabToSelfBridge::CreateMetadataChangeList() {
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> SendTabToSelfBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(entries_.empty());
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
SendTabToSelfBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<const SendTabToSelfEntry*> added;

  // The opened vector will accumulate both added entries that are already
  // opened as well as existing entries that have been updated to be marked as
  // opened.
  std::vector<const SendTabToSelfEntry*> opened;
  std::vector<std::string> removed;
  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const std::string& guid = change->storage_key();
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      if (entries_.find(guid) != entries_.end()) {
        EraseEntryInBatch(guid, batch.get());
        removed.push_back(change->storage_key());
      }
    } else {
      const sync_pb::SendTabToSelfSpecifics& specifics =
          change->data().specifics.send_tab_to_self();

      std::unique_ptr<SendTabToSelfEntry> remote_entry =
          SendTabToSelfEntry::FromProto(specifics, clock_->Now());
      if (!remote_entry) {
        continue;  // Skip invalid entries.
      }
      if (remote_entry->IsExpired(clock_->Now())) {
        // Remove expired data from server.
        change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                                   batch->GetMetadataChangeList());
      } else {
        SendTabToSelfEntry* local_entry =
            GetMutableEntryByGUID(remote_entry->GetGUID());
        SendTabToSelfLocal remote_entry_pb = remote_entry->AsLocalProto();
        if (local_entry == nullptr) {
          if (unknown_opened_entries_.contains(remote_entry->GetGUID())) {
            unknown_opened_entries_.erase(remote_entry->GetGUID());
            remote_entry->MarkOpened();
            // Reupload the entry to the server. This operation is safe because
            // it is happening at most once per entry.
            change_processor()->Put(
                remote_entry->GetGUID(),
                CopyToEntityData(remote_entry->AsLocalProto().specifics()),
                batch->GetMetadataChangeList());
          }
          // This remote_entry is new. Add it to the model.
          added.push_back(remote_entry.get());
          if (remote_entry->IsOpened()) {
            opened.push_back(remote_entry.get());
          }
          entries_[remote_entry->GetGUID()] = std::move(remote_entry);
        } else {
          // Update existing model if entries have been opened.
          if (remote_entry->IsOpened() && !local_entry->IsOpened()) {
            local_entry->MarkOpened();
            opened.push_back(local_entry);
          }
        }

        // Write to the store.
        batch->WriteData(guid, remote_entry_pb.SerializeAsString());
      }
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  Commit(std::move(batch));

  NotifyRemoteSendTabToSelfEntryDeleted(removed);
  NotifyRemoteSendTabToSelfEntryAdded(added);
  NotifyRemoteSendTabToSelfEntryOpened(opened);

  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> SendTabToSelfBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();

  for (const std::string& guid : storage_keys) {
    const SendTabToSelfEntry* entry = GetEntryByGUID(guid);
    if (!entry) {
      continue;
    }

    batch->Put(guid, CopyToEntityData(entry->AsLocalProto().specifics()));
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
SendTabToSelfBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& it : entries_) {
    batch->Put(it.first,
               CopyToEntityData(it.second->AsLocalProto().specifics()));
  }
  return batch;
}

std::string SendTabToSelfBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string SendTabToSelfBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.send_tab_to_self().guid();
}

void SendTabToSelfBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK(store_);

  store_->DeleteAllDataAndMetadata(base::DoNothing());

  std::vector<std::string> all_guids = GetAllGuids();

  entries_.clear();
  mru_entry_ = nullptr;
  NotifyRemoteSendTabToSelfEntryDeleted(all_guids);
}

std::vector<std::string> SendTabToSelfBridge::GetAllGuids() const {
  std::vector<std::string> keys;
  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->GetGUID());
    keys.push_back(it.first);
  }
  return keys;
}

const SendTabToSelfEntry* SendTabToSelfBridge::GetEntryByGUID(
    const std::string& guid) const {
  auto it = entries_.find(guid);
  if (it == entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

const SendTabToSelfEntry* SendTabToSelfBridge::AddEntry(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid) {
  if (!change_processor()->IsTrackingMetadata()) {
    // TODO(crbug.com/40617641) handle failure case.
    return nullptr;
  }

  if (!url.is_valid()) {
    return nullptr;
  }

  // In the case where the user has attempted to send an identical URL to the
  // same device within the last |kDedupeTime| we think it is likely that user
  // still has the first sent tab in progress, and so we will not attempt to
  // resend.
  base::Time shared_time = clock_->Now();
  if (mru_entry_ && url == mru_entry_->GetURL() &&
      target_device_cache_guid == mru_entry_->GetTargetDeviceSyncCacheGuid() &&
      shared_time - mru_entry_->GetSharedTime() < kDedupeTime) {
    send_tab_to_self::RecordNotificationThrottled();
    return mru_entry_;
  }

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Assure that we don't have a guid collision.
  DCHECK_EQ(GetEntryByGUID(guid), nullptr);

  std::string trimmed_title = "";

  if (base::IsStringUTF8(title)) {
    trimmed_title = base::UTF16ToUTF8(
        base::CollapseWhitespace(base::UTF8ToUTF16(title), false));
  }

  auto entry = std::make_unique<SendTabToSelfEntry>(
      guid, url, trimmed_title, shared_time, local_device_name_,
      target_device_cache_guid);

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();
  // This entry is new. Add it to the store and model.
  auto entity_data = CopyToEntityData(entry->AsLocalProto().specifics());

  change_processor()->Put(guid, std::move(entity_data),
                          batch->GetMetadataChangeList());

  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntryAddedLocally(entry.get());
  }

  const SendTabToSelfEntry* result =
      entries_.emplace(guid, std::move(entry)).first->second.get();

  batch->WriteData(guid, result->AsLocalProto().SerializeAsString());

  Commit(std::move(batch));
  mru_entry_ = result;

  return result;
}

void SendTabToSelfBridge::DeleteEntry(const std::string& guid) {
  // Assure that an entry with that guid exists.
  if (GetEntryByGUID(guid) == nullptr) {
    return;
  }

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  DeleteEntryWithBatch(guid, batch.get());

  Commit(std::move(batch));
}

void SendTabToSelfBridge::DismissEntry(const std::string& guid) {
  SendTabToSelfEntry* entry = GetMutableEntryByGUID(guid);
  // Assure that an entry with that guid exists.
  if (!entry) {
    return;
  }

  DCHECK(change_processor()->IsTrackingMetadata());

  entry->SetNotificationDismissed(true);

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  auto entity_data = CopyToEntityData(entry->AsLocalProto().specifics());

  change_processor()->Put(guid, std::move(entity_data),
                          batch->GetMetadataChangeList());

  batch->WriteData(guid, entry->AsLocalProto().SerializeAsString());
  Commit(std::move(batch));
}

void SendTabToSelfBridge::MarkEntryOpened(const std::string& guid) {
  SendTabToSelfEntry* entry = GetMutableEntryByGUID(guid);
  // Assure that an entry with that guid exists.
  if (!entry) {
    unknown_opened_entries_.insert(guid);
    return;
  }

  DCHECK(change_processor()->IsTrackingMetadata());

  entry->MarkOpened();

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  auto entity_data = CopyToEntityData(entry->AsLocalProto().specifics());

  change_processor()->Put(guid, std::move(entity_data),
                          batch->GetMetadataChangeList());

  batch->WriteData(guid, entry->AsLocalProto().SerializeAsString());
  Commit(std::move(batch));
}

void SendTabToSelfBridge::OnHistoryDeletions(
    history::HistoryService* history_service,
    const history::DeletionInfo& deletion_info) {
  // We only care about actual user (or sync) deletions.

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  if (deletion_info.is_from_expiration())
    return;

  if (!deletion_info.IsAllHistory()) {
    std::vector<GURL> urls;

    for (const history::URLRow& row : deletion_info.deleted_rows()) {
      urls.push_back(row.url());
    }

    DeleteEntries(urls);
    return;
  }

  // All history was cleared: just delete all entries.
  DeleteAllEntries();
}

void SendTabToSelfBridge::OnDeviceInfoChange() {
  TRACE_EVENT0("ui", "SendTabToSelfBridge::OnDeviceInfoChange");
  ComputeTargetDeviceInfoSortedList();
}

bool SendTabToSelfBridge::IsReady() {
  return change_processor()->IsTrackingMetadata();
}

bool SendTabToSelfBridge::HasValidTargetDevice() {
  return GetTargetDeviceInfoSortedList().size() > 0;
}

std::vector<TargetDeviceInfo>
SendTabToSelfBridge::GetTargetDeviceInfoSortedList() {
  // Filter expired devices (some timestamps in the cached list may now be too
  // old). |target_device_info_sorted_list_| is copied here to avoid mutations
  // inside a getter.
  // TODO(crbug.com/40200734): Consider having a timer that fires on the next
  // expiry and removes the corresponding device(s) then.
  std::vector<TargetDeviceInfo> non_expired_devices =
      target_device_info_sorted_list_;
  const base::Time now = clock_->Now();
  std::erase_if(non_expired_devices, [now](const TargetDeviceInfo& device) {
    return now - device.last_updated_timestamp > kDeviceExpiration;
  });

  return non_expired_devices;
}

// static
std::unique_ptr<syncer::DataTypeStore>
SendTabToSelfBridge::DestroyAndStealStoreForTest(
    std::unique_ptr<SendTabToSelfBridge> bridge) {
  return std::move(bridge->store_);
}

void SendTabToSelfBridge::SetLocalDeviceNameForTest(
    const std::string& local_device_name) {
  local_device_name_ = local_device_name;
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryAdded(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  if (new_entries.empty()) {
    return;
  }

  std::vector<const SendTabToSelfEntry*> new_local_entries;

  // Only pass along entries that are not dismissed or opened, and are
  // targeted at this device, which is determined by comparing the cache guid
  // associated with the entry to each device's local list of recently used
  // cache_guids
  DCHECK(!change_processor()->TrackedCacheGuid().empty());
  for (const SendTabToSelfEntry* entry : new_entries) {
    if (device_info_tracker_->IsRecentLocalCacheGuid(
            entry->GetTargetDeviceSyncCacheGuid()) &&
        !entry->GetNotificationDismissed() && !entry->IsOpened()) {
      new_local_entries.push_back(entry);
    }
  }

  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntriesAddedRemotely(new_local_entries);
  }
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryDeleted(
    const std::vector<std::string>& guids) {
  if (guids.empty()) {
    return;
  }

  // TODO(crbug.com/40624520): Only send the entries that targeted this device.
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntriesRemovedRemotely(guids);
  }
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryOpened(
    const std::vector<const SendTabToSelfEntry*>& opened_entries) {
  if (opened_entries.empty()) {
    return;
  }
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntriesOpenedRemotely(opened_entries);
  }
}

void SendTabToSelfBridge::NotifySendTabToSelfModelLoaded() {
  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.SendTabToSelfModelLoaded();
  }
}

void SendTabToSelfBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  auto initial_entries = std::make_unique<SendTabToSelfEntries>();
  SendTabToSelfEntries* initial_entries_copy = initial_entries.get();

  auto local_device_name = std::make_unique<std::string>();
  std::string* local_device_name_copy = local_device_name.get();

  store_ = std::move(store);
  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseLocalEntriesOnBackendSequence, clock_->Now(),
                     base::Unretained(initial_entries_copy),
                     base::Unretained(local_device_name_copy)),
      base::BindOnce(&SendTabToSelfBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(initial_entries),
                     std::move(local_device_name)));
}

void SendTabToSelfBridge::OnReadAllData(
    std::unique_ptr<SendTabToSelfEntries> initial_entries,
    std::unique_ptr<std::string> local_device_name,
    const std::optional<syncer::ModelError>& error) {
  DCHECK(initial_entries);
  DCHECK(local_device_name);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  entries_ = std::move(*initial_entries);
  local_device_name_ = std::move(*local_device_name);

  store_->ReadAllMetadata(base::BindOnce(
      &SendTabToSelfBridge::OnReadAllMetadata, weak_ptr_factory_.GetWeakPtr()));
}

void SendTabToSelfBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "SendTabToSelfBridge::OnReadAllMetadata");
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  NotifySendTabToSelfModelLoaded();

  DoGarbageCollection();
}

void SendTabToSelfBridge::OnCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void SendTabToSelfBridge::Commit(
    std::unique_ptr<DataTypeStore::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&SendTabToSelfBridge::OnCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

SendTabToSelfEntry* SendTabToSelfBridge::GetMutableEntryByGUID(
    const std::string& guid) const {
  auto it = entries_.find(guid);
  if (it == entries_.end()) {
    return nullptr;
  }
  return it->second.get();
}

void SendTabToSelfBridge::DoGarbageCollection() {
  std::vector<std::string> removed;

  auto entry = entries_.begin();
  while (entry != entries_.end()) {
    DCHECK_EQ(entry->first, entry->second->GetGUID());

    std::string guid = entry->first;
    bool expired = entry->second->IsExpired(clock_->Now());
    entry++;
    if (expired) {
      DeleteEntry(guid);
      removed.push_back(guid);
    }
  }
  NotifyRemoteSendTabToSelfEntryDeleted(removed);
}

void SendTabToSelfBridge::ComputeTargetDeviceInfoSortedList() {
  TRACE_EVENT0("ui", "SendTabToSelfBridge::ComputeTargetDeviceInfoSortedList");
  if (!device_info_tracker_->IsSyncing()) {
    return;
  }

  std::vector<const syncer::DeviceInfo*> all_devices =
      device_info_tracker_->GetAllDeviceInfo();

  // Sort the DeviceInfo vector so the most recently modified devices are first.
  std::stable_sort(all_devices.begin(), all_devices.end(),
                   [](const syncer::DeviceInfo* device1,
                      const syncer::DeviceInfo* device2) {
                     return device1->last_updated_timestamp() >
                            device2->last_updated_timestamp();
                   });

  target_device_info_sorted_list_.clear();
  std::set<std::string> unique_device_names;
  std::unordered_map<std::string, int> short_names_counter;
  for (const syncer::DeviceInfo* device : all_devices) {
    // If the current device is considered expired for our purposes, stop here
    // since the next devices in the vector are at least as expired than this
    // one.
    if (clock_->Now() - device->last_updated_timestamp() > kDeviceExpiration) {
      break;
    }

    // Don't include this device if it is the local device.
    if (device_info_tracker_->IsRecentLocalCacheGuid(device->guid())) {
      continue;
    }

    DCHECK_NE(device->guid(), change_processor()->TrackedCacheGuid());

    // Don't include devices that have disabled the send tab to self receiving
    // feature.
    if (!device->send_tab_to_self_receiving_enabled()) {
      continue;
    }

    SharingDeviceNames device_names = GetSharingDeviceNames(device);

    // Don't include this device if it has the same name as the local device.
    if (device_names.full_name == local_device_name_) {
      continue;
    }

    // Only keep one device per device name. We only keep the first occurrence
    // which is the most recent.
    if (unique_device_names.insert(device_names.full_name).second) {
      TargetDeviceInfo target_device_info(
          device_names.full_name, device_names.short_name, device->guid(),
          device->form_factor(), device->last_updated_timestamp());
      target_device_info_sorted_list_.push_back(target_device_info);

      short_names_counter[device_names.short_name]++;
    }
  }
  for (auto& device_info : target_device_info_sorted_list_) {
    bool unique_short_name = short_names_counter[device_info.short_name] == 1;
    device_info.device_name =
        (unique_short_name ? device_info.short_name : device_info.full_name);
  }
}

void SendTabToSelfBridge::DeleteEntryWithBatch(
    const std::string& guid,
    DataTypeStore::WriteBatch* batch) {
  // Assure that an entry with that guid exists.
  DCHECK(GetEntryByGUID(guid) != nullptr);
  DCHECK(change_processor()->IsTrackingMetadata());

  change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());

  EraseEntryInBatch(guid, batch);
}

void SendTabToSelfBridge::DeleteEntries(const std::vector<GURL>& urls) {
  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  std::vector<std::string> removed_guids;

  for (const GURL& url : urls) {
    auto entry = entries_.begin();
    while (entry != entries_.end()) {
      bool to_delete =
          (entry->second == nullptr || url == entry->second->GetURL());

      std::string guid = entry->first;
      entry++;
      if (to_delete) {
        removed_guids.push_back(guid);
        DeleteEntryWithBatch(guid, batch.get());
      }
    }
  }
  Commit(std::move(batch));
  // To err on the side of completeness this notifies all clients that these
  // entries have been removed. Regardless of if these entries were removed
  // "remotely".
  NotifyRemoteSendTabToSelfEntryDeleted(removed_guids);
}

void SendTabToSelfBridge::DeleteAllEntries() {
  if (!change_processor()->IsTrackingMetadata()) {
    DCHECK_EQ(0ul, entries_.size());
    return;
  }

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();

  std::vector<std::string> all_guids = GetAllGuids();

  for (const auto& guid : all_guids) {
    change_processor()->Delete(guid, syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
    batch->DeleteData(guid);
  }
  entries_.clear();
  mru_entry_ = nullptr;

  NotifyRemoteSendTabToSelfEntryDeleted(all_guids);
}

void SendTabToSelfBridge::EraseEntryInBatch(const std::string& guid,
                                            DataTypeStore::WriteBatch* batch) {
  if (mru_entry_ && mru_entry_->GetGUID() == guid) {
    mru_entry_ = nullptr;
  }
  entries_.erase(guid);
  unknown_opened_entries_.erase(guid);
  batch->DeleteData(guid);
}

}  // namespace send_tab_to_self
