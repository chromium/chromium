// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"

#include <algorithm>
#include <memory>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/client_tag_based_model_type_processor.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "net/base/escape.h"

using base::Optional;
using base::Time;
using sync_pb::AutofillSpecifics;
using syncer::ClientTagBasedModelTypeProcessor;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;
using syncer::ModelTypeChangeProcessor;
using syncer::ModelTypeSyncBridge;
using syncer::MutableDataBatch;

namespace autofill {

namespace {

const char kAutocompleteEntryNamespaceTag[] = "autofill_entry|";
const char kAutocompleteTagDelimiter[] = "|";

// Simplify checking for optional errors and returning only when present.
#define RETURN_IF_ERROR(x)                \
  if (Optional<ModelError> ret_val = x) { \
    return ret_val;                       \
  }

void* AutocompleteSyncBridgeUserDataKey() {
  // Use the address of a static that COMDAT folding won't ever collide
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

std::string EscapeIdentifiers(const AutofillSpecifics& specifics) {
  return net::EscapePath(specifics.name()) +
         std::string(kAutocompleteTagDelimiter) +
         net::EscapePath(specifics.value());
}

std::unique_ptr<EntityData> CreateEntityData(const AutofillEntry& entry) {
  auto entity_data = std::make_unique<EntityData>();
  AutofillSpecifics* autofill = entity_data->specifics.mutable_autofill();
  autofill->set_name(base::UTF16ToUTF8(entry.key().name()));
  autofill->set_value(base::UTF16ToUTF8(entry.key().value()));
  autofill->add_usage_timestamp(entry.date_created().ToInternalValue());
  if (entry.date_created() != entry.date_last_used())
    autofill->add_usage_timestamp(entry.date_last_used().ToInternalValue());
  entity_data->name = EscapeIdentifiers(*autofill);
  return entity_data;
}

std::string BuildSerializedStorageKey(const std::string& name,
                                      const std::string& value) {
  AutofillSyncStorageKey proto;
  proto.set_name(name);
  proto.set_value(value);
  return proto.SerializeAsString();
}

std::string GetStorageKeyFromModel(const AutofillKey& key) {
  return BuildSerializedStorageKey(base::UTF16ToUTF8(key.name()),
                                   base::UTF16ToUTF8(key.value()));
}

AutofillEntry MergeEntryDates(const AutofillEntry& entry1,
                              const AutofillEntry& entry2) {
  DCHECK(entry1.key() == entry2.key());
  return AutofillEntry(
      entry1.key(), std::min(entry1.date_created(), entry2.date_created()),
      std::max(entry1.date_last_used(), entry2.date_last_used()));
}

bool ParseStorageKey(const std::string& storage_key, AutofillKey* out_key) {
  AutofillSyncStorageKey proto;
  if (proto.ParseFromString(storage_key)) {
    *out_key = AutofillKey(base::UTF8ToUTF16(proto.name()),
                           base::UTF8ToUTF16((proto.value())));
    return true;
  }
  return false;
}

AutofillEntry CreateAutofillEntry(const AutofillSpecifics& autofill_specifics) {
  AutofillKey key(base::UTF8ToUTF16(autofill_specifics.name()),
                  base::UTF8ToUTF16(autofill_specifics.value()));
  Time date_created, date_last_used;
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (!timestamps.empty()) {
    auto iter_pair = std::minmax_element(timestamps.begin(), timestamps.end());
    date_created = Time::FromInternalValue(*iter_pair.first);
    date_last_used = Time::FromInternalValue(*iter_pair.second);
  }
  return AutofillEntry(key, date_created, date_last_used);
}

// This is used to respond to ApplySyncChanges() and MergeSyncData(). Attempts
// to lazily load local data, and then react to sync data by maintaining
// internal state until flush calls are made, at which point the applicable
// modification should be sent towards local and sync directions.
class SyncDifferenceTracker {
 public:
  explicit SyncDifferenceTracker(AutofillTable* table) : table_(table) {}

  Optional<ModelError> IncorporateRemoteSpecifics(
      const std::string& storage_key,
      const AutofillSpecifics& specifics) {
    if (!specifics.has_value()) {
      // A long time ago autofill had a different format, and it's possible we
      // could encounter some of that legacy data. It is not useful to us,
      // because an autofill entry with no value will not place any text in a
      // form for the user. So drop all of these on the floor.
      DVLOG(1) << "Dropping old-style autofill profile change.";
      return {};
    }

    const AutofillEntry remote = CreateAutofillEntry(specifics);
    DCHECK_EQ(storage_key, GetStorageKeyFromModel(remote.key()));

    Optional<AutofillEntry> local;
    if (!ReadEntry(remote.key(), &local))
      return ModelError(FROM_HERE, "Failed reading from WebDatabase.");

    if (!local) {
      save_to_local_.push_back(remote);
    } else {
      unique_to_local_.erase(local.value());
      if (remote != local.value()) {
        if (specifics.usage_timestamp().empty()) {
          // Skip merging if there are no timestamps. We don't want to wipe out
          // a local value of |date_created| if the remote copy is oddly formed.
          save_to_sync_.push_back(local.value());
        } else {
          const AutofillEntry merged = MergeEntryDates(local.value(), remote);
          save_to_local_.push_back(merged);
          save_to_sync_.push_back(merged);
        }
      }
    }
    return {};
  }

  Optional<ModelError> IncorporateRemoteDelete(const std::string& storage_key) {
    AutofillKey key;
    if (!ParseStorageKey(storage_key, &key)) {
      return ModelError(FROM_HERE, "Failed parsing storage key.");
    }
    delete_from_local_.insert(key);
    return {};
  }

  Optional<ModelError> FlushToLocal(AutofillWebDataBackend* web_data_backend) {
    for (const AutofillKey& key : delete_from_local_) {
      if (!table_->RemoveFormElement(key.name(), key.value())) {
        return ModelError(FROM_HERE, "Failed deleting from WebDatabase");
      }
    }
    if (!table_->UpdateAutofillEntries(save_to_local_)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }
    if (!delete_from_local_.empty() || !save_to_local_.empty()) {
      web_data_backend->NotifyOfMultipleAutofillChanges();
    }
    return {};
  }

  Optional<ModelError> FlushToSync(
      bool include_local_only,
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      ModelTypeChangeProcessor* change_processor) {
    for (const AutofillEntry& entry : save_to_sync_) {
      change_processor->Put(GetStorageKeyFromModel(entry.key()),
                            CreateEntityData(entry),
                            metadata_change_list.get());
    }
    if (include_local_only) {
      if (!InitializeIfNeeded()) {
        return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
      }
      for (const AutofillEntry& entry : unique_to_local_) {
        // This should never be true because only ApplySyncChanges should be
        // calling IncorporateRemoteDelete, while only MergeSyncData should be
        // passing in true for |include_local_only|. If this requirement
        // changes, this DCHECK can change to act as a filter.
        DCHECK(delete_from_local_.find(entry.key()) ==
               delete_from_local_.end());
        change_processor->Put(GetStorageKeyFromModel(entry.key()),
                              CreateEntityData(entry),
                              metadata_change_list.get());
      }
    }
    return static_cast<syncer::SyncMetadataStoreChangeList*>(
               metadata_change_list.get())
        ->TakeError();
  }

 private:
  // There are three major outcomes of this method.
  // 1. An error is encountered reading from the db, false is returned.
  // 2. The entry is not found, |entry| will not be touched.
  // 3. The entry is found, |entry| will be set.
  bool ReadEntry(const AutofillKey& key, Optional<AutofillEntry>* entry) {
    if (!InitializeIfNeeded()) {
      return false;
    }
    auto iter = unique_to_local_.find(AutofillEntry(key, Time(), Time()));
    if (iter != unique_to_local_.end()) {
      *entry = *iter;
    }
    return true;
  }

  bool InitializeIfNeeded() {
    if (initialized_) {
      return true;
    }

    std::vector<AutofillEntry> vector;
    if (!table_->GetAllAutofillEntries(&vector)) {
      return false;
    }

    unique_to_local_ = std::set<AutofillEntry>(vector.begin(), vector.end());
    initialized_ = true;
    return true;
  }

  AutofillTable* table_;

  // This class attempts to lazily load data from |table_|. This field tracks
  // if that has happened or not yet. To facilitate this, the first usage of
  // |unique_to_local_| should typically be done through ReadEntry().
  bool initialized_ = false;

  // Important to note that because AutofillEntry's operator < simply compares
  // contained AutofillKeys, this acts as a map<AutofillKey, AutofillEntry>.
  // Shouldn't be accessed until either ReadEntry() or InitializeIfNeeded() is
  // called, afterward it will start with all the local data. As sync data is
  // encountered entries are removed from here, leaving only entries that exist
  // solely on the local client.
  std::set<AutofillEntry> unique_to_local_;

  std::set<AutofillKey> delete_from_local_;
  std::vector<AutofillEntry> save_to_local_;

  // Contains merged data for entries that existed on both sync and local sides
  // and need to be saved back to sync.
  std::vector<AutofillEntry> save_to_sync_;

  DISALLOW_COPY_AND_ASSIGN(SyncDifferenceTracker);
};

}  // namespace

// static
void AutocompleteSyncBridge::CreateForWebDataServiceAndBackend(
    AutofillWebDataService* web_data_service,
    AutofillWebDataBackend* web_data_backend) {
  web_data_service->GetDBUserData()->SetUserData(
      AutocompleteSyncBridgeUserDataKey(),
      std::make_unique<AutocompleteSyncBridge>(
          web_data_backend,
          std::make_unique<ClientTagBasedModelTypeProcessor>(
              syncer::AUTOFILL, /*dump_stack=*/base::RepeatingClosure())));
}

// static
ModelTypeSyncBridge* AutocompleteSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutocompleteSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          AutocompleteSyncBridgeUserDataKey()));
}

AutocompleteSyncBridge::AutocompleteSyncBridge(
    AutofillWebDataBackend* backend,
    std::unique_ptr<ModelTypeChangeProcessor> change_processor)
    : ModelTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  DCHECK(web_data_backend_);

  scoped_observer_.Add(web_data_backend_);

  LoadMetadata();
}

AutocompleteSyncBridge::~AutocompleteSyncBridge() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

std::unique_ptr<MetadataChangeList>
AutocompleteSyncBridge::CreateMetadataChangeList() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetAutofillTable(), syncer::AUTOFILL);
}

Optional<syncer::ModelError> AutocompleteSyncBridge::MergeSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SyncDifferenceTracker tracker(GetAutofillTable());
  for (const auto& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill());
    RETURN_IF_ERROR(tracker.IncorporateRemoteSpecifics(
        change->storage_key(), change->data().specifics.autofill()));
  }

  RETURN_IF_ERROR(tracker.FlushToLocal(web_data_backend_));
  RETURN_IF_ERROR(tracker.FlushToSync(true, std::move(metadata_change_list),
                                change_processor()));

  web_data_backend_->CommitChanges();
  web_data_backend_->NotifyThatSyncHasStarted(syncer::AUTOFILL);
  return {};
}

Optional<ModelError> AutocompleteSyncBridge::ApplySyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK(thread_checker_.CalledOnValidThread());

  SyncDifferenceTracker tracker(GetAutofillTable());
  for (const std::unique_ptr<EntityChange>& change : entity_changes) {
    if (change->type() == EntityChange::ACTION_DELETE) {
      RETURN_IF_ERROR(tracker.IncorporateRemoteDelete(change->storage_key()));
    } else {
      DCHECK(change->data().specifics.has_autofill());
      RETURN_IF_ERROR(tracker.IncorporateRemoteSpecifics(
          change->storage_key(), change->data().specifics.autofill()));
    }
  }

  RETURN_IF_ERROR(tracker.FlushToLocal(web_data_backend_));
  RETURN_IF_ERROR(tracker.FlushToSync(false, std::move(metadata_change_list),
                                      change_processor()));

  web_data_backend_->CommitChanges();
  return {};
}

void AutocompleteSyncBridge::AutocompleteSyncBridge::GetData(
    StorageKeyList storage_keys,
    DataCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::vector<AutofillEntry> entries;
  if (!GetAutofillTable()->GetAllAutofillEntries(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  std::unordered_set<std::string> keys_set(storage_keys.begin(),
                                           storage_keys.end());
  auto batch = std::make_unique<MutableDataBatch>();
  for (const AutofillEntry& entry : entries) {
    std::string key = GetStorageKeyFromModel(entry.key());
    if (keys_set.find(key) != keys_set.end()) {
      batch->Put(key, CreateEntityData(entry));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void AutocompleteSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  std::vector<AutofillEntry> entries;
  if (!GetAutofillTable()->GetAllAutofillEntries(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const AutofillEntry& entry : entries) {
    batch->Put(GetStorageKeyFromModel(entry.key()), CreateEntityData(entry));
  }
  std::move(callback).Run(std::move(batch));
}

void AutocompleteSyncBridge::ActOnLocalChanges(
    const AutofillChangeList& changes) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  auto metadata_change_list =
      std::make_unique<syncer::SyncMetadataStoreChangeList>(GetAutofillTable(),
                                                            syncer::AUTOFILL);
  for (const auto& change : changes) {
    const std::string storage_key = GetStorageKeyFromModel(change.key());
    switch (change.type()) {
      case AutofillChange::ADD:
      case AutofillChange::UPDATE: {
        base::Time date_created, date_last_used;
        bool success = GetAutofillTable()->GetAutofillTimestamps(
            change.key().name(), change.key().value(), &date_created,
            &date_last_used);
        if (!success) {
          change_processor()->ReportError(
              {FROM_HERE, "Failed reading autofill entry from WebDatabase."});
          return;
        }

        const AutofillEntry entry(change.key(), date_created, date_last_used);
        change_processor()->Put(storage_key, CreateEntityData(entry),
                                metadata_change_list.get());
        break;
      }
      case AutofillChange::REMOVE: {
        change_processor()->Delete(storage_key, metadata_change_list.get());
        break;
      }
      case AutofillChange::EXPIRE: {
        // For expired entries, unlink and delete the sync metadata.
        // That way we are not sending tombstone updates to the sync servers.
        bool success = GetAutofillTable()->ClearSyncMetadata(syncer::AUTOFILL,
                                                             storage_key);
        if (!success) {
          change_processor()->ReportError(
              {FROM_HERE,
               "Failed to clear sync metadata for an expired autofill entry "
               "from WebDatabase."});
          return;
        }

        change_processor()->UntrackEntityForStorageKey(storage_key);
      }
    }
  }

  // We do not need to commit any local changes (written by the processor via
  // the metadata change list) because the open WebDatabase transaction is
  // committed by the AutofillWebDataService when the original local write
  // operation (that triggered this notification to the bridge) finishes.

  if (Optional<ModelError> error = metadata_change_list->TakeError())
    change_processor()->ReportError(*error);
}

void AutocompleteSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutofillTable()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAutofillTable()->GetAllSyncMetadata(syncer::AUTOFILL, batch.get())) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed reading autofill metadata from WebDatabase."});
    return;
  }
  change_processor()->ModelReadyToSync(std::move(batch));
}

std::string AutocompleteSyncBridge::GetClientTag(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill());
  // Must have the format "autofill_entry|$name|$value" where $name and $value
  // are URL escaped. This is to maintain compatibility with the previous sync
  // integration (Directory and SyncableService).
  return std::string(kAutocompleteEntryNamespaceTag) +
         EscapeIdentifiers(entity_data.specifics.autofill());
}

std::string AutocompleteSyncBridge::GetStorageKey(
    const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_autofill());
  // Marginally more space efficient than GetClientTag() by omitting the
  // kAutocompleteEntryNamespaceTag prefix and using protobuf serialization
  // instead of URL escaping for Unicode characters.
  const AutofillSpecifics specifics = entity_data.specifics.autofill();
  return BuildSerializedStorageKey(specifics.name(), specifics.value());
}

void AutocompleteSyncBridge::AutofillEntriesChanged(
    const AutofillChangeList& changes) {
  DCHECK(thread_checker_.CalledOnValidThread());
  ActOnLocalChanges(changes);
}

AutofillTable* AutocompleteSyncBridge::GetAutofillTable() const {
  return AutofillTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

}  // namespace autofill
