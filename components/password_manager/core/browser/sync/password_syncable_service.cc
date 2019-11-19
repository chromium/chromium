// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sync/password_syncable_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "net/base/escape.h"

namespace password_manager {

// Converts the |password| into a SyncData object.
syncer::SyncData SyncDataFromPassword(const autofill::PasswordForm& password);

// Extracts the |PasswordForm| data from sync's protobuf format.
autofill::PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password);

// Returns the unique tag that will serve as the sync identifier for the
// |password| entry.
std::string MakePasswordSyncTag(const sync_pb::PasswordSpecificsData& password);
std::string MakePasswordSyncTag(const autofill::PasswordForm& password);

namespace {

// Returns true iff |password_specifics| and |password_specifics| are equal
// memberwise.
bool AreLocalAndSyncPasswordsEqual(
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
      password_form.date_last_used.ToDeltaSinceWindowsEpoch()
              .InMicroseconds() == password_specifics.date_last_used() &&
      password_form.date_created.ToInternalValue() ==
          password_specifics.date_created() &&
      password_form.blacklisted_by_user == password_specifics.blacklisted() &&
      static_cast<int>(password_form.type) == password_specifics.type() &&
      password_form.times_used == password_specifics.times_used() &&
      base::UTF16ToUTF8(password_form.display_name) ==
          password_specifics.display_name() &&
      password_form.icon_url.spec() == password_specifics.avatar_url() &&
      url::Origin::Create(GURL(password_specifics.federation_url()))
              .Serialize() == password_form.federation_origin.Serialize());
}

syncer::SyncChange::SyncChangeType GetSyncChangeType(
    PasswordStoreChange::Type type) {
  switch (type) {
    case PasswordStoreChange::ADD:
      return syncer::SyncChange::ACTION_ADD;
    case PasswordStoreChange::UPDATE:
      return syncer::SyncChange::ACTION_UPDATE;
    case PasswordStoreChange::REMOVE:
      return syncer::SyncChange::ACTION_DELETE;
  }
  NOTREACHED();
  return syncer::SyncChange::ACTION_INVALID;
}

// Creates a PasswordForm from |specifics| and |sync_time|, appends it to
// |entries|.
void AppendPasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& specifics,
    base::Time sync_time,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* entries) {
  entries->push_back(std::make_unique<autofill::PasswordForm>(
      PasswordFromSpecifics(specifics)));
  entries->back()->date_synced = sync_time;
}

}  // namespace

struct PasswordSyncableService::SyncEntries {
  std::vector<std::unique_ptr<autofill::PasswordForm>>* EntriesForChangeType(
      syncer::SyncChange::SyncChangeType type) {
    switch (type) {
      case syncer::SyncChange::ACTION_ADD:
        return &new_entries;
      case syncer::SyncChange::ACTION_UPDATE:
        return &updated_entries;
      case syncer::SyncChange::ACTION_DELETE:
        return &deleted_entries;
      case syncer::SyncChange::ACTION_INVALID:
        return nullptr;
    }
    NOTREACHED();
    return nullptr;
  }

  // List that contains the entries that are known only to sync.
  std::vector<std::unique_ptr<autofill::PasswordForm>> new_entries;

  // List that contains the entries that are known to both sync and the local
  // database but have updates in sync. They need to be updated in the local
  // database.
  std::vector<std::unique_ptr<autofill::PasswordForm>> updated_entries;

  // The list of entries to be deleted from the local database.
  std::vector<std::unique_ptr<autofill::PasswordForm>> deleted_entries;
};

PasswordSyncableService::PasswordSyncableService(
    PasswordStoreSync* password_store)
    : password_store_(password_store), is_processing_sync_changes_(false) {}

PasswordSyncableService::~PasswordSyncableService() = default;

void PasswordSyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  // PasswordStore becomes ready upon construction.
  std::move(done).Run();
}

syncer::SyncMergeResult PasswordSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(syncer::PASSWORDS, type);
  base::AutoReset<bool> processing_changes(&is_processing_sync_changes_, true);
  syncer::SyncMergeResult merge_result(type);

  // We add all the db entries as |new_local_entries| initially. During model
  // association entries that match a sync entry will be removed and this list
  // will only contain entries that are not in sync.
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_entries;
  PasswordEntryMap new_local_entries;
  if (!ReadFromPasswordStore(&password_entries, &new_local_entries)) {
    if (!ShouldRecoverPasswordsDuringMerge()) {
      merge_result.set_error(sync_error_factory->CreateAndUploadError(
          FROM_HERE, "Failed to get passwords from store."));
      metrics_util::LogPasswordSyncState(metrics_util::NOT_SYNCING_FAILED_READ);
      return merge_result;
    }

    // On MacOS it may happen that some passwords cannot be decrypted due to
    // modification of encryption key in Keychain (https://crbug.com/730625).
    // Delete those logins from the store, they should be automatically updated
    // with Sync data.
    DatabaseCleanupResult cleanup_result =
        password_store_->DeleteUndecryptableLogins();

    if (cleanup_result == DatabaseCleanupResult::kEncryptionUnavailable) {
      merge_result.set_error(sync_error_factory->CreateAndUploadError(
          FROM_HERE, "Failed to get encryption key during database cleanup."));
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_DECRYPTION);
      return merge_result;
    }

    if (cleanup_result != DatabaseCleanupResult::kSuccess) {
      merge_result.set_error(sync_error_factory->CreateAndUploadError(
          FROM_HERE, "Failed to cleanup database."));
      metrics_util::LogPasswordSyncState(
          metrics_util::NOT_SYNCING_FAILED_CLEANUP);
      return merge_result;
    }

    // Try to read all entries again. If deletion of passwords which couldn't
    // be deleted didn't help, return an error.
    password_entries.clear();
    new_local_entries.clear();
    if (!ReadFromPasswordStore(&password_entries, &new_local_entries)) {
      merge_result.set_error(sync_error_factory->CreateAndUploadError(
          FROM_HERE, "Failed to get passwords from store."));
      metrics_util::LogPasswordSyncState(metrics_util::NOT_SYNCING_FAILED_READ);
      return merge_result;
    }
  }

  if (password_entries.size() != new_local_entries.size()) {
    merge_result.set_error(sync_error_factory->CreateAndUploadError(
        FROM_HERE,
        "There are passwords with identical sync tags in the database."));
    metrics_util::LogPasswordSyncState(
        metrics_util::NOT_SYNCING_DUPLICATE_TAGS);
    return merge_result;
  }
  merge_result.set_num_items_before_association(new_local_entries.size());

  SyncEntries sync_entries;
  // Changes from password db that need to be propagated to sync.
  syncer::SyncChangeList updated_db_entries;
  for (auto sync_iter = initial_sync_data.begin();
       sync_iter != initial_sync_data.end(); ++sync_iter) {
    CreateOrUpdateEntry(*sync_iter, &new_local_entries, &sync_entries,
                        &updated_db_entries);
  }

  for (auto it = new_local_entries.begin(); it != new_local_entries.end();
       ++it) {
    updated_db_entries.push_back(
        syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_ADD,
                           SyncDataFromPassword(*it->second)));
  }

  WriteToPasswordStore(sync_entries, /*is_merge=*/true);
  merge_result.set_error(
      sync_processor->ProcessSyncChanges(FROM_HERE, updated_db_entries));
  if (merge_result.error().IsSet()) {
    metrics_util::LogPasswordSyncState(metrics_util::NOT_SYNCING_SERVER_ERROR);
    return merge_result;
  }

  merge_result.set_num_items_after_association(
      merge_result.num_items_before_association() +
      sync_entries.new_entries.size());
  merge_result.set_num_items_added(sync_entries.new_entries.size());
  merge_result.set_num_items_modified(sync_entries.updated_entries.size());
  merge_result.set_num_items_deleted(sync_entries.deleted_entries.size());

  // Save |sync_processor_| only if the whole procedure succeeded. In case of
  // failure Sync shouldn't receive any updates from the PasswordStore.
  sync_error_factory_ = std::move(sync_error_factory);
  sync_processor_ = std::move(sync_processor);

  metrics_util::LogPasswordSyncState(metrics_util::SYNCING_OK);
  return merge_result;
}

void PasswordSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(syncer::PASSWORDS, type);

  sync_processor_.reset();
  sync_error_factory_.reset();
}

syncer::SyncDataList PasswordSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(syncer::PASSWORDS, type);
  std::vector<std::unique_ptr<autofill::PasswordForm>> password_entries;
  ReadFromPasswordStore(&password_entries, nullptr);

  syncer::SyncDataList sync_data;
  sync_data.reserve(password_entries.size());
  std::transform(password_entries.begin(), password_entries.end(),
                 std::back_inserter(sync_data),
                 [](const std::unique_ptr<autofill::PasswordForm>& form) {
                   return SyncDataFromPassword(*form);
                 });
  return sync_data;
}

syncer::SyncError PasswordSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::AutoReset<bool> processing_changes(&is_processing_sync_changes_, true);
  SyncEntries sync_entries;
  base::Time time_now = base::Time::Now();

  for (auto it = change_list.begin(); it != change_list.end(); ++it) {
    const sync_pb::EntitySpecifics& specifics = it->sync_data().GetSpecifics();
    std::vector<std::unique_ptr<autofill::PasswordForm>>* entries =
        sync_entries.EntriesForChangeType(it->change_type());
    if (!entries) {
      return sync_error_factory_->CreateAndUploadError(
          FROM_HERE, "Failed to process sync changes for passwords datatype.");
    }
    AppendPasswordFromSpecifics(
        specifics.password().client_only_encrypted_data(), time_now, entries);
  }

  WriteToPasswordStore(sync_entries, /*is_merge=*/false);
  return syncer::SyncError();
}

void PasswordSyncableService::ActOnPasswordStoreChanges(
    const PasswordStoreChangeList& local_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sync_processor_) {
    if (!flare_.is_null()) {
      flare_.Run(syncer::PASSWORDS);
      flare_.Reset();
    }
    return;
  }

  // ActOnPasswordStoreChanges() can be called from ProcessSyncChanges(). Do
  // nothing in this case.
  if (is_processing_sync_changes_)
    return;
  syncer::SyncChangeList sync_changes;
  for (auto it = local_changes.begin(); it != local_changes.end(); ++it) {
    syncer::SyncData data =
        (it->type() == PasswordStoreChange::REMOVE
             ? syncer::SyncData::CreateLocalDelete(
                   MakePasswordSyncTag(it->form()), syncer::PASSWORDS)
             : SyncDataFromPassword(it->form()));
    sync_changes.push_back(
        syncer::SyncChange(FROM_HERE, GetSyncChangeType(it->type()), data));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_changes);
}

void PasswordSyncableService::InjectStartSyncFlare(
    const syncer::SyncableService::StartSyncFlare& flare) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flare_ = flare;
}

bool PasswordSyncableService::ReadFromPasswordStore(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* password_entries,
    PasswordEntryMap* passwords_entry_map) const {
  DCHECK(password_entries);
  std::vector<std::unique_ptr<autofill::PasswordForm>> autofillable_entries;
  std::vector<std::unique_ptr<autofill::PasswordForm>> blacklist_entries;
  if (!password_store_->FillAutofillableLogins(&autofillable_entries) ||
      !password_store_->FillBlacklistLogins(&blacklist_entries)) {
    return false;
  }
  password_entries->resize(autofillable_entries.size() +
                           blacklist_entries.size());
  std::move(autofillable_entries.begin(), autofillable_entries.end(),
            password_entries->begin());
  std::move(blacklist_entries.begin(), blacklist_entries.end(),
            password_entries->begin() + autofillable_entries.size());

  if (!passwords_entry_map)
    return true;

  PasswordEntryMap& entry_map = *passwords_entry_map;
  for (const auto& form : *password_entries) {
    autofill::PasswordForm* password_form = form.get();
    entry_map[MakePasswordSyncTag(*password_form)] = password_form;
  }

  return true;
}

void PasswordSyncableService::WriteToPasswordStore(const SyncEntries& entries,
                                                   bool is_merge) {
  PasswordStoreChangeList changes;

  for (const std::unique_ptr<autofill::PasswordForm>& form :
       entries.new_entries) {
    AddLoginError add_login_error;
    PasswordStoreChangeList new_changes =
        password_store_->AddLoginSync(*form, &add_login_error);
    changes.insert(changes.end(), new_changes.begin(), new_changes.end());
    if (is_merge) {
      base::UmaHistogramEnumeration(
          "PasswordManager.MergeSyncData.AddLoginSyncError", add_login_error);
    } else {
      base::UmaHistogramEnumeration(
          "PasswordManager.ApplySyncChanges.AddLoginSyncError",
          add_login_error);
    }
  }

  for (const std::unique_ptr<autofill::PasswordForm>& form :
       entries.updated_entries) {
    UpdateLoginError update_login_error;
    PasswordStoreChangeList new_changes =
        password_store_->UpdateLoginSync(*form, &update_login_error);
    if (is_merge) {
      base::UmaHistogramEnumeration(
          "PasswordManager.MergeSyncData.UpdateLoginSyncError",
          update_login_error);
    } else {
      base::UmaHistogramEnumeration(
          "PasswordManager.ApplySyncChanges.UpdateLoginSyncError",
          update_login_error);
    }
    changes.insert(changes.end(), new_changes.begin(), new_changes.end());
  }

  for (const std::unique_ptr<autofill::PasswordForm>& form :
       entries.deleted_entries) {
    PasswordStoreChangeList new_changes =
        password_store_->RemoveLoginSync(*form);
    changes.insert(changes.end(), new_changes.begin(), new_changes.end());
  }

  // We have to notify password store observers of the change by hand since
  // we use internal password store interfaces to make changes synchronously.
  password_store_->NotifyLoginsChanged(changes);
}

// static
void PasswordSyncableService::CreateOrUpdateEntry(
    const syncer::SyncData& data,
    PasswordEntryMap* unmatched_data_from_password_db,
    SyncEntries* sync_entries,
    syncer::SyncChangeList* updated_db_entries) {
  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::PasswordSpecificsData& password_specifics(
      specifics.password().client_only_encrypted_data());
  std::string tag = MakePasswordSyncTag(password_specifics);

  // Check whether the data from sync is already in the password store.
  auto existing_local_entry_iter = unmatched_data_from_password_db->find(tag);
  base::Time time_now = base::Time::Now();
  if (existing_local_entry_iter == unmatched_data_from_password_db->end()) {
    // The sync data is not in the password store, so we need to create it in
    // the password store. Add the entry to the new_entries list.
    AppendPasswordFromSpecifics(password_specifics, time_now,
                                &sync_entries->new_entries);
  } else {
    // The entry is in password store. If the entries are not identical, then
    // the entries need to be merged.
    // If the passwords differ, take the one that was created more recently.
    const autofill::PasswordForm& password_form =
        *existing_local_entry_iter->second;
    if (!AreLocalAndSyncPasswordsEqual(password_specifics, password_form)) {
      if (base::Time::FromInternalValue(password_specifics.date_created()) <
          password_form.date_created) {
        updated_db_entries->push_back(
            syncer::SyncChange(FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
                               SyncDataFromPassword(password_form)));
      } else {
        AppendPasswordFromSpecifics(password_specifics, time_now,
                                    &sync_entries->updated_entries);
      }
    }
    // Remove the entry from the entry map to indicate a match has been found.
    // Entries that remain in the map at the end of associating all sync entries
    // will be treated as additions that need to be propagated to sync.
    unmatched_data_from_password_db->erase(existing_local_entry_iter);
  }
}

bool PasswordSyncableService::ShouldRecoverPasswordsDuringMerge() const {
  return !base::FeatureList::IsEnabled(features::kDeleteCorruptedPasswords);
}

syncer::SyncData SyncDataFromPassword(
    const autofill::PasswordForm& password_form) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
#define CopyEnumField(field) \
  password_specifics->set_##field(static_cast<int>(password_form.field))
#define CopyField(field) password_specifics->set_##field(password_form.field)
#define CopyStringField(field) \
  password_specifics->set_##field(base::UTF16ToUTF8(password_form.field))
  CopyEnumField(scheme);
  CopyField(signon_realm);
  password_specifics->set_origin(password_form.origin.spec());
  password_specifics->set_action(password_form.action.spec());
  CopyStringField(username_element);
  CopyStringField(password_element);
  CopyStringField(username_value);
  CopyStringField(password_value);
  CopyField(preferred);
  password_specifics->set_date_last_used(
      password_form.date_last_used.ToDeltaSinceWindowsEpoch().InMicroseconds());
  password_specifics->set_date_created(
      password_form.date_created.ToInternalValue());
  password_specifics->set_blacklisted(password_form.blacklisted_by_user);
  CopyEnumField(type);
  CopyField(times_used);
  CopyStringField(display_name);
  password_specifics->set_avatar_url(password_form.icon_url.spec());
  password_specifics->set_federation_url(
      password_form.federation_origin.opaque()
          ? std::string()
          : password_form.federation_origin.Serialize());
#undef CopyStringField
#undef CopyField
#undef CopyEnumField

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

autofill::PasswordForm PasswordFromSpecifics(
    const sync_pb::PasswordSpecificsData& password) {
  autofill::PasswordForm new_password;
  new_password.scheme =
      static_cast<autofill::PasswordForm::Scheme>(password.scheme());
  new_password.signon_realm = password.signon_realm();
  new_password.origin = GURL(password.origin());
  new_password.action = GURL(password.action());
  new_password.username_element =
      base::UTF8ToUTF16(password.username_element());
  new_password.password_element =
      base::UTF8ToUTF16(password.password_element());
  new_password.username_value = base::UTF8ToUTF16(password.username_value());
  new_password.password_value = base::UTF8ToUTF16(password.password_value());
  new_password.preferred = password.preferred();
  if (password.has_date_last_used()) {
    new_password.date_last_used = base::Time::FromDeltaSinceWindowsEpoch(
        base::TimeDelta::FromMicroseconds(password.date_last_used()));
  } else if (password.preferred()) {
    // For legacy passwords that don't have the |date_last_used| field set, we
    // should set it similar to the logic in login database migration.
    new_password.date_last_used =
        base::Time::FromDeltaSinceWindowsEpoch(base::TimeDelta::FromDays(1));
  }
  new_password.date_created =
      base::Time::FromInternalValue(password.date_created());
  new_password.blacklisted_by_user = password.blacklisted();
  new_password.type =
      static_cast<autofill::PasswordForm::Type>(password.type());
  new_password.times_used = password.times_used();
  new_password.display_name = base::UTF8ToUTF16(password.display_name());
  new_password.icon_url = GURL(password.avatar_url());
  new_password.federation_origin =
      url::Origin::Create(GURL(password.federation_url()));
  return new_password;
}

std::string MakePasswordSyncTag(
    const sync_pb::PasswordSpecificsData& password) {
  return MakePasswordSyncTag(PasswordFromSpecifics(password));
}

std::string MakePasswordSyncTag(const autofill::PasswordForm& password) {
  return (net::EscapePath(password.origin.spec()) + "|" +
          net::EscapePath(base::UTF16ToUTF8(password.username_element)) + "|" +
          net::EscapePath(base::UTF16ToUTF8(password.username_value)) + "|" +
          net::EscapePath(base::UTF16ToUTF8(password.password_element)) + "|" +
          net::EscapePath(password.signon_realm));
}

}  // namespace password_manager
