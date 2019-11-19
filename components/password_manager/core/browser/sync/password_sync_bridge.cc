// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_sync_bridge.h"

#include <unordered_set>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/model_impl/sync_metadata_store_change_list.h"
#include "net/base/escape.h"
#include "url/gurl.h"

namespace password_manager {

namespace {

std::string ComputeClientTag(
    const sync_pb::PasswordSpecificsData& password_data) {
  return net::EscapePath(GURL(password_data.origin()).spec()) + "|" +
         net::EscapePath(password_data.username_element()) + "|" +
         net::EscapePath(password_data.username_value()) + "|" +
         net::EscapePath(password_data.password_element()) + "|" +
         net::EscapePath(password_data.signon_realm());
}

sync_pb::PasswordSpecifics SpecificsFromPassword(
    const autofill::PasswordForm& password_form) {
  sync_pb::PasswordSpecifics specifics;
  sync_pb::PasswordSpecificsData* password_data =
      specifics.mutable_client_only_encrypted_data();
  password_data->set_scheme(static_cast<int>(password_form.scheme));
  password_data->set_signon_realm(password_form.signon_realm);
  password_data->set_origin(password_form.origin.spec());
  password_data->set_action(password_form.action.spec());
  password_data->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_data->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_data->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_data->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_data->set_preferred(password_form.preferred);
  password_data->set_date_last_used(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data->set_date_created(
      password_form.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_data->set_blacklisted(password_form.blacklisted_by_user);
  password_data->set_type(static_cast<int>(password_form.type));
  password_data->set_times_used(password_form.times_used);
  password_data->set_display_name(
      base::UTF16ToUTF8(password_form.display_name));
  password_data->set_avatar_url(password_form.icon_url.spec());
  password_data->set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
  return specifics;
}

autofill::PasswordForm PasswordFromEntityChange(
    const syncer::EntityChange& entity_change,
    base::Time sync_time) {
  DCHECK(entity_change.data().specifics.has_password());
  const sync_pb::PasswordSpecificsData& password_data =
      entity_change.data().specifics.password().client_only_encrypted_data();

  autofill::PasswordForm password;
  password.scheme =
      static_cast<autofill::PasswordForm::Scheme>(password_data.scheme());
  password.signon_realm = password_data.signon_realm();
  password.origin = GURL(password_data.origin());
  password.action = GURL(password_data.action());
  password.username_element =
      base::UTF8ToUTF16(password_data.username_element());
  password.password_element =
      base::UTF8ToUTF16(password_data.password_element());
  password.username_value = base::UTF8ToUTF16(password_data.username_value());
  password.password_value = base::UTF8ToUTF16(password_data.password_value());
  password.preferred = password_data.preferred();
  if (password_data.has_date_last_used()) {
    password.date_last_used = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(password_data.date_last_used()));
  } else if (password_data.preferred()) {
    // For legacy passwords that don't have the |date_last_used| field set, we
    // should it similar to the logic in login database migration.
    password.date_last_used =
        base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(1));
  }
  password.date_created = base::Time::FromDeltaSinceWindowsEpoch(
      // Use FromDeltaSinceWindowsEpoch because create_time_us has
      // always used the Windows epoch.
      base::TimeDelta::FromMicroseconds(password_data.date_created()));
  password.blacklisted_by_user = password_data.blacklisted();
  password.type =
      static_cast<autofill::PasswordForm::Type>(password_data.type());
  password.times_used = password_data.times_used();
  password.display_name = base::UTF8ToUTF16(password_data.display_name());
  password.icon_url = GURL(password_data.avatar_url());
  password.federation_origin =
      url::Origin::Create(GURL(password_data.federation_url()));
  password.date_synced = sync_time;

  return password;
}

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const autofill::PasswordForm& form) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_password() = SpecificsFromPassword(form);
  entity_data->name = form.signon_realm;
  return entity_data;
}

int ParsePrimaryKey(const std::string& storage_key) {
  int primary_key = 0;
  bool success = base::StringToInt(storage_key, &primary_key);
  DCHECK(success)
      << "Invalid storage key. Failed to convert the storage key to "
         "an integer";
  return primary_key;
}

// Returns true iff |password_specifics| and |password_form| are equal
// memberwise.
bool AreLocalAndRemotePasswordsEqual(
    const sync_pb::PasswordSpecificsData& password_specifics,
    const autofill::PasswordForm& password_form) {
  return (
      static_cast<int>(password_form.scheme) == password_specifics.scheme() &&
      password_form.signon_realm == password_specifics.signon_realm() &&
      password_form.origin.spec() == password_specifics.origin() &&
      password_form.action.spec() == password_specifics.action() &&
      base::UTF16ToUTF8(password_form.username_element) ==
          password_specifics.username_element() &&
      base::UTF16ToUTF8(password_form.password_element) ==
          password_specifics.password_element() &&
      base::UTF16ToUTF8(password_form.username_value) ==
          password_specifics.username_value() &&
      base::UTF16ToUTF8(password_form.password_value) ==
          password_specifics.password_value() &&
      password_form.preferred == password_specifics.preferred() &&
      password_form.date_last_used ==
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(
                  password_specifics.date_last_used())) &&
      password_form.date_created ==
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(
                  password_specifics.date_created())) &&
      password_form.blacklisted_by_user == password_specifics.blacklisted() &&
      static_cast<int>(password_form.type) == password_specifics.type() &&
      password_form.times_used == password_specifics.times_used() &&
      base::UTF16ToUTF8(password_form.display_name) ==
          password_specifics.display_name() &&
      password_form.icon_url.spec() == password_specifics.avatar_url() &&
      url::Origin::Create(GURL(password_specifics.federation_url()))
              .Serialize() == password_form.federation_origin.Serialize());
}

bool ShouldRecoverPasswordsDuringMerge() {
  return !base::FeatureList::IsEnabled(features::kDeleteCorruptedPasswords);
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

  ~ScopedStoreTransaction() {
    if (!committed_) {
      store_->RollbackTransaction();
    }
  }

 private:
  PasswordStoreSync* store_;
  bool committed_;

  DISALLOW_COPY_AND_ASSIGN(ScopedStoreTransaction);
};

}  // namespace

PasswordSyncBridge::PasswordSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    PasswordStoreSync* password_store_sync,
    const base::RepeatingClosure& sync_enabled_or_disabled_cb)
    : ModelTypeSyncBridge(std::move(change_processor)),
      password_store_sync_(password_store_sync),
      sync_enabled_or_disabled_cb_(sync_enabled_or_disabled_cb) {
  DCHECK(password_store_sync_);
  DCHECK(sync_enabled_or_disabled_cb_);
  // The metadata store could be null if the login database initialization
  // fails.
  if (!password_store_sync_->GetMetadataStore()) {
    this->change_processor()->ReportError(
        {FROM_HERE, "Password metadata store isn't available."});
    return;
  }
  std::unique_ptr<syncer::MetadataBatch> batch =
      password_store_sync_->GetMetadataStore()->GetAllSyncMetadata();
  if (!batch) {
    this->change_processor()->ReportError(
        {FROM_HERE, "Failed reading passwords metadata from password store."});
    return;
  }
  this->change_processor()->ModelReadyToSync(std::move(batch));
}

PasswordSyncBridge::~PasswordSyncBridge() = default;

void PasswordSyncBridge::ActOnPasswordStoreChanges(
    const PasswordStoreChangeList& local_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It's the responsibility of the callers to call this method within the same
  // transaction as the data changes to fulfill atomic writes of data and
  // metadata constraint.

  // TODO(mamir):ActOnPasswordStoreChanges() DCHECK we are inside a
  // transaction!;

  if (!change_processor()->IsTrackingMetadata()) {
    return;  // Sync processor not yet ready, don't sync.
  }

  // ActOnPasswordStoreChanges() can be called from ApplySyncChanges(). Do
  // nothing in this case.
  if (is_processing_remote_sync_changes_) {
    return;
  }

  syncer::SyncMetadataStoreChangeList metadata_change_list(
      password_store_sync_->GetMetadataStore(), syncer::PASSWORDS);

  for (const PasswordStoreChange& change : local_changes) {
    const std::string storage_key = base::NumberToString(change.primary_key());
    switch (change.type()) {
      case PasswordStoreChange::ADD:
      case PasswordStoreChange::UPDATE: {
        change_processor()->Put(storage_key, CreateEntityData(change.form()),
                                &metadata_change_list);
        break;
      }
      case PasswordStoreChange::REMOVE: {
        change_processor()->Delete(storage_key, &metadata_change_list);
        break;
      }
    }
  }
}

std::unique_ptr<syncer::MetadataChangeList>
PasswordSyncBridge::CreateMetadataChangeList() {
  return std::make_unique<syncer::InMemoryMetadataChangeList>();
}

base::Optional<syncer::ModelError> PasswordSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  base::Optional<syncer::ModelError> error = MergeSyncDataInternal(
      std::move(metadata_change_list), std::move(entity_data));
  if (error) {
    base::UmaHistogramCounts10000(
        "Sync.DownloadedPasswordsCountWhenInitialMergeFails",
        entity_data.size());
  } else {
    sync_enabled_or_disabled_cb_.Run();
  }
  return error;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::MergeSyncDataInternal(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // This method merges the local and remote passwords based on their client
  // tags. For a form |F|, there are three cases to handle:
  // 1. |F| exists only in the local model --> |F| should be Put() in the change
  //    processor.
  // 2. |F| exists only in the remote model --> |F| should be AddLoginSync() to
  //    the local password store.
  // 3. |F| exists in both the local and the remote models --> both versions
  //    should be merged by accepting the most recently created one, and update
  //    local and remote models accordingly.

  base::AutoReset<bool> processing_changes(&is_processing_remote_sync_changes_,
                                           true);

  // Read all local passwords.
  PrimaryKeyToFormMap key_to_local_form_map;
  FormRetrievalResult read_result =
      password_store_sync_->ReadAllLogins(&key_to_local_form_map);

  if (read_result == FormRetrievalResult::kDbError) {
    metrics_util::LogPasswordSyncState(metrics_util::NOT_SYNCING_FAILED_READ);
    return syncer::ModelError(FROM_HERE,
                              "Failed to load entries from password store.");
  }
  if (read_result == FormRetrievalResult::kEncrytionServiceFailure) {
    if (!ShouldRecoverPasswordsDuringMerge()) {
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_DECRYPTION);
      return syncer::ModelError(FROM_HERE,
                                "Failed to load entries from password store. "
                                "Encryption service failure.");
    }
    base::Optional<syncer::ModelError> cleanup_result_error =
        CleanupPasswordStore();
    if (cleanup_result_error) {
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_CLEANUP);
      return cleanup_result_error;
    }
    // Clean up done successfully, try to read again.
    read_result = password_store_sync_->ReadAllLogins(&key_to_local_form_map);
    if (read_result != FormRetrievalResult::kSuccess) {
      metrics_util::LogPasswordSyncState(metrics_util::NOT_SYNCING_FAILED_READ);
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
    const base::Time time_now = base::Time::Now();
    // For any local password that doesn't exist in the remote passwords, issue
    // a change_processor()->Put(). For any local password that exists in the
    // remote passwords, both should be merged by picking the most recently
    // created version. Password comparison is done by comparing the client
    // tags. In addition, collect the client tags of local passwords.
    std::unordered_set<std::string> client_tags_of_local_passwords;
    for (const auto& pair : key_to_local_form_map) {
      const int primary_key = pair.first;
      const autofill::PasswordForm& local_password_form = *pair.second;
      std::unique_ptr<syncer::EntityData> local_form_entity_data =
          CreateEntityData(local_password_form);
      const std::string client_tag_of_local_password =
          GetClientTag(*local_form_entity_data);
      client_tags_of_local_passwords.insert(client_tag_of_local_password);

      if (client_tag_to_remote_entity_change_map.count(
              client_tag_of_local_password) == 0) {
        // Local password doesn't exist in the remote model, Put() it in the
        // processor.
        change_processor()->Put(
            /*storage_key=*/base::NumberToString(primary_key),
            std::move(local_form_entity_data), metadata_change_list.get());
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
      change_processor()->UpdateStorageKey(remote_entity_change.data(),
                                           /*storage_key=*/
                                           base::NumberToString(primary_key),
                                           metadata_change_list.get());

      if (AreLocalAndRemotePasswordsEqual(remote_password_specifics,
                                          local_password_form)) {
        // Passwords are identical, nothing else to do.
        continue;
      }

      // Passwords aren't identical, pick the most recently created one.
      if (base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(
                  remote_password_specifics.date_created())) <
          local_password_form.date_created) {
        // The local password is more recent, update the processor.
        change_processor()->Put(
            /*storage_key=*/base::NumberToString(primary_key),
            std::move(local_form_entity_data), metadata_change_list.get());
      } else {
        // The remote password is more recent, update the local model.
        UpdateLoginError update_login_error;
        PasswordStoreChangeList changes = password_store_sync_->UpdateLoginSync(
            PasswordFromEntityChange(remote_entity_change,
                                     /*sync_time=*/time_now),
            &update_login_error);
        DCHECK_LE(changes.size(), 1U);
        base::UmaHistogramEnumeration(
            "PasswordManager.MergeSyncData.UpdateLoginSyncError",
            update_login_error);
        if (changes.empty()) {
          metrics_util::LogPasswordSyncState(
              metrics_util::NOT_SYNCING_FAILED_UPDATE);
          return syncer::ModelError(
              FROM_HERE, "Failed to update an entry in the password store.");
        }
        DCHECK(changes[0].primary_key() == primary_key);
        password_store_changes.push_back(changes[0]);
      }
    }

    // At this point, we have processed all local passwords. In addition, we
    // also have processed all remote passwords that exist in the local model.
    // What's remaining is to process remote passwords that don't exist in the
    // local model.

    // For any remote password that doesn't exist in the local passwords, issue
    // a password_store_sync_->AddLoginSync() and invoke the
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

      AddLoginError add_login_error;
      PasswordStoreChangeList changes = password_store_sync_->AddLoginSync(
          PasswordFromEntityChange(*entity_change, /*sync_time=*/time_now),
          &add_login_error);
      base::UmaHistogramEnumeration(
          "PasswordManager.MergeSyncData.AddLoginSyncError", add_login_error);

      // TODO(crbug.com/939302): It's not yet clear if the DCHECK_LE below is
      // legit. However, recent crashes suggest that 2 changes are returned
      // when trying to AddLoginSync (details are in the bug). Once this is
      // resolved, we should update the call the UpdateStorageKey() if
      // necessary and remove unnecessary DCHECKs below.
      // DCHECK_LE(changes.size(), 1U);
      DCHECK_LE(changes.size(), 2U);
      if (changes.empty()) {
        DCHECK_NE(add_login_error, AddLoginError::kNone);
        metrics_util::LogPasswordSyncState(
            metrics_util::NOT_SYNCING_FAILED_ADD);
        // If the remote update is invalid, direct the processor to ignore and
        // move on.
        if (add_login_error == AddLoginError::kConstraintViolation) {
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

      change_processor()->UpdateStorageKey(
          entity_change->data(),
          /*storage_key=*/
          base::NumberToString(changes.back().primary_key()),
          metadata_change_list.get());

      password_store_changes.insert(password_store_changes.end(),
                                    changes.begin(), changes.end());
    }

    // Persist the metadata changes.
    // TODO(mamir): add some test coverage for the metadata persistence.
    syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
        password_store_sync_->GetMetadataStore(), syncer::PASSWORDS);
    // |metadata_change_list| must have been created via
    // CreateMetadataChangeList() so downcasting is safe.
    static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
        ->TransferChangesTo(&sync_metadata_store_change_list);
    base::Optional<syncer::ModelError> error =
        sync_metadata_store_change_list.TakeError();
    if (error) {
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_METADATA_PERSISTENCE);
      return error;
    }
    transaction.Commit();
  }  // End of scoped transaction.

  if (!password_store_changes.empty()) {
    // It could be the case that there are no remote passwords. In such case,
    // there would be no changes to the password store other than the sync
    // metadata changes, and no need to notify observers since they aren't
    // interested in changes to sync metadata.
    password_store_sync_->NotifyLoginsChanged(password_store_changes);
  }

  metrics_util::LogPasswordSyncState(metrics_util::SYNCING_OK);
  return base::nullopt;
}

base::Optional<syncer::ModelError> PasswordSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  base::AutoReset<bool> processing_changes(&is_processing_remote_sync_changes_,
                                           true);

  const base::Time time_now = base::Time::Now();

  // This is used to keep track of all the changes applied to the password store
  // to notify other observers of the password store.
  PasswordStoreChangeList password_store_changes;
  {
    ScopedStoreTransaction transaction(password_store_sync_);

    for (const std::unique_ptr<syncer::EntityChange>& entity_change :
         entity_changes) {
      PasswordStoreChangeList changes;
      switch (entity_change->type()) {
        case syncer::EntityChange::ACTION_ADD:
          AddLoginError add_login_error;
          changes = password_store_sync_->AddLoginSync(
              PasswordFromEntityChange(*entity_change, /*sync_time=*/time_now),
              &add_login_error);
          base::UmaHistogramEnumeration(
              "PasswordManager.ApplySyncChanges.AddLoginSyncError",
              add_login_error);
          // If the addition has been successful, inform the processor about the
          // assigned storage key. AddLoginSync() might return multiple changes
          // and the last one should be the one representing the actual addition
          // in the DB.
          if (changes.empty()) {
            DCHECK_NE(add_login_error, AddLoginError::kNone);
            metrics_util::LogApplySyncChangesState(
                metrics_util::ApplySyncChangesState::kApplyAddFailed);
            // If the remote update is invalid, direct the processor to ignore
            // and move on.
            if (add_login_error == AddLoginError::kConstraintViolation) {
              change_processor()->UntrackEntityForClientTagHash(
                  entity_change->data().client_tag_hash);
              continue;
            }
            // For all other types of error, we should stop syncing.
            return syncer::ModelError(
                FROM_HERE, "Failed to add an entry to the password store.");
          }
          // TODO(crbug.com/939302): It's not yet clear if the DCHECK_LE below
          // is legit. However, recent crashes suggest that 2 changes are
          // returned when trying to AddLoginSync (details are in the bug). Once
          // this is resolved, we should update the call the UpdateStorageKey()
          // if necessary and remove unnecessary DCHECKs below.
          // DCHECK_EQ(1U, changes.size());
          DCHECK_LE(changes.size(), 2U);
          if (changes.size() == 1) {
            DCHECK_EQ(changes[0].type(), PasswordStoreChange::ADD);
          } else {
            // There must be 2 changes.
            DCHECK_EQ(changes[0].type(), PasswordStoreChange::REMOVE);
            DCHECK_EQ(changes[1].type(), PasswordStoreChange::ADD);
          }

          change_processor()->UpdateStorageKey(
              entity_change->data(),
              /*storage_key=*/
              base::NumberToString(changes.back().primary_key()),
              metadata_change_list.get());
          break;
        case syncer::EntityChange::ACTION_UPDATE:
          // TODO(mamir): This had been added to mitigate some potential issues
          // in the login database. Once the underlying cause is verified, we
          // should remove this check.
          if (entity_change->storage_key().empty()) {
            continue;
          }
          UpdateLoginError update_login_error;
          changes = password_store_sync_->UpdateLoginSync(
              PasswordFromEntityChange(*entity_change, /*sync_time=*/time_now),
              &update_login_error);
          base::UmaHistogramEnumeration(
              "PasswordManager.ApplySyncChanges.UpdateLoginSyncError",
              update_login_error);
          // If there are no entries to update, direct the processor to ignore
          // and move on.
          if (update_login_error == UpdateLoginError::kNoUpdatedRecords) {
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
          DCHECK(changes[0].primary_key() ==
                 ParsePrimaryKey(entity_change->storage_key()));
          break;
        case syncer::EntityChange::ACTION_DELETE: {
          // TODO(mamir): This had been added to mitigate some potential issues
          // in the login database. Once the underlying cause is verified, we
          // should remove this check.
          if (entity_change->storage_key().empty()) {
            continue;
          }
          int primary_key = ParsePrimaryKey(entity_change->storage_key());
          changes =
              password_store_sync_->RemoveLoginByPrimaryKeySync(primary_key);
          if (changes.empty()) {
            metrics_util::LogApplySyncChangesState(
                metrics_util::ApplySyncChangesState::kApplyDeleteFailed);
            // TODO(mamir): Revisit this after checking UMA to decide if this
            // relaxation is crucial or not.
            continue;
          }
          DCHECK_EQ(1U, changes.size());
          DCHECK_EQ(changes[0].primary_key(), primary_key);
          break;
        }
      }
      password_store_changes.insert(password_store_changes.end(),
                                    changes.begin(), changes.end());
    }

    // Persist the metadata changes.
    // TODO(mamir): add some test coverage for the metadata persistence.
    syncer::SyncMetadataStoreChangeList sync_metadata_store_change_list(
        password_store_sync_->GetMetadataStore(), syncer::PASSWORDS);
    // |metadata_change_list| must have been created via
    // CreateMetadataChangeList() so downcasting is safe.
    static_cast<syncer::InMemoryMetadataChangeList*>(metadata_change_list.get())
        ->TransferChangesTo(&sync_metadata_store_change_list);
    base::Optional<syncer::ModelError> error =
        sync_metadata_store_change_list.TakeError();
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
    password_store_sync_->NotifyLoginsChanged(password_store_changes);
  }
  metrics_util::LogApplySyncChangesState(
      metrics_util::ApplySyncChangesState::kApplyOK);
  return base::nullopt;
}

void PasswordSyncBridge::GetData(StorageKeyList storage_keys,
                                 DataCallback callback) {
  // This method is called only when there are uncommitted changes on startup.
  // There are more efficient implementations, but since this method is rarely
  // called, simplicity is preferred over efficiency.
  PrimaryKeyToFormMap key_to_form_map;
  if (password_store_sync_->ReadAllLogins(&key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from the password store."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& storage_key : storage_keys) {
    int primary_key = ParsePrimaryKey(storage_key);
    if (key_to_form_map.count(primary_key) != 0) {
      batch->Put(storage_key, CreateEntityData(*key_to_form_map[primary_key]));
    }
  }
  std::move(callback).Run(std::move(batch));
}

void PasswordSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  PrimaryKeyToFormMap key_to_form_map;
  if (password_store_sync_->ReadAllLogins(&key_to_form_map) !=
      FormRetrievalResult::kSuccess) {
    change_processor()->ReportError(
        {FROM_HERE, "Failed to load entries from the password store."});
    return;
  }

  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& pair : key_to_form_map) {
    autofill::PasswordForm form = *pair.second;
    form.password_value = base::UTF8ToUTF16("hidden");
    batch->Put(base::NumberToString(pair.first), CreateEntityData(form));
  }
  std::move(callback).Run(std::move(batch));
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
  NOTREACHED() << "PasswordSyncBridge does not support GetStorageKey.";
  return std::string();
}

bool PasswordSyncBridge::SupportsGetStorageKey() const {
  return false;
}

void PasswordSyncBridge::ApplyStopSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  if (delete_metadata_change_list) {
    password_store_sync_->GetMetadataStore()->DeleteAllSyncMetadata();

    // If this is the account store, also delete the actual data.
    if (password_store_sync_->IsAccountStore()) {
      base::AutoReset<bool> processing_changes(
          &is_processing_remote_sync_changes_, true);

      PasswordStoreChangeList password_store_changes;
      PrimaryKeyToFormMap logins;
      FormRetrievalResult result = password_store_sync_->ReadAllLogins(&logins);
      if (result == FormRetrievalResult::kSuccess) {
        for (const auto& primary_key_and_form : logins) {
          password_store_changes.emplace_back(PasswordStoreChange::REMOVE,
                                              *primary_key_and_form.second,
                                              primary_key_and_form.first);
        }
      }
      password_store_sync_->DeleteAndRecreateDatabaseFile();
      password_store_sync_->NotifyLoginsChanged(password_store_changes);

      sync_enabled_or_disabled_cb_.Run();
    }
  }
}

// static
std::string PasswordSyncBridge::ComputeClientTagForTesting(
    const sync_pb::PasswordSpecificsData& password_data) {
  return ComputeClientTag(password_data);
}

base::Optional<syncer::ModelError> PasswordSyncBridge::CleanupPasswordStore() {
  DatabaseCleanupResult cleanup_result =
      password_store_sync_->DeleteUndecryptableLogins();
  switch (cleanup_result) {
    case DatabaseCleanupResult::kSuccess:
      break;
    case DatabaseCleanupResult::kEncryptionUnavailable:
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_DECRYPTION);
      return syncer::ModelError(
          FROM_HERE, "Failed to get encryption key during database cleanup.");
    case DatabaseCleanupResult::kItemFailure:
    case DatabaseCleanupResult::kDatabaseUnavailable:
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_CLEANUP);
      return syncer::ModelError(FROM_HERE, "Failed to cleanup database.");
  }
  return base::nullopt;
}

}  // namespace password_manager
