// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/password_store_change.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/base/data_type_histogram.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/features.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/in_memory_metadata_change_list.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model/sync_metadata_store_change_list.h"
#include "components/sync/protocol/data_type_state_helper.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

// Error values for reading sync metadata.
// Used in metrics: "PasswordManager.SyncMetadataReadError". These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class SyncMetadataReadError {
  // Success.
  kNone = 0,
  // Database not available.
  kDbNotAvailable = 1,
  // Reading failure.
  kReadFailed = 2,
  // Reading successful but cleaning initiated in order to force initial sync.
  // This will clean undecryptable passwords.
  kReadSuccessButCleared = 3,
  // Reading successful, but the base entity specifics cache contains proto
  // fields supported in the current browser version. The cache is meant to only
  // preserve unsupported fields, hence the initial sync flow is forced to
  // resolve this incosistency.
  kNewlySupportedFieldDetectedInUnsupportedFieldsCache = 4,
  // Reading successful, but the browser has been upgraded to a version that
  // supports
  // password notes for the first time. Therefore, initial sync is enforced to
  // issue a password re-download in order to obtain any potential password
  // notes on the server that has been ignored by earlier version of the
  // browser.
  kPasswordsRequireRedownloadForPotentialNotesOnTheServer = 5,
  // Reading successful, but suspicious bulk deletions were detected. To err on
  // the side of safety, drop all password sync metadata and start again.
  kPasswordsCleanupAccidentalBatchDeletions = 6,

  kMaxValue = kPasswordsCleanupAccidentalBatchDeletions,
};

std::string ComputeClientTag(
    const sync_pb::PasswordSpecificsData& password_data) {
  GURL origin(password_data.origin());

  return base::EscapePath(origin.is_valid() ? origin.spec() : "") + "|" +
         base::EscapePath(password_data.username_element()) + "|" +
         base::EscapePath(password_data.username_value()) + "|" +
         base::EscapePath(password_data.password_element()) + "|" +
         base::EscapePath(password_data.signon_realm());
}

sync_pb::PasswordSpecificsData PasswordFromEntityChange(
    const syncer::EntityChange& entity_change) {
  DCHECK(entity_change.data().specifics.has_password());
  return entity_change.data().specifics.password().client_only_encrypted_data();
}

// Returns syncer::EntityData based on given `form`.
// `base_password_data` is intended for carrying over unknown and unsupported
// fields when there is a local modification to an existing sync entity. `form`
// and `base_password_data` are combined such that all supported proto fields
// are read from `form` while unsupported field are read from
// `base_password_data`.
std::unique_ptr<syncer::EntityData> CreateEntityData(
    const PasswordForm& form,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_password() =
      SpecificsFromPassword(form, base_password_data);
  entity_data->name = form.signon_realm;
  return entity_data;
}

// Similar to the method above but acts on sync_pb::PasswordSpecificsData
// instead.
std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::PasswordSpecificsData& password_data,
    const sync_pb::PasswordSpecificsData& base_password_data) {
  return CreateEntityData(PasswordFromSpecifics(password_data),
                          base_password_data);
}

FormPrimaryKey ParsePrimaryKey(const std::string& storage_key) {
  int primary_key = 0;
  bool success = base::StringToInt(storage_key, &primary_key);
  DCHECK(success)
      << "Invalid storage key. Failed to convert the storage key to "
         "an integer";
  return FormPrimaryKey(primary_key);
}

// Returns true iff |remote_password_specifics| and |local_password_specifics|
// are equal memberwise. It doesn't compare |password_issues|.
bool AreLocalAndRemotePasswordsEqualExcludingIssues(
    const sync_pb::PasswordSpecificsData& remote_password_specifics,
    const sync_pb::PasswordSpecificsData& local_password_specifics) {
  return local_password_specifics.scheme() ==
             remote_password_specifics.scheme() &&
         local_password_specifics.signon_realm() ==
             remote_password_specifics.signon_realm() &&
         local_password_specifics.origin() ==
             remote_password_specifics.origin() &&
         local_password_specifics.action() ==
             remote_password_specifics.action() &&
         local_password_specifics.username_element() ==
             remote_password_specifics.username_element() &&
         local_password_specifics.password_element() ==
             remote_password_specifics.password_element() &&
         local_password_specifics.username_value() ==
             remote_password_specifics.username_value() &&
         local_password_specifics.password_value() ==
             remote_password_specifics.password_value() &&
         local_password_specifics.date_last_used() ==
             remote_password_specifics.date_last_used() &&
         local_password_specifics
                 .date_password_modified_windows_epoch_micros() ==
             remote_password_specifics
                 .date_password_modified_windows_epoch_micros() &&
         local_password_specifics.date_created() ==
             remote_password_specifics.date_created() &&
         local_password_specifics.blacklisted() ==
             remote_password_specifics.blacklisted() &&
         local_password_specifics.type() == remote_password_specifics.type() &&
         local_password_specifics.times_used() ==
             remote_password_specifics.times_used() &&
         local_password_specifics.display_name() ==
             remote_password_specifics.display_name() &&
         local_password_specifics.avatar_url() ==
             remote_password_specifics.avatar_url() &&
         local_password_specifics.federation_url() ==
             remote_password_specifics.federation_url() &&
         PasswordNotesFromProto(local_password_specifics.notes()) ==
             PasswordNotesFromProto(remote_password_specifics.notes());
}

// Returns true iff |remote_password_specifics| and |local_password_specifics|
// are equal memberwise.
bool AreLocalAndRemotePasswordsEqual(
    const sync_pb::PasswordSpecificsData& remote_password_specifics,
    const sync_pb::PasswordSpecificsData& local_password_specifics) {
  return AreLocalAndRemotePasswordsEqualExcludingIssues(
             remote_password_specifics, local_password_specifics) &&
         PasswordIssuesMapFromProto(remote_password_specifics) ==
             PasswordIssuesMapFromProto(local_password_specifics);
}

bool IsCredentialPhished(const sync_pb::PasswordSpecificsData& specifics) {
  return specifics.password_issues().has_phished_password_issue();
}

// Whether we should try to recover undecryptable local passwords by deleting
// the local copy, to be replaced by the remote version coming from Sync during
// merge.
bool ShouldRecoverPasswordsDuringMerge() {
  // Delete the local undecryptable copy. Launched on MacOS, Linux, Win and iOS.
  return base::FeatureList::IsEnabled(
      features::kClearUndecryptablePasswordsOnSync);
}

bool WereUndecryptablePasswordsDeleted(PasswordStoreSync* password_store_sync) {
  // This check is needed in case, someone read passwords before password
  // sync init was started.
  auto were_undecryptable_logins_deleted =
      password_store_sync->WereUndecryptableLoginsDeleted();
  if (were_undecryptable_logins_deleted.has_value()) {
    return were_undecryptable_logins_deleted.value();
  }

  // In case nothing was deleted before sync init started, try to read passwords
  // and delete undecryptable ones. If anything was deleted then resync should
  // happen.
  PrimaryKeyToPasswordSpecificsDataMap key_to_specifics_map;
  const bool read_success =
      password_store_sync->ReadAllCredentials(&key_to_specifics_map) ==
      FormRetrievalResult::kSuccess;

  if (!read_success) {
    return false;
  }
  were_undecryptable_logins_deleted =
      password_store_sync->WereUndecryptableLoginsDeleted();
  return were_undecryptable_logins_deleted.has_value() &&
         were_undecryptable_logins_deleted.value();
}

bool DoesPasswordStoreContainAccidentalBatchDeletions(
    bool is_account_store,
    const syncer::EntityMetadataMap& metadata_map) {
  // Accidental batch deletions only ever affected the account store.
  if (!is_account_store) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          syncer::kSyncPasswordCleanUpAccidentalBatchDeletions)) {
    return false;
  }

  std::vector<const sync_pb::EntityMetadata*> deleted_metadata_without_version;
  for (const auto& metadata_entry : metadata_map) {
    const auto& metadata = metadata_entry.second;
    if (metadata->is_deleted() && !metadata->has_deleted_by_version()) {
      deleted_metadata_without_version.push_back(metadata.get());
    }
  }
  std::sort(deleted_metadata_without_version.begin(),
            deleted_metadata_without_version.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs->modification_time() < rhs->modification_time();
            });

  int count_threshold =
      syncer::kSyncPasswordCleanUpAccidentalBatchDeletionsCountThreshold.Get();
  CHECK_GT(count_threshold, 0);
  base::TimeDelta time_threshold =
      syncer::kSyncPasswordCleanUpAccidentalBatchDeletionsTimeThreshold.Get();
  CHECK_GT(time_threshold, base::Milliseconds(0));

  // Finds the first window where:
  // 1) Deletions are within `time_threshold` of each other.
  // 2) At least `count_threshold` of such deletions.
  auto batch_deletions_first = deleted_metadata_without_version.begin();
  auto batch_deletions_last = deleted_metadata_without_version.begin();
  while (batch_deletions_last != deleted_metadata_without_version.end()) {
    base::TimeDelta time_delta =
        base::Milliseconds((*batch_deletions_last)->modification_time() -
                           (*batch_deletions_first)->modification_time());
    CHECK_GE(time_delta, base::Milliseconds(0));
    auto count = std::distance(batch_deletions_first, batch_deletions_last) + 1;
    CHECK_GT(count, 0);
    if (time_delta < time_threshold && count >= count_threshold) {
      return true;
    } else if (time_delta < time_threshold) {
      ++batch_deletions_last;
    } else if (batch_deletions_first !=
               deleted_metadata_without_version.end()) {
      ++batch_deletions_first;
    }
  }

  return false;
}

// A simple class for scoping a password store sync transaction. If the
// transaction hasn't been committed, it will be rolled back when it goes out of
// scope.
class ScopedStoreTransaction {
 public:
  explicit ScopedStoreTransaction(PasswordStoreSync* store) : store_(store) {
    store_->BeginTransaction();
    committed_ = false;
  }

  void Commit() {
    if (!committed_) {
      store_->CommitTransaction();
      committed_ = true;
    }
  }

  ScopedStoreTransaction(const ScopedStoreTransaction&) = delete;
  ScopedStoreTransaction& operator=(const ScopedStoreTransaction&) = delete;

  ~ScopedStoreTransaction() {
    if (!committed_) {
      store_->RollbackTransaction();
    }
  }

 private:
  const raw_ptr<PasswordStoreSync> store_;
  bool committed_;
};

}  // namespace

PasswordSyncBridge::PasswordSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::WipeModelUponSyncDisabledBehavior
        wipe_model_upon_sync_disabled_behavior)
    : DataTypeSyncBridge(std::move(change_processor)),
      wipe_model_upon_sync_disabled_behavior_(
          wipe_model_upon_sync_disabled_behavior) {}

void PasswordSyncBridge::Init(
    PasswordStoreSync* password_store_sync,
    const base::RepeatingClosure& sync_enabled_or_disabled_cb) {
  password_store_sync_ = password_store_sync;
  sync_enabled_or_disabled_cb_ = sync_enabled_or_disabled_cb;

  // The metadata store could be null if the login database initialization
  // fails.
  SyncMetadataReadError sync_metadata_read_error = SyncMetadataReadError::kNone;
  std::unique_ptr<syncer::MetadataBatch> batch;
  if (!password_store_sync_->GetMetadataStore()) {
    this->change_processor()->ReportError(
        {FROM_HERE, "Password metadata store isn't available."});
    sync_metadata_read_error = SyncMetadataReadError::kDbNotAvailable;
  } else {
    batch = password_store_sync_->GetMetadataStore()->GetAllSyncMetadata(
        syncer::PASSWORDS);
    if (!batch) {
      // If the metadata cannot be read, it's either a persistent error or force
      // initial sync has been requested. In both cases, we drop the metadata to
      // go through the initial sync flow.
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      batch = std::make_unique<syncer::MetadataBatch>();
      sync_metadata_read_error = SyncMetadataReadError::kReadFailed;
    } else if (
        base::FeatureList::IsEnabled(
            features::
                kTriggerPasswordResyncAfterDeletingUndecryptablePasswords) &&
        WereUndecryptablePasswordsDeleted(password_store_sync_)) {
      // Some locally undecryptable credentials were deleted, force initial sync
      // by dropping the sync metadata.
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      batch = std::make_unique<syncer::MetadataBatch>();
      sync_metadata_read_error = SyncMetadataReadError::kReadSuccessButCleared;
      password_store_sync_->ClearWereUndecryptableLoginsDeleted();
    } else if (SyncMetadataCacheContainsSupportedFields(
                   batch->GetAllMetadata())) {
      // Caching entity specifics is meant to preserve fields not supported in a
      // given browser version during commits to the server. If the cache
      // contains supported fields, this means that the browser was updated and
      // we should force the initial sync flow to propagate the cached data into
      // the local model.
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      batch = std::make_unique<syncer::MetadataBatch>();
      sync_metadata_read_error = SyncMetadataReadError::
          kNewlySupportedFieldDetectedInUnsupportedFieldsCache;
    } else if (syncer::IsInitialSyncDone(
                   batch->GetDataTypeState().initial_sync_state()) &&
               !batch->GetDataTypeState()
                    .notes_enabled_before_initial_sync_for_passwords()) {
      // The browser has just been upgraded to a version that supports password
      // notes. Therefore, the metadata are cleared to enforce the initial sync
      // flow and download any potential passwords notes on the server. The
      // processor takes care of setting the flag in the data type state to
      // avoid running this flow upon every start-up.
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      batch = std::make_unique<syncer::MetadataBatch>();
      sync_metadata_read_error = SyncMetadataReadError::
          kPasswordsRequireRedownloadForPotentialNotesOnTheServer;
    } else if (DoesPasswordStoreContainAccidentalBatchDeletions(
                   password_store_sync_->IsAccountStore(),
                   batch->GetAllMetadata())) {
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      batch = std::make_unique<syncer::MetadataBatch>();
      sync_metadata_read_error =
          SyncMetadataReadError::kPasswordsCleanupAccidentalBatchDeletions;
    }
  }
  base::UmaHistogramEnumeration("PasswordManager.SyncMetadataReadError2",
                                sync_metadata_read_error);

  if (wipe_model_upon_sync_disabled_behavior_ ==
          syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata &&
      (!batch || !syncer::IsInitialSyncDone(
                     batch->GetDataTypeState().initial_sync_state()))) {
    // Since the model isn't initially tracking metadata, move away from
    // kOnceIfTrackingMetadata so the behavior doesn't kick in, in case sync
    // is turned on later and back to off.
    //
    // Note that implementing this using IsInitialSyncDone(), instead of
    // invoking IsTrackingMetadata() later, is more reliable, because the
    // function cannot be trusted in ApplyDisableSyncChanges(), as it can
    // return false negatives.
    wipe_model_upon_sync_disabled_behavior_ =
        syncer::WipeModelUponSyncDisabledBehavior::kNever;
  }

  if (batch) {
    this->change_processor()->ModelReadyToSync(std::move(batch));
  }
}

PasswordSyncBridge::~PasswordSyncBridge() = default;

void PasswordSyncBridge::ActOnPasswordStoreChanges(
    const base::Location& location,
    const PasswordStoreChangeList& local_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);
  // It's the responsibility of the callers to call this method within the same
  // transaction as the data changes to fulfill atomic writes of data and
  // metadata constraint.

  // TODO(mamir):ActOnPasswordStoreChanges() DCHECK we are inside a
  // transaction!;

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // ActOnPasswordStoreChanges() can be called from
  // ApplyIncrementalSyncChanges(). Do nothing in this case.
  if (is_processing_remote_sync_changes_) {
    return;
  }

  syncer::SyncMetadataStoreChangeList metadata_change_list(
      password_store_sync_->GetMetadataStore(), syncer::PASSWORDS,
      base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                          change_processor()->GetWeakPtr()));

  for (const PasswordStoreChange& change : local_changes) {
    DCHECK(change.form().primary_key.has_value());
    const std::string storage_key =
        base::NumberToString(change.form().primary_key.value().value());
    switch (change.type()) {
      case PasswordStoreChange::ADD:
      case PasswordStoreChange::UPDATE: {
        change_processor()->Put(
            storage_key,
            CreateEntityData(
                change.form(),
                GetPossiblyTrimmedPasswordSpecificsData(storage_key)),
            &metadata_change_list);

        break;
      }
      case PasswordStoreChange::REMOVE: {
        change_processor()->Delete(
            storage_key, syncer::DeletionOrigin::FromLocation(location),
            &metadata_change_list);
        break;
      }
    }
  }
}

std::unique_ptr<syncer::MetadataChangeList>
PasswordSyncBridge::CreateMetadataChangeList() {
  // Note: This creates an InMemoryMetadataChangeList (rather than
  // SyncMetadataStoreChangeList) to ensure that metadata changes are always
  // persisted as part of a transaction.
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

std::optional<syncer::ModelError> PasswordSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);
  // This method merges the local and remote passwords based on their client
  // tags. For a form |F|, there are three cases to handle:
  // 1. |F| exists only in the local model --> |F| should be Put() in the change
  //    processor.
  // 2. |F| exists only in the remote model --> |F| should be
  //    AddCredentialSync() to the local password store.
  // 3. |F| exists in both the local and the remote models --> both versions
  //    should be merged by accepting the most recently created one, and update
  //    local and remote models accordingly.
  base::AutoReset<bool> processing_changes(&is_processing_remote_sync_changes_,
                                           true);

  // Read all local passwords.
  PrimaryKeyToPasswordSpecificsDataMap key_to_local_specifics_map;
  FormRetrievalResult read_result =
      password_store_sync_->ReadAllCredentials(&key_to_local_specifics_map);

  if (read_result == FormRetrievalResult::kDbError) {
    metrics_util::LogPasswordSyncState(
        metrics_util::PasswordSyncState::kNotSyncingFailedRead);
    return syncer::ModelError(FROM_HERE,
                              "Failed to load entries from password store.");
  }
  if (read_result == FormRetrievalResult::kEncryptionServiceFailure ||
      read_result ==
          FormRetrievalResult::kEncryptionServiceFailureWithPartialData) {
    if (!ShouldRecoverPasswordsDuringMerge()) {
      metrics_util::LogPasswordSyncState(
          metrics_util::PasswordSyncState::kNotSyncingFailedDecryption);
      return syncer::ModelError(FROM_HERE,
                                "Failed to load entries from password store. "
                                "Encryption service failure.");
    }
    std::optional<syncer::ModelError> cleanup_result_error =
        CleanupPasswordStore();
    if (cleanup_result_error) {
      return cleanup_result_error;
    }
    // Clean up done successfully, try to read again.
    read_result =
        password_store_sync_->ReadAllCredentials(&key_to_local_specifics_map);
    if (read_result != FormRetrievalResult::kSuccess) {
      metrics_util::LogPasswordSyncState(
          metrics_util::PasswordSyncState::kNotSyncingFailedRead);
      return syncer::ModelError(
          FROM_HERE,
          "Failed to load entries from password store after cleanup.");
    }
  }
  DCHECK_EQ(read_result, FormRetrievalResult::kSuccess);

  // Collect the client tags of remote passwords and the corresponding
  // EntityChange. Note that |entity_data| only contains client tag *hashes*.
  std::map<std::string, const syncer::EntityChange*>
      client_tag_to_remote_entity_change_map;
  for (const std::unique_ptr<syncer::EntityChange>& entity_change :
       entity_data) {
    client_tag_to_remote_entity_change_map[GetClientTag(
        entity_change->data())] = entity_change.get();
  }

  // This is used to keep track of all the changes applied to the password
  // store to notify other observers of the password store.
  PasswordStoreChangeList password_store_changes;
  {
    ScopedStoreTransaction transaction(password_store_sync_);
    // For any local password that doesn't exist in the remote passwords, issue
    // a change_processor()->Put(). For any local password that exists in the
    // remote passwords, both should be merged by picking the most recently
    // created version. Password comparison is done by comparing the client
    // tags. In addition, collect the client tags of local passwords.
    std::unordered_set<std::string> client_tags_of_local_passwords;
    for (const auto& [primary_key, local_password_specifics] :
         key_to_local_specifics_map) {
      const std::string storage_key = base::NumberToString(primary_key.value());
      std::unique_ptr<syncer::EntityData> local_form_entity_data =
          CreateEntityData(
              *local_password_specifics,
              GetPossiblyTrimmedPasswordSpecificsData(storage_key));

      const std::string client_tag_of_local_password =
          GetClientTag(*local_form_entity_data);
      const bool local_client_tag_is_unique =
          client_tags_of_local_passwords.insert(client_tag_of_local_password)
              .second;

      if (!local_client_tag_is_unique) {
        // The client tag should uniquely identify local passwords. If there are
        // two local passwords with the same client tag, it must be a corrupt
        // state that cannot be synced.
        continue;
      }

      if (client_tag_to_remote_entity_change_map.count(
              client_tag_of_local_password) == 0) {
        // Local password doesn't exist in the remote model, Put() it in the
        // processor.
        change_processor()->Put(storage_key, std::move(local_form_entity_data),
                                metadata_change_list.get());
        continue;
      }

      // Local password exists in the remote model as well. A merge is required.
      const syncer::EntityChange& remote_entity_change =
          *client_tag_to_remote_entity_change_map[client_tag_of_local_password];
      const sync_pb::PasswordSpecificsData& remote_password_specifics =
          remote_entity_change.data()
              .specifics.password()
              .client_only_encrypted_data();

      // First, we need to inform the processor about the storage key anyway.
      change_processor()->UpdateStorageKey(
          remote_entity_change.data(), storage_key, metadata_change_list.get());

      if (AreLocalAndRemotePasswordsEqual(remote_password_specifics,
                                          *local_password_specifics)) {
        // Passwords are identical, nothing else to do.
        continue;
      }

      // Passwords or insecure credentials aren't identical.
      if (remote_password_specifics.date_created() <
              local_password_specifics->date_created() ||
          (AreLocalAndRemotePasswordsEqualExcludingIssues(
               remote_password_specifics, *local_password_specifics) &&
           IsCredentialPhished(*local_password_specifics))) {
        // Either the local password is more recent, or they are equal but the
        // local password has been marked as phished. While all other types of
        // issues are easy to recompute (e.g. via Password Check) phished
        // entries are only found locally, so persisting them is important.
        change_processor()->Put(storage_key, std::move(local_form_entity_data),
                                metadata_change_list.get());
      } else {
        // The remote password is more recent, update the local model.
        UpdateCredentialError update_credential_error;
        const sync_pb::PasswordSpecificsData password_specifics =
            PasswordFromEntityChange(remote_entity_change);
        PasswordStoreChangeList changes =
            password_store_sync_->UpdateCredentialSync(
                password_specifics, &update_credential_error);
        DCHECK_LE(changes.size(), 1U);
        base::UmaHistogramEnumeration(
            "PasswordManager.MergeSyncData.UpdateLoginSyncError",
            update_credential_error);
        if (changes.empty()) {
          metrics_util::LogPasswordSyncState(
              metrics_util::PasswordSyncState::kNotSyncingFailedUpdate);
          return syncer::ModelError(
              FROM_HERE, "Failed to update an entry in the password store.");
        }
        DCHECK(changes[0].form().primary_key.has_value());
        DCHECK_EQ(changes[0].form().primary_key.value(), primary_key);
        password_store_changes.push_back(changes[0]);
      }
    }

    // At this point, we have processed all local passwords. In addition, we
    // also have processed all remote passwords that exist in the local model.
    // What's remaining is to process remote passwords that don't exist in the
    // local model.

    // For any remote password that doesn't exist in the local passwords, issue
    // a password_store_sync_->AddCredentialSync() and invoke the
    // change_processor()->UpdateStorageKey(). Password comparison is done by
    // comparing the client tags.
    for (const std::unique_ptr<syncer::EntityChange>& entity_change :
         entity_data) {
      const std::string client_tag_of_remote_password =
          GetClientTag(entity_change->data());
      if (client_tags_of_local_passwords.count(client_tag_of_remote_password) !=
          0) {
        // Passwords in both local and remote models have been processed
        // already.
        continue;
      }

      AddCredentialError add_credential_error;
      PasswordStoreChangeList changes = password_store_sync_->AddCredentialSync(
          PasswordFromEntityChange(*entity_change), &add_credential_error);
      base::UmaHistogramEnumeration(
          "PasswordManager.MergeSyncData.AddLoginSyncError",
          add_credential_error);

      // In almost all cases, `AddCredentialSync` should have returned exactly 1
      // change, corresponding to the new addition. There might be 0 in case of
      // a DB error, or there might be 2 in a specific edge case: A matching
      // local password (same client tag) already exists in the DB, but wasn't
      // returned by the `ReadAllCredentials` call above. This can happen if the
      // password data in un-decryptable.
      DCHECK_LE(changes.size(), 2U);
      if (changes.empty()) {
        DCHECK_NE(add_credential_error, AddCredentialError::kNone);
        metrics_util::LogPasswordSyncState(
            metrics_util::PasswordSyncState::kNotSyncingFailedAdd);
        // If the remote update is invalid, direct the processor to ignore and
        // move on.
        if (add_credential_error == AddCredentialError::kConstraintViolation) {
          change_processor()->UntrackEntityForClientTagHash(
              entity_change->data().client_tag_hash);
          continue;
        }
        // For all other types of error, we should stop syncing.
        return syncer::ModelError(
            FROM_HERE, "Failed to add an entry in the password store.");
      }

      if (changes.size() == 1) {
        DCHECK_EQ(changes[0].type(), PasswordStoreChange::ADD);
      } else {
        // There must be 2 changes.
        DCHECK_EQ(changes[0].type(), PasswordStoreChange::REMOVE);
        DCHECK_EQ(changes[1].type(), PasswordStoreChange::ADD);
      }
      DCHECK(changes.back().form().primary_key.has_value());
      change_processor()->UpdateStorageKey(
          entity_change->data(),
          /*storage_key=*/
          base::NumberToString(
              changes.back().form().primary_key.value().value()),
          metadata_change_list.get());

      password_store_changes.insert(password_store_changes.end(),
                                    changes.begin(), changes.end());
    }

    // Persist the metadata changes.
    // TODO(mamir): add some test coverage for the metadata persistence.
    syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
        password_store_sync_->GetMetadataStore(), syncer::PASSWORDS,
        base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                            change_processor()->GetWeakPtr()));
    // |metadata_change_list| must have been created via
    // CreateMetadataChangeList() so downcasting is safe.
    static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
        ->TransferChangesTo(&sync_metadata_store_change_list);
    std::optional<syncer::ModelError> error = change_processor()->GetError();
    if (error) {
      metrics_util::LogPasswordSyncState(
          metrics_util::PasswordSyncState::
              kNotSyncingFailedMetadataPersistence);
      return error;
    }
    transaction.Commit();
  }  // End of scoped transaction.

  if (!password_store_changes.empty()) {
    // It could be the case that there are no remote passwords. In such case,
    // there would be no changes to the password store other than the sync
    // metadata changes, and no need to notify observers since they aren't
    // interested in changes to sync metadata.
    password_store_sync_->NotifyCredentialsChanged(password_store_changes);
  }

  metrics_util::LogPasswordSyncState(
      metrics_util::PasswordSyncState::kSyncingOk);
  if (password_store_sync_->IsAccountStore()) {
    int password_count = base::ranges::count_if(
        entity_data,
        [](const std::unique_ptr<syncer::EntityChange>& entity_change) {
          return !entity_change->data()
                      .specifics.password()
                      .client_only_encrypted_data()
                      .blacklisted();
        });
    metrics_util::LogDownloadedPasswordsCountFromAccountStoreAfterUnlock(
        password_count);
    metrics_util::
        LogDownloadedBlocklistedEntriesCountFromAccountStoreAfterUnlock(
            entity_data.size() - password_count);
  } else {
    base::UmaHistogramCustomCounts(
        "PasswordManager.ProfileStore.TotalAccountsBeforeInitialSync",
        key_to_local_specifics_map.size(), 0, 1000, 100);
  }

  sync_enabled_or_disabled_cb_.Run();
  return std::nullopt;
}

std::optional<syncer::ModelError>
PasswordSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);
  base::AutoReset<bool> processing_changes(&is_processing_remote_sync_changes_,
                                           true);

  // This is used to keep track of all the changes applied to the password store
  // to notify other observers of the password store.
  PasswordStoreChangeList password_store_changes;
  // Whether local state of insecure credentials changed.
  {
    ScopedStoreTransaction transaction(password_store_sync_);

    for (const std::unique_ptr<syncer::EntityChange>& entity_change :
         entity_changes) {
      PasswordStoreChangeList changes;
      switch (entity_change->type()) {
        case syncer::EntityChange::ACTION_ADD:
          AddCredentialError add_credential_error;
          changes = password_store_sync_->AddCredentialSync(
              PasswordFromEntityChange(*entity_change), &add_credential_error);
          base::UmaHistogramEnumeration(
              "PasswordManager.ApplySyncChanges.AddLoginSyncError",
              add_credential_error);
          // If the addition has been successful, inform the processor about the
          // assigned storage key. AddCredentialSync() might return multiple
          // changes and the last one should be the one representing the actual
          // addition in the DB.
          if (changes.empty()) {
            DCHECK_NE(add_credential_error, AddCredentialError::kNone);
            metrics_util::LogApplySyncChangesState(
                metrics_util::ApplySyncChangesState::kApplyAddFailed);
            // If the remote update is invalid, direct the processor to ignore
            // and move on.
            if (add_credential_error ==
                AddCredentialError::kConstraintViolation) {
              change_processor()->UntrackEntityForClientTagHash(
                  entity_change->data().client_tag_hash);
              continue;
            }
            // For all other types of error, we should stop syncing.
            return syncer::ModelError(
                FROM_HERE, "Failed to add an entry to the password store.");
          }
          // TODO(crbug.com/40617060): It's not yet clear if the DCHECK_LE below
          // is legit. However, recent crashes suggest that 2 changes are
          // returned when trying to AddCredentialSync (details are in the bug).
          // Once this is resolved, we should update the call the
          // UpdateStorageKey() if necessary and remove unnecessary DCHECKs
          // below. DCHECK_EQ(1U, changes.size());
          DCHECK_LE(changes.size(), 2U);
          if (changes.size() == 1) {
            DCHECK_EQ(changes[0].type(), PasswordStoreChange::ADD);
          } else {
            // There must be 2 changes.
            DCHECK_EQ(changes[0].type(), PasswordStoreChange::REMOVE);
            DCHECK_EQ(changes[1].type(), PasswordStoreChange::ADD);
          }
          DCHECK(changes.back().form().primary_key.has_value());
          change_processor()->UpdateStorageKey(
              entity_change->data(),
              /*storage_key=*/
              base::NumberToString(
                  changes.back().form().primary_key.value().value()),
              metadata_change_list.get());
          break;
        case syncer::EntityChange::ACTION_UPDATE: {
          // TODO(mamir): This had been added to mitigate some potential issues
          // in the login database. Once the underlying cause is verified, we
          // should remove this check.
          if (entity_change->storage_key().empty()) {
            continue;
          }
          UpdateCredentialError update_credential_error;
          sync_pb::PasswordSpecificsData password_specifics =
              PasswordFromEntityChange(*entity_change);
          changes = password_store_sync_->UpdateCredentialSync(
              password_specifics, &update_credential_error);
          FormPrimaryKey primary_key =
              FormPrimaryKey(ParsePrimaryKey(entity_change->storage_key()));
          base::UmaHistogramEnumeration(
              "PasswordManager.ApplySyncChanges.UpdateLoginSyncError",
              update_credential_error);
          // If there are no entries to update, direct the processor to ignore
          // and move on.
          if (update_credential_error ==
              UpdateCredentialError::kNoUpdatedRecords) {
            change_processor()->UntrackEntityForClientTagHash(
                entity_change->data().client_tag_hash);
            continue;
          }
          if (changes.empty()) {
            metrics_util::LogApplySyncChangesState(
                metrics_util::ApplySyncChangesState::kApplyUpdateFailed);
            return syncer::ModelError(
                FROM_HERE, "Failed to update an entry in the password store.");
          }
          DCHECK_EQ(1U, changes.size());
          DCHECK(changes[0].form().primary_key.has_value());
          DCHECK(changes[0].form().primary_key.value() == primary_key);
          break;
        }
        case syncer::EntityChange::ACTION_DELETE: {
          // TODO(mamir): This had been added to mitigate some potential issues
          // in the login database. Once the underlying cause is verified, we
          // should remove this check.
          if (entity_change->storage_key().empty()) {
            continue;
          }
          FormPrimaryKey primary_key =
              ParsePrimaryKey(entity_change->storage_key());
          changes = password_store_sync_->RemoveCredentialByPrimaryKeySync(
              primary_key);
          if (changes.empty()) {
            metrics_util::LogApplySyncChangesState(
                metrics_util::ApplySyncChangesState::kApplyDeleteFailed);
            // TODO(mamir): Revisit this after checking UMA to decide if this
            // relaxation is crucial or not.
            continue;
          }
          DCHECK_EQ(1U, changes.size());
          DCHECK(changes[0].form().primary_key.has_value());
          DCHECK(changes[0].form().primary_key.value() == primary_key);
          break;
        }
      }
      password_store_changes.insert(password_store_changes.end(),
                                    changes.begin(), changes.end());
    }

    // Persist the metadata changes.
    // TODO(mamir): add some test coverage for the metadata persistence.
    syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
        password_store_sync_->GetMetadataStore(), syncer::PASSWORDS,
        base::BindRepeating(&syncer::DataTypeLocalChangeProcessor::ReportError,
                            change_processor()->GetWeakPtr()));
    // |metadata_change_list| must have been created via
    // CreateMetadataChangeList() so downcasting is safe.
    static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
        ->TransferChangesTo(&sync_metadata_store_change_list);
    std::optional<syncer::ModelError> error = change_processor()->GetError();
    if (error) {
      metrics_util::LogApplySyncChangesState(
          metrics_util::ApplySyncChangesState::kApplyMetadataChangesFailed);
      return error;
    }
    transaction.Commit();
  }  // End of scoped transaction.

  if (!password_store_changes.empty()) {
    // It could be the case that there are no password store changes, and all
    // changes are only metadata changes. In such case, no need to notify
    // observers since they aren't interested in changes to sync metadata.
    password_store_sync_->NotifyCredentialsChanged(password_store_changes);
  }
  metrics_util::LogApplySyncChangesState(
      metrics_util::ApplySyncChangesState::kApplyOK);
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch> PasswordSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);
  // This method is called only when there are uncommitted changes on startup.
  // There are more efficient implementations, but since this method is rarely
  // called, simplicity is preferred over efficiency.
  PrimaryKeyToPasswordSpecificsDataMap key_to_specifics_map;
  if (password_store_sync_->ReadAllCredentials(&key_to_specifics_map) !=
      FormRetrievalResult::kSuccess) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from the password store."});
    return nullptr;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    FormPrimaryKey primary_key = ParsePrimaryKey(storage_key);
    if (key_to_specifics_map.count(primary_key) != 0) {
      batch->Put(storage_key,
                 CreateEntityData(
                     *key_to_specifics_map[primary_key],
                     GetPossiblyTrimmedPasswordSpecificsData(storage_key)));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
PasswordSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);

  PrimaryKeyToPasswordSpecificsDataMap key_to_specifics_map;
  if (password_store_sync_->ReadAllCredentials(&key_to_specifics_map) !=
      FormRetrievalResult::kSuccess) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from the password store."});
    return nullptr;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [primary_key, specifics] : key_to_specifics_map) {
    // TODO(crbug.com/40252694): consider whether the VISIT_SECRET macro in
    // proto_visitors.h could replace this.
    specifics->set_password_value("<redacted>");
    const std::string storage_key = base::NumberToString(primary_key.value());
    for (sync_pb::PasswordSpecificsData_Notes_Note& note :
         *specifics->mutable_notes()->mutable_note()) {
      note.set_value("<redacted>");
    }
    batch->Put(
        storage_key,
        CreateEntityData(*specifics,
                         GetPossiblyTrimmedPasswordSpecificsData(storage_key)));
  }
  return batch;
}

std::string PasswordSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_password())
      << "EntityData does not have password specifics.";

  return ComputeClientTag(
      entity_data.specifics.password().client_only_encrypted_data());
}

std::string PasswordSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  NOTREACHED_IN_MIGRATION()
      << "PasswordSyncBridge does not support GetStorageKey.";
  return std::string();
}

bool PasswordSyncBridge::SupportsGetStorageKey() const {
  return false;
}

void PasswordSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);
  switch (wipe_model_upon_sync_disabled_behavior_) {
    case syncer::WipeModelUponSyncDisabledBehavior::kNever:
      CHECK(!password_store_sync_->IsAccountStore());
      // The actual model data should NOT be wiped. Only wipe the metadata.
      password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
          syncer::PASSWORDS);
      sync_enabled_or_disabled_cb_.Run();
      return;
    case syncer::WipeModelUponSyncDisabledBehavior::kOnceIfTrackingMetadata:
      CHECK(!password_store_sync_->IsAccountStore());
      // Wipe the model data this once, and flip the behavior to kNever so it
      // doesn't get wiped again.
      syncer::SyncRecordModelClearedOnceHistogram(syncer::PASSWORDS);
      wipe_model_upon_sync_disabled_behavior_ =
          syncer::WipeModelUponSyncDisabledBehavior::kNever;
      break;
    case syncer::WipeModelUponSyncDisabledBehavior::kAlways:
      CHECK(password_store_sync_->IsAccountStore());
      break;
  }

  // The data should be deleted too. So do the following:
  // 1. Collect the credentials that will be deleted.
  // 2. Collect which credentials out of those to be deleted are unsynced.
  // 3. Delete the metadata and the data.
  // 4. Notify the store about deleted credentials, to notify store observers.
  // 5. Notify the store about deleted unsynced credentials, to take care of
  //    notifying the UI and offering the user to save those credentials in the
  //    profile store.
  base::AutoReset<bool> processing_changes(&is_processing_remote_sync_changes_,
                                           true);

  PasswordStoreChangeList password_store_changes;
  std::vector<PasswordForm> unsynced_credentials_being_deleted;
  PrimaryKeyToPasswordSpecificsDataMap credentials;
  FormRetrievalResult result =
      password_store_sync_->ReadAllCredentials(&credentials);
  if (result == FormRetrievalResult::kSuccess) {
    std::set<FormPrimaryKey> unsynced_passwords_storage_keys =
        GetUnsyncedPasswordsStorageKeys();
    for (const auto& [primary_key, specifics] : credentials) {
      PasswordForm form = PasswordFromSpecifics(*specifics);
      form.primary_key = primary_key;
      form.in_store = password_manager::PasswordForm::Store::kAccountStore;
      password_store_changes.emplace_back(PasswordStoreChange::REMOVE, form);
      if (unsynced_passwords_storage_keys.count(primary_key) != 0 &&
          !form.blocked_by_user) {
        unsynced_credentials_being_deleted.push_back(std::move(form));
      }
    }
  }
  password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata(
      syncer::PASSWORDS);
  password_store_sync_->DeleteAndRecreateDatabaseFile();
  password_store_sync_->NotifyCredentialsChanged(password_store_changes);

  if (password_store_sync_->IsAccountStore()) {
    base::UmaHistogramCounts100(
        "PasswordManager.AccountStorage.UnsyncedPasswordsFoundDuringSignOut",
        unsynced_credentials_being_deleted.size());

    if (!unsynced_credentials_being_deleted.empty()) {
      password_store_sync_->NotifyUnsyncedCredentialsWillBeDeleted(
          std::move(unsynced_credentials_being_deleted));
    }
  }

  sync_enabled_or_disabled_cb_.Run();
}

sync_pb::EntitySpecifics
PasswordSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  DCHECK(entity_specifics.has_password());

  const sync_pb::PasswordSpecificsData trimmed_password_specifics_data =
      TrimPasswordSpecificsDataForCaching(
          entity_specifics.password().client_only_encrypted_data());
  if (trimmed_password_specifics_data.ByteSizeLong() == 0u) {
    return sync_pb::EntitySpecifics();
  }

  sync_pb::EntitySpecifics trimmed_entity_specifics;
  *trimmed_entity_specifics.mutable_password()
       ->mutable_client_only_encrypted_data() = trimmed_password_specifics_data;
  return trimmed_entity_specifics;
}

const sync_pb::PasswordSpecificsData&
PasswordSyncBridge::GetPossiblyTrimmedPasswordSpecificsData(
    const std::string& storage_key) {
  return change_processor()
      ->GetPossiblyTrimmedRemoteSpecifics(storage_key)
      .password()
      .client_only_encrypted_data();
}

// TODO(crbug.com/40253286): Consider moving this logic to processor.
bool PasswordSyncBridge::SyncMetadataCacheContainsSupportedFields(
    const syncer::EntityMetadataMap& metadata_map) const {
  for (const auto& metadata_entry : metadata_map) {
    // Serialize the cached specifics and parse them back to a proto. Any fields
    // that were cached as unknown and are known in the current browser version
    // should be parsed correctly.
    std::string serialized_specifics;
    metadata_entry.second->possibly_trimmed_base_specifics().SerializeToString(
        &serialized_specifics);
    sync_pb::EntitySpecifics parsed_specifics;
    parsed_specifics.ParseFromString(serialized_specifics);

    // Skip entities without a `password` field to avoid failing the
    // precondition in the `TrimAllSupportedFieldsFromRemoteSpecifics` function
    // below.
    if (!parsed_specifics.has_password()) {
      continue;
    }

    // If `parsed_specifics` contain any supported fields, they would be cleared
    // by the trimming function.
    if (parsed_specifics.ByteSizeLong() !=
        TrimAllSupportedFieldsFromRemoteSpecifics(parsed_specifics)
            .ByteSizeLong()) {
      return true;
    }
  }

  return false;
}

std::set<FormPrimaryKey> PasswordSyncBridge::GetUnsyncedPasswordsStorageKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(password_store_sync_);

  std::set<FormPrimaryKey> storage_keys;
  PasswordStoreSync::MetadataStore* metadata_store =
      password_store_sync_->GetMetadataStore();
  // The metadata store could be null if the login database initialization
  // fails.
  if (!metadata_store) {
    return storage_keys;
  }
  std::unique_ptr<syncer::MetadataBatch> batch =
      metadata_store->GetAllSyncMetadata(syncer::PASSWORDS);
  for (const auto& [storage_key, metadata] : batch->GetAllMetadata()) {
    // Ignore unsynced deletions.
    if (!metadata->is_deleted() &&
        change_processor()->IsEntityUnsynced(storage_key)) {
      storage_keys.insert(ParsePrimaryKey(storage_key));
    }
  }
  return storage_keys;
}

// static
std::string PasswordSyncBridge::ComputeClientTagForTesting(
    const sync_pb::PasswordSpecificsData& password_data) {
  return ComputeClientTag(password_data);
}

std::optional<syncer::ModelError> PasswordSyncBridge::CleanupPasswordStore() {
  DatabaseCleanupResult cleanup_result =
      password_store_sync_->DeleteUndecryptableCredentials();
  switch (cleanup_result) {
    case DatabaseCleanupResult::kSuccess:
      break;
    case DatabaseCleanupResult::kEncryptionUnavailable:
      metrics_util::LogPasswordSyncState(
          metrics_util::PasswordSyncState::kNotSyncingFailedDecryption);
      return syncer::ModelError(
          FROM_HERE, "Failed to get encryption key during database cleanup.");
    case DatabaseCleanupResult::kItemFailure:
    case DatabaseCleanupResult::kDatabaseUnavailable:
      metrics_util::LogPasswordSyncState(
          metrics_util::PasswordSyncState::kNotSyncingFailedCleanup);
      return syncer::ModelError(FROM_HERE, "Failed to cleanup database.");
  }
  return std::nullopt;
}

}  // namespace password_manager
