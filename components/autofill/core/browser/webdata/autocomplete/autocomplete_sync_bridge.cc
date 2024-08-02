// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_sync_bridge.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/proto/autofill_sync.pb.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/entity_data.h"

using sync_pb::AutofillSpecifics;
using syncer::ClientTagBasedDataTypeProcessor;
using syncer::DataTypeLocalChangeProcessor;
using syncer::DataTypeSyncBridge;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;
using syncer::ModelError;
using syncer::MutableDataBatch;

namespace autofill {

namespace {

const char kAutocompleteEntryNamespaceTag[] = "autofill_entry|";
const char kAutocompleteTagDelimiter[] = "|";

// Simplify checking for optional errors and returning only when present.
#undef RETURN_IF_ERROR
#define RETURN_IF_ERROR(x)                     \
  if (std::optional<ModelError> ret_val = x) { \
    return ret_val;                            \
  }

void* AutocompleteSyncBridgeUserDataKey() {
  // Use the address of a static that COMDAT folding won't ever collide
  // with something else.
  static int user_data_key = 0;
  return reinterpret_cast<void*>(&user_data_key);
}

std::string EscapeIdentifiers(const AutofillSpecifics& specifics) {
  return base::EscapePath(specifics.name()) +
         std::string(kAutocompleteTagDelimiter) +
         base::EscapePath(specifics.value());
}

std::unique_ptr<EntityData> CreateEntityData(const AutocompleteEntry& entry) {
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

std::string GetStorageKeyFromModel(const AutocompleteKey& key) {
  return BuildSerializedStorageKey(base::UTF16ToUTF8(key.name()),
                                   base::UTF16ToUTF8(key.value()));
}

AutocompleteEntry MergeEntryDates(const AutocompleteEntry& entry1,
                                  const AutocompleteEntry& entry2) {
  DCHECK(entry1.key() == entry2.key());
  return AutocompleteEntry(
      entry1.key(), std::min(entry1.date_created(), entry2.date_created()),
      std::max(entry1.date_last_used(), entry2.date_last_used()));
}

bool ParseStorageKey(const std::string& storage_key, AutocompleteKey* out_key) {
  AutofillSyncStorageKey proto;
  if (proto.ParseFromString(storage_key)) {
    *out_key = AutocompleteKey(base::UTF8ToUTF16(proto.name()),
                               base::UTF8ToUTF16((proto.value())));
    return true;
  }
  return false;
}

AutocompleteEntry CreateAutocompleteEntry(
    const AutofillSpecifics& autofill_specifics) {
  AutocompleteKey key(base::UTF8ToUTF16(autofill_specifics.name()),
                      base::UTF8ToUTF16(autofill_specifics.value()));
  const google::protobuf::RepeatedField<int64_t>& timestamps =
      autofill_specifics.usage_timestamp();
  if (timestamps.empty()) {
    return AutocompleteEntry(key, base::Time(), base::Time());
  }

  auto [date_created_iter, date_last_used_iter] =
      std::minmax_element(timestamps.begin(), timestamps.end());
  return AutocompleteEntry(key,
                           base::Time::FromInternalValue(*date_created_iter),
                           base::Time::FromInternalValue(*date_last_used_iter));
}

// This is used to respond to ApplyIncrementalSyncChanges() and
// MergeFullSyncData(). Attempts to lazily load local data, and then react to
// sync data by maintaining internal state until flush calls are made, at which
// point the applicable modification should be sent towards local and sync
// directions.
class SyncDifferenceTracker {
 public:
  explicit SyncDifferenceTracker(AutocompleteTable* table) : table_(table) {}

  SyncDifferenceTracker(const SyncDifferenceTracker&) = delete;
  SyncDifferenceTracker& operator=(const SyncDifferenceTracker&) = delete;

  std::optional<ModelError> IncorporateRemoteSpecifics(
      const std::string& storage_key,
      const AutofillSpecifics& specifics) {
    if (!specifics.has_value()) {
      // A long time ago autofill had a different format, and it's possible we
      // could encounter some of that legacy data. It is not useful to us,
      // because an autofill entry with no value will not place any text in a
      // form for the user. So drop all of these on the floor.
      DVLOG(1) << "Dropping old-style autofill profile change.";
      return std::nullopt;
    }

    const AutocompleteEntry remote = CreateAutocompleteEntry(specifics);
    DCHECK_EQ(storage_key, GetStorageKeyFromModel(remote.key()));

    std::optional<AutocompleteEntry> local;
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
          const AutocompleteEntry merged =
              MergeEntryDates(local.value(), remote);
          save_to_local_.push_back(merged);
          save_to_sync_.push_back(merged);
        }
      }
    }
    return std::nullopt;
  }

  std::optional<ModelError> IncorporateRemoteDelete(
      const std::string& storage_key) {
    AutocompleteKey key;
    if (!ParseStorageKey(storage_key, &key)) {
      return ModelError(FROM_HERE, "Failed parsing storage key.");
    }
    delete_from_local_.insert(key);
    return std::nullopt;
  }

  std::optional<ModelError> FlushToLocal(
      AutofillWebDataBackend* web_data_backend) {
    for (const AutocompleteKey& key : delete_from_local_) {
      if (!table_->RemoveFormElement(key.name(), key.value())) {
        return ModelError(FROM_HERE, "Failed deleting from WebDatabase");
      }
    }
    if (!table_->UpdateAutocompleteEntries(save_to_local_)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }

    // We do not need to NotifyOnAutofillChangedBySync() because
    // `AutocompleteHistoryManager` queries AutofillTable on demand, hence,
    // the changes are invisible to PersonalDataManager.

    return std::nullopt;
  }

  std::optional<ModelError> FlushToSync(
      bool include_local_only,
      std::unique_ptr<MetadataChangeList> metadata_change_list,
      DataTypeLocalChangeProcessor* change_processor) {
    for (const AutocompleteEntry& entry : save_to_sync_) {
      change_processor->Put(GetStorageKeyFromModel(entry.key()),
                            CreateEntityData(entry),
                            metadata_change_list.get());
    }
    if (include_local_only) {
      if (!InitializeIfNeeded()) {
        return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
      }
      for (const AutocompleteEntry& entry : unique_to_local_) {
        // This should never be true because only ApplyIncrementalSyncChanges
        // should be calling IncorporateRemoteDelete, while only
        // MergeFullSyncData should be passing in true for |include_local_only|.
        // If this requirement changes, this DCHECK can change to act as a
        // filter.
        DCHECK(delete_from_local_.find(entry.key()) ==
               delete_from_local_.end());
        change_processor->Put(GetStorageKeyFromModel(entry.key()),
                              CreateEntityData(entry),
                              metadata_change_list.get());
      }
    }
    return change_processor->GetError();
  }

 private:
  // There are three major outcomes of this method.
  // 1. An error is encountered reading from the db, false is returned.
  // 2. The entry is not found, |entry| will not be touched.
  // 3. The entry is found, |entry| will be set.
  bool ReadEntry(const AutocompleteKey& key,
                 std::optional<AutocompleteEntry>* entry) {
    if (!InitializeIfNeeded()) {
      return false;
    }
    auto iter = unique_to_local_.find(
        AutocompleteEntry(key, base::Time(), base::Time()));
    if (iter != unique_to_local_.end()) {
      *entry = *iter;
    }
    return true;
  }

  bool InitializeIfNeeded() {
    if (initialized_) {
      return true;
    }

    std::vector<AutocompleteEntry> vector;
    if (!table_->GetAllAutocompleteEntries(&vector)) {
      return false;
    }

    unique_to_local_ = AutocompleteEntrySet(vector.begin(), vector.end());
    initialized_ = true;
    return true;
  }

  const raw_ptr<AutocompleteTable> table_;

  // This class attempts to lazily load data from |table_|. This field tracks
  // if that has happened or not yet. To facilitate this, the first usage of
  // |unique_to_local_| should typically be done through ReadEntry().
  bool initialized_ = false;

  // Because we use a custom comparison function for `AutocompleteEntry` that
  // only compares `AutocompleteKey`s, this acts as a map<AutocompleteKey,
  // AutocompleteEntry>. It should not be accessed until either ReadEntry() or
  // InitializeIfNeeded() is called.  Afterwards, it will start with all the
  // local data. As sync data is encountered entries are removed from here,
  // leaving only entries that exist solely on the local client.
  struct AutocompleteEntryComparison {
    bool operator()(const AutocompleteEntry& lhs,
                    const AutocompleteEntry& rhs) const {
      return lhs.key() < rhs.key();
    }
  };
  using AutocompleteEntrySet =
      std::set<AutocompleteEntry, AutocompleteEntryComparison>;
  AutocompleteEntrySet unique_to_local_;

  std::set<AutocompleteKey> delete_from_local_;
  std::vector<AutocompleteEntry> save_to_local_;

  // Contains merged data for entries that existed on both sync and local sides
  // and need to be saved back to sync.
  std::vector<AutocompleteEntry> save_to_sync_;
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
          std::make_unique<ClientTagBasedDataTypeProcessor>(
              syncer::AUTOFILL, /*dump_stack=*/base::DoNothing())));
}

// static
DataTypeSyncBridge* AutocompleteSyncBridge::FromWebDataService(
    AutofillWebDataService* web_data_service) {
  return static_cast<AutocompleteSyncBridge*>(
      web_data_service->GetDBUserData()->GetUserData(
          AutocompleteSyncBridgeUserDataKey()));
}

AutocompleteSyncBridge::AutocompleteSyncBridge(
    AutofillWebDataBackend* backend,
    std::unique_ptr<DataTypeLocalChangeProcessor> change_processor)
    : DataTypeSyncBridge(std::move(change_processor)),
      web_data_backend_(backend) {
  DCHECK(web_data_backend_);

  scoped_observation_.Observe(web_data_backend_.get());

  LoadMetadata();
}

AutocompleteSyncBridge::~AutocompleteSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

std::unique_ptr<MetadataChangeList>
AutocompleteSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<syncer::SyncMetadataStoreChangeList>(
      GetSyncMetadataStore(), syncer::AUTOFILL,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));
}

std::optional<syncer::ModelError> AutocompleteSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SyncDifferenceTracker tracker(GetAutocompleteTable());
  for (const auto& change : entity_data) {
    DCHECK(change->data().specifics.has_autofill());
    RETURN_IF_ERROR(tracker.IncorporateRemoteSpecifics(
        change->storage_key(), change->data().specifics.autofill()));
  }

  RETURN_IF_ERROR(tracker.FlushToLocal(web_data_backend_));
  RETURN_IF_ERROR(tracker.FlushToSync(true, std::move(metadata_change_list),
                                      change_processor()));

  web_data_backend_->CommitChanges();
  return std::nullopt;
}

std::optional<ModelError> AutocompleteSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  SyncDifferenceTracker tracker(GetAutocompleteTable());
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
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
AutocompleteSyncBridge::AutocompleteSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<AutocompleteEntry> entries;
  if (!GetAutocompleteTable()->GetAllAutocompleteEntries(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return nullptr;
  }

  std::unordered_set<std::string> keys_set(storage_keys.begin(),
                                           storage_keys.end());
  auto batch = std::make_unique<MutableDataBatch>();
  for (const AutocompleteEntry& entry : entries) {
    std::string key = GetStorageKeyFromModel(entry.key());
    if (keys_set.find(key) != keys_set.end()) {
      batch->Put(key, CreateEntityData(entry));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
AutocompleteSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<AutocompleteEntry> entries;
  if (!GetAutocompleteTable()->GetAllAutocompleteEntries(&entries)) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from table."});
    return nullptr;
  }

  auto batch = std::make_unique<MutableDataBatch>();
  for (const AutocompleteEntry& entry : entries) {
    batch->Put(GetStorageKeyFromModel(entry.key()), CreateEntityData(entry));
  }
  return batch;
}

void AutocompleteSyncBridge::ActOnLocalChanges(
    const AutocompleteChangeList& changes) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::unique_ptr<MetadataChangeList> metadata_change_list =
      CreateMetadataChangeList();
  for (const auto& change : changes) {
    const std::string storage_key = GetStorageKeyFromModel(change.key());
    switch (change.type()) {
      case AutocompleteChange::ADD:
      case AutocompleteChange::UPDATE: {
        std::optional<AutocompleteEntry> entry =
            GetAutocompleteTable()->GetAutocompleteEntry(change.key().name(),
                                                         change.key().value());
        if (!entry) {
          change_processor()->ReportError(
              {FROM_HERE, "Failed reading autofill entry from WebDatabase."});
          return;
        }
        change_processor()->Put(storage_key, CreateEntityData(*entry),
                                metadata_change_list.get());
        break;
      }
      case AutocompleteChange::REMOVE: {
        change_processor()->Delete(storage_key,
                                   syncer::DeletionOrigin::Unspecified(),
                                   metadata_change_list.get());
        break;
      }
      case AutocompleteChange::EXPIRE: {
        // For expired entries, unlink and delete the sync metadata.
        // That way we are not sending tombstone updates to the sync servers.
        bool success = GetSyncMetadataStore()->ClearEntityMetadata(
            syncer::AUTOFILL, storage_key);
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
}

void AutocompleteSyncBridge::LoadMetadata() {
  if (!web_data_backend_ || !web_data_backend_->GetDatabase() ||
      !GetAutocompleteTable() || !GetSyncMetadataStore()) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load AutofillWebDatabase."});
    return;
  }

  auto batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetSyncMetadataStore()->GetAllSyncMetadata(syncer::AUTOFILL,
                                                  batch.get())) {
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

void AutocompleteSyncBridge::AutocompleteEntriesChanged(
    const AutocompleteChangeList& changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ActOnLocalChanges(changes);
}

AutocompleteTable* AutocompleteSyncBridge::GetAutocompleteTable() {
  return AutocompleteTable::FromWebDatabase(web_data_backend_->GetDatabase());
}

AutofillSyncMetadataTable* AutocompleteSyncBridge::GetSyncMetadataStore() {
  return AutofillSyncMetadataTable::FromWebDatabase(
      web_data_backend_->GetDatabase());
}

}  // namespace autofill
