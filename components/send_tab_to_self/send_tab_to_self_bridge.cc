// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_bridge.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "components/history/core/browser/history_service.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/pref_names.h"
#include "components/send_tab_to_self/proto/send_tab_to_self.pb.h"
#include "components/send_tab_to_self/proto_conversions.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync_device_info/device_name_util.h"
#include "components/sync_device_info/local_device_info_util.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
namespace send_tab_to_self {

namespace {

using syncer::DataTypeStore;

const base::TimeDelta kDedupeTime = base::Seconds(5);

const base::TimeDelta kDeviceExpiration = base::Days(10);

const base::TimeDelta kCommitTimeout = base::Seconds(3);

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
    std::unique_ptr<DataTypeStore::RecordList> record_list) {
  DCHECK(entries);
  DCHECK(entries->empty());
  DCHECK(record_list);

  for (const syncer::DataTypeStore::Record& r : *record_list) {
    auto specifics = std::make_unique<SendTabToSelfLocal>();
    if (specifics->ParseFromString(r.value)) {
      (*entries)[specifics->specifics().guid()] =
          SendTabToSelfEntry::FromLocalProto(*specifics, now);
    } else {
      return syncer::ModelError(
          FROM_HERE,
          syncer::ModelError::Type::kSendTabToSelfFailedToDeserializeSpecifics);
    }
  }

  return std::nullopt;
}

base::flat_map<std::string, base::Time> GetSessionTimestamps(
    sync_sessions::SessionSyncService* session_sync_service) {
  if (!session_sync_service) {
    return {};
  }
  sync_sessions::OpenTabsUIDelegate* delegate =
      session_sync_service->GetOpenTabsUIDelegate();
  return delegate ? delegate->GetAllForeignSessionLastModifiedTimes()
                  : base::flat_map<std::string, base::Time>();
}

struct DeviceWithTimestamp {
  raw_ptr<const syncer::DeviceInfo> device;
  base::Time last_active;
  bool has_high_precision = false;
};

// Returns a list of devices with the last active timestamp for each device.
// The last active timestamp is the maximum of the device's last updated
// timestamp and the last modified time of any session on the device.
std::vector<DeviceWithTimestamp> GetDevicesWithLastActiveTime(
    const std::vector<const syncer::DeviceInfo*>& all_devices,
    const base::flat_map<std::string, base::Time>& session_timestamps) {
  std::vector<DeviceWithTimestamp> devices_with_timestamps;
  devices_with_timestamps.reserve(all_devices.size());

  for (const syncer::DeviceInfo* device : all_devices) {
    base::Time last_active = device->last_updated_timestamp();
    bool has_high_precision = false;
    auto it = session_timestamps.find(device->guid());
    if (it != session_timestamps.end()) {
      last_active = std::max(last_active, it->second);
      // If the device has a session timestamp, it is highly precise.
      has_high_precision = true;
    }
    devices_with_timestamps.emplace_back(device, last_active,
                                         has_high_precision);
  }
  return devices_with_timestamps;
}

}  // namespace

SendTabToSelfBridge::SendTabToSelfBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    base::Clock* clock,
    syncer::OnceDataTypeStoreFactory create_store_callback,
    history::HistoryService* history_service,
    syncer::DeviceInfoTracker* device_info_tracker,
    sync_sessions::SessionSyncService* session_sync_service,
    PrefService* pref_service)
    : DataTypeSyncBridge(std::move(change_processor)),
      clock_(clock),
      history_service_(history_service),
      device_info_tracker_(device_info_tracker),
      session_sync_service_(session_sync_service),
      pref_service_(pref_service) {
  DCHECK(clock_);
  DCHECK(device_info_tracker_);
  if (history_service) {
    history_service_observation_.Observe(history_service);
  }

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

SendTabToSelfBridge::PendingCommit::PendingCommit(
    std::string guid,
    base::OnceCallback<void(SendTabToSelfResult)> callback)
    : guid(std::move(guid)), callback(std::move(callback)) {}

SendTabToSelfBridge::PendingCommit::~PendingCommit() = default;

SendTabToSelfBridge::PendingCommit::PendingCommit(PendingCommit&&) = default;

SendTabToSelfBridge::PendingCommit&
SendTabToSelfBridge::PendingCommit::operator=(PendingCommit&&) = default;

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
  std::unique_ptr<DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

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
        if (local_entry == nullptr) {
          bool needs_reupload = false;
          // If this device is the target and the entry hasn't been received
          // yet, set the received timestamp.
          if (device_info_tracker_->IsRecentLocalCacheGuid(
                  remote_entry->GetTargetDeviceSyncCacheGuid()) &&
              !remote_entry->IsReceived()) {
            remote_entry->MarkReceived(clock_->Now());
            RecordTimeSentToReceived(remote_entry->GetReceivedTime() -
                                     remote_entry->GetSharedTime());
            needs_reupload = true;
          }
          if (unknown_opened_entries_.contains(remote_entry->GetGUID())) {
            base::Time opened_time =
                unknown_opened_entries_[remote_entry->GetGUID()];
            unknown_opened_entries_.erase(remote_entry->GetGUID());
            remote_entry->MarkOpened(opened_time);
            RecordTimeSentToOpened(remote_entry->GetOpenedTime() -
                                   remote_entry->GetSharedTime());
            needs_reupload = true;
          }
          // Reupload the entry to the server so the sending device can
          // observe the acknowledgment. This is safe because it happens at
          // most once per entry (the IsReceived() guard above prevents
          // re-entry on subsequent syncs).
          if (needs_reupload) {
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

          // Write to the store *after* all mutations so the local store has
          // the up-to-date fields.
          batch->WriteData(guid,
                           remote_entry->AsLocalProto().SerializeAsString());
          std::string remote_guid = remote_entry->GetGUID();
          entries_[remote_guid] = std::move(remote_entry);
        } else {
          // Propagate timestamp fields from the remote entry.
          if (remote_entry->IsReceived() && !local_entry->IsReceived()) {
            local_entry->MarkReceived(remote_entry->GetReceivedTime());
          }
          // Update existing model if entries have been opened.
          if (remote_entry->IsOpened() && !local_entry->IsOpened()) {
            local_entry->MarkOpened(remote_entry->GetOpenedTime());
            opened.push_back(local_entry);
          }

          // Write to the store.
          batch->WriteData(guid,
                           local_entry->AsLocalProto().SerializeAsString());
        }
      }
    }
  }

  Commit(std::move(batch));

  NotifyRemoteSendTabToSelfEntryDeleted(removed);
  NotifyRemoteSendTabToSelfEntryAdded(added);
  NotifyRemoteSendTabToSelfEntryOpened(opened);

  NotifySuccessForPendingCommits();

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
    const syncer::EntityData& entity_data) const {
  return GetStorageKey(entity_data);
}

std::string SendTabToSelfBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.send_tab_to_self().guid();
}

bool SendTabToSelfBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  CHECK(entity_data.specifics.has_send_tab_to_self());
  sync_pb::SendTabToSelfSpecifics specifics =
      entity_data.specifics.send_tab_to_self();
  return !specifics.guid().empty() && GURL(specifics.url()).is_valid();
}

sync_pb::EntitySpecifics
SendTabToSelfBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  // Clears all fields by default to avoid the memory and I/O overhead of an
  // additional copy of the data.
  return sync_pb::EntitySpecifics();
}

void SendTabToSelfBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK(store_);

  store_->DeleteAllDataAndMetadata(std::move(delete_metadata_change_list),
                                   base::DoNothing());

  std::vector<std::string> all_guids = GetAllGuids();

  entries_.clear();
  mru_entry_guid_.clear();

  for (auto& [hash, pending] : pending_commits_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending.callback),
                                  SendTabToSelfResult::kFailureSyncDisabled));
  }
  pending_commits_.clear();

  NotifyRemoteSendTabToSelfEntryDeleted(all_guids);
}

void SendTabToSelfBridge::OnCommitAttemptErrors(
    const syncer::FailedCommitResponseDataList& error_response_list) {
  for (const syncer::FailedCommitResponseData& error : error_response_list) {
    auto it = pending_commits_.find(error.client_tag_hash);
    if (it != pending_commits_.end()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(it->second.callback),
                         SendTabToSelfResult::kFailureCommitAttemptError));
      pending_commits_.erase(it);
    }
  }
}

syncer::DataTypeSyncBridge::CommitAttemptFailedBehavior
SendTabToSelfBridge::OnCommitAttemptFailed(syncer::SyncCommitError error) {
  for (auto& [hash, pending] : pending_commits_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(pending.callback),
                       SendTabToSelfResult::kFailureCommitAttemptFailed));
  }
  pending_commits_.clear();
  // Even if the immediate UI notification failed, the sync engine should
  // keep trying to commit the entry in the background (e.g. if the failure was
  // due to a transient network issue).
  return CommitAttemptFailedBehavior::kShouldRetryOnNextCycle;
}

void SendTabToSelfBridge::NotifySuccessForPendingCommits() {
  base::EraseIf(pending_commits_, [this](auto& pair) {
    if (!change_processor()->IsEntityUnsynced(pair.second.guid)) {
      // If the entity is no longer unsynced, the sync engine has successfully
      // committed it to the server and received an acknowledgment. This allows
      // notifying the UI to show the success confirmation (e.g. the "Sent"
      // toast).
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(pair.second.callback),
                                    SendTabToSelfResult::kSuccess));
      return true;
    }
    return false;
  });
}

void SendTabToSelfBridge::HandleCommitTimeout(
    const syncer::ClientTagHash& client_tag_hash) {
  auto it = pending_commits_.find(client_tag_hash);
  if (it != pending_commits_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(it->second.callback),
                                  SendTabToSelfResult::kFailureCommitTimeout));
    pending_commits_.erase(it);
  }
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

const SendTabToSelfEntry* SendTabToSelfBridge::SendEntry(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid,
    const PageContext& context,
    NavigationHistory navigation_history,
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation) {
  CHECK(commit_confirmation);

  if (!change_processor()->IsTrackingMetadata()) {
    std::move(commit_confirmation)
        .Run(SendTabToSelfResult::kFailureNotTrackingMetadata);
    return nullptr;
  }

  if (!url.is_valid()) {
    std::move(commit_confirmation).Run(SendTabToSelfResult::kFailureInvalidUrl);
    return nullptr;
  }

  // In the case where the user has attempted to send an identical URL to the
  // same device within the last |kDedupeTime| we think it is likely that user
  // still has the first sent tab in progress, and so we will not attempt to
  // resend.
  base::Time shared_time = clock_->Now();
  const SendTabToSelfEntry* mru_entry = GetEntryByGUID(mru_entry_guid_);
  if (mru_entry && url == mru_entry->GetURL() &&
      target_device_cache_guid == mru_entry->GetTargetDeviceSyncCacheGuid() &&
      shared_time - mru_entry->GetSharedTime() < kDedupeTime) {
    send_tab_to_self::RecordNotificationThrottled();
    std::move(commit_confirmation).Run(SendTabToSelfResult::kSuccessThrottled);
    return mru_entry;
  }

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();

  // Assure that we don't have a guid collision.
  DCHECK_EQ(GetEntryByGUID(guid), nullptr);

  std::string trimmed_title = "";

  if (base::IsStringUTF8(title)) {
    trimmed_title = base::UTF16ToUTF8(
        base::CollapseWhitespace(base::UTF8ToUTF16(title), false));
  }

  std::unique_ptr<SendTabToSelfEntry> entry =
      std::make_unique<SendTabToSelfEntry>(
          guid, url, trimmed_title, shared_time, GetLocalFullName(),
          target_device_cache_guid, context, std::move(navigation_history));

  // The size is recorded before potential truncation (dropping) of the context
  // due to the per-entity size limit.
  RecordPageContextSize(PageContextToProto(context).ByteSizeLong());

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();
  // This entry is new. Add it to the store and model.
  std::unique_ptr<syncer::EntityData> entity_data =
      CopyToEntityData(entry->AsLocalProto().specifics());

  change_processor()->Put(guid, std::move(entity_data),
                          batch->GetMetadataChangeList());

  if (commit_confirmation) {
    syncer::ClientTagHash client_tag_hash =
        syncer::ClientTagHash::FromUnhashed(syncer::SEND_TAB_TO_SELF, guid);
    pending_commits_.emplace(
        client_tag_hash, PendingCommit{guid, std::move(commit_confirmation)});
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&SendTabToSelfBridge::HandleCommitTimeout,
                       weak_ptr_factory_.GetWeakPtr(), client_tag_hash),
        kCommitTimeout);
  }

  for (SendTabToSelfModelObserver& observer : observers_) {
    observer.EntryAddedLocally(entry.get());
  }

  const SendTabToSelfEntry* result =
      entries_.emplace(guid, std::move(entry)).first->second.get();

  batch->WriteData(guid, result->AsLocalProto().SerializeAsString());

  Commit(std::move(batch));
  mru_entry_guid_ = guid;

  return result;
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
    unknown_opened_entries_[guid] = clock_->Now();
    return;
  }

  DCHECK(change_processor()->IsTrackingMetadata());

  entry->MarkOpened(clock_->Now());

  RecordTimeSentToOpened(entry->GetOpenedTime() - entry->GetSharedTime());

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

  if (deletion_info.is_from_expiration()) {
    return;
  }

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

bool SendTabToSelfBridge::IsReady() {
  return change_processor()->IsTrackingMetadata();
}

bool SendTabToSelfBridge::HasValidTargetDevice() {
  return GetTargetDeviceInfoSortedList().size() > 0;
}

std::vector<TargetDeviceInfo>
SendTabToSelfBridge::GetTargetDeviceInfoSortedList() {
  TRACE_EVENT0("ui", "SendTabToSelfBridge::GetTargetDeviceInfoSortedList");
  if (!IsReady() || !device_info_tracker_->IsSyncing()) {
    return {};
  }

  // Pre-calculate last active timestamps for sorting and filtering.
  std::vector<DeviceWithTimestamp> devices_with_timestamps =
      GetDevicesWithLastActiveTime(device_info_tracker_->GetAllDeviceInfo(),
                                   GetSessionTimestamps(session_sync_service_));

  // Sort the devices so the most recently active devices are first.
  std::stable_sort(
      devices_with_timestamps.begin(), devices_with_timestamps.end(),
      [](const DeviceWithTimestamp& a, const DeviceWithTimestamp& b) {
        return a.last_active > b.last_active;
      });

  std::vector<const syncer::DeviceInfo*> devices;
  for (const auto& entry : devices_with_timestamps) {
    // Filter out devices that are too old or don't support the feature.
    if (clock_->Now() - entry.last_active > kDeviceExpiration) {
      break;
    }
    if (ShouldIncludeDevice(*entry.device)) {
      devices.push_back(entry.device);
    }
  }

  // Resolve display names for the filtered list. This handles de-duplication
  // by name and chooses between short/full names based on collisions.
  std::vector<syncer::DeviceInfoWithName> device_names =
      syncer::DetermineDisplayNamesAndDeduplicate(devices, GetLocalFullName());

  return base::ToVector(device_names, [&](const auto& info) {
    auto it = std::ranges::find(devices_with_timestamps, info.device,
                                &DeviceWithTimestamp::device);
    return TargetDeviceInfo(info.display_name, info.device->guid(),
                            info.device->form_factor(), it->last_active,
                            it->has_high_precision);
  });
}

// static
std::unique_ptr<syncer::DataTypeStore>
SendTabToSelfBridge::DestroyAndStealStoreForTest(
    std::unique_ptr<SendTabToSelfBridge> bridge) {
  return std::move(bridge->store_);
}

void SendTabToSelfBridge::SetLocalDeviceNameForTest(
    const std::string& local_device_name) {
  local_device_name_for_testing_ = local_device_name;
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

#if BUILDFLAG(IS_IOS)
  if (!new_local_entries.empty()) {
    pref_service_->SetString(prefs::kIOSSendTabToSelfLastReceivedTabURLPref,
                             new_local_entries.back()->GetURL().spec());
  }
#endif
}

void SendTabToSelfBridge::NotifyRemoteSendTabToSelfEntryDeleted(
    const std::vector<std::string>& guids) {
  if (guids.empty()) {
    return;
  }

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

  store_ = std::move(store);
  store_->ReadAllDataAndPreprocess(
      base::BindOnce(&ParseLocalEntriesOnBackendSequence, clock_->Now(),
                     base::Unretained(initial_entries_copy)),
      base::BindOnce(&SendTabToSelfBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(initial_entries)));
}

void SendTabToSelfBridge::OnReadAllData(
    std::unique_ptr<SendTabToSelfEntries> initial_entries,
    const std::optional<syncer::ModelError>& error) {
  DCHECK(initial_entries);

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  entries_ = std::move(*initial_entries);

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

std::string SendTabToSelfBridge::GetLocalFullName() const {
  if (local_device_name_for_testing_.has_value()) {
    return *local_device_name_for_testing_;
  }
  CHECK(change_processor()->IsTrackingMetadata());
  const syncer::DeviceInfo* local_device = device_info_tracker_->GetDeviceInfo(
      change_processor()->TrackedCacheGuid());
  CHECK(local_device, base::NotFatalUntil::M148);

  return syncer::GetDeviceDisplayNames(local_device).full_name;
}

bool SendTabToSelfBridge::ShouldIncludeDevice(
    const syncer::DeviceInfo& device) const {
  // Don't include this device if it is the local device.
  if (device_info_tracker_->IsRecentLocalCacheGuid(device.guid())) {
    return false;
  }

  DCHECK_NE(device.guid(), change_processor()->TrackedCacheGuid());

  // Don't include devices that have disabled the send tab to self receiving
  // feature.
  if (!device.send_tab_to_self_receiving_enabled()) {
    return false;
  }

  return true;
}

void SendTabToSelfBridge::DoGarbageCollection() {
  std::vector<std::string> removed_guids;

  for (const auto& it : entries_) {
    DCHECK_EQ(it.first, it.second->GetGUID());

    if (it.second->IsExpired(clock_->Now())) {
      removed_guids.push_back(it.first);
    }
  }

  if (removed_guids.empty()) {
    return;
  }

  std::unique_ptr<DataTypeStore::WriteBatch> batch = store_->CreateWriteBatch();
  for (const std::string& guid : removed_guids) {
    DeleteEntryWithBatch(guid, batch.get());
  }
  Commit(std::move(batch));
  NotifyRemoteSendTabToSelfEntryDeleted(removed_guids);
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
  unknown_opened_entries_.clear();
  mru_entry_guid_.clear();

  for (auto& [hash, pending] : pending_commits_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(pending.callback),
                                  SendTabToSelfResult::kFailureEntryRemoved));
  }
  pending_commits_.clear();

  Commit(std::move(batch));

  NotifyRemoteSendTabToSelfEntryDeleted(all_guids);
}

void SendTabToSelfBridge::EraseEntryInBatch(const std::string& guid,
                                            DataTypeStore::WriteBatch* batch) {
  if (mru_entry_guid_ == guid) {
    mru_entry_guid_.clear();
  }
  entries_.erase(guid);
  unknown_opened_entries_.erase(guid);
  batch->DeleteData(guid);

  if (auto it = pending_commits_.find(
          syncer::ClientTagHash::FromUnhashed(syncer::SEND_TAB_TO_SELF, guid));
      it != pending_commits_.end()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(it->second.callback),
                                  SendTabToSelfResult::kFailureEntryRemoved));
    pending_commits_.erase(it);
  }
}

}  // namespace send_tab_to_self
