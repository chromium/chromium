// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_profile_sync_difference_tracker.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile_sync_util.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_profile_comparator.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/model/model_error.h"

namespace autofill {

using base::Optional;
using syncer::ModelError;

AutofillProfileSyncDifferenceTracker::AutofillProfileSyncDifferenceTracker(
    AutofillTable* table)
    : table_(table) {}

AutofillProfileSyncDifferenceTracker::~AutofillProfileSyncDifferenceTracker() {}

Optional<ModelError>
AutofillProfileSyncDifferenceTracker::IncorporateRemoteProfile(
    std::unique_ptr<AutofillProfile> remote) {
  const std::string remote_storage_key =
      GetStorageKeyFromAutofillProfile(*remote);

  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }

  Optional<AutofillProfile> local_with_same_storage_key =
      ReadEntry(remote_storage_key);

  if (local_with_same_storage_key) {
    // The remote profile already exists locally with the same key. Update
    // the local entry with remote data.
    std::unique_ptr<AutofillProfile> updated =
        std::make_unique<AutofillProfile>(local_with_same_storage_key.value());
    // We ignore remote updates to a verified profile because we want to keep
    // the exact version that the user edited by hand.
    if (local_with_same_storage_key->IsVerified() && !remote->IsVerified()) {
      return base::nullopt;
    }
    updated->OverwriteDataFrom(*remote);
    // TODO(jkrcal): if |updated| deviates from |remote|, we should sync it back
    // up. The only way |updated| can differ is having some extra fields
    // compared to |remote|. Thus, this cannot lead to an infinite loop of
    // commits from two clients as each commit decreases the set of empty
    // fields. This invariant depends on the implementation of
    // OverwriteDataFrom() and thus should be enforced by a DCHECK.

    if (!updated->EqualsForSyncPurposes(*local_with_same_storage_key)) {
      // We need to write back locally new changes in this entry.
      update_to_local_.push_back(std::move(updated));
    }
    GetLocalOnlyEntries()->erase(remote_storage_key);
    return base::nullopt;
  }

  // Check if profile appears under a different storage key to be de-duplicated.
  for (const auto& pair : *GetLocalOnlyEntries()) {
    const std::string& local_storage_key = pair.first;
    const AutofillProfile& local = *pair.second;

    // Look for exact duplicates, compare only profile contents (and
    // ignore origin and language code in comparison).
    if (local.Compare(*remote) == 0) {
      // We found a duplicate, we keep the new (remote) one and delete the
      // local one.
      DVLOG(2)
          << "[AUTOFILL SYNC] The profile "
          << base::UTF16ToUTF8(local.GetRawInfo(NAME_FIRST))
          << base::UTF16ToUTF8(local.GetRawInfo(NAME_LAST))
          << " already exists with a different storage key; keep the remote key"
          << remote_storage_key << " and delete the local key "
          << local_storage_key;

      // Ensure that a verified profile can never revert back to an unverified
      // one. In such a case, take over the local origin for the new (remote)
      // entry.
      if (local.IsVerified() && !remote->IsVerified()) {
        remote->set_origin(local.origin());
        // Save a copy of the remote profile also to sync.
        save_to_sync_.push_back(std::make_unique<AutofillProfile>(*remote));
      }
      // Delete the local profile that gets replaced by |remote|.
      DeleteFromLocal(local_storage_key);
      break;
    }
  }

  add_to_local_.push_back(std::move(remote));
  return base::nullopt;
}

Optional<ModelError>
AutofillProfileSyncDifferenceTracker::IncorporateRemoteDelete(
    const std::string& storage_key) {
  DCHECK(!storage_key.empty());
  DeleteFromLocal(storage_key);
  return base::nullopt;
}

Optional<ModelError> AutofillProfileSyncDifferenceTracker::FlushToLocal(
    base::OnceClosure autofill_changes_callback) {
  for (const std::string& storage_key : delete_from_local_) {
    if (!table_->RemoveAutofillProfile(storage_key)) {
      return ModelError(FROM_HERE, "Failed deleting from WebDatabase");
    }
  }
  for (const std::unique_ptr<AutofillProfile>& entry : add_to_local_) {
    if (!table_->AddAutofillProfile(*entry)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }
  }
  for (const std::unique_ptr<AutofillProfile>& entry : update_to_local_) {
    if (!table_->UpdateAutofillProfile(*entry)) {
      return ModelError(FROM_HERE, "Failed updating WebDatabase");
    }
  }
  if (!delete_from_local_.empty() || !add_to_local_.empty() ||
      !update_to_local_.empty()) {
    std::move(autofill_changes_callback).Run();
  }
  return base::nullopt;
}

Optional<ModelError> AutofillProfileSyncDifferenceTracker::FlushToSync(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync) {
  for (std::unique_ptr<AutofillProfile>& entry : save_to_sync_) {
    profiles_to_upload_to_sync->push_back(std::move(entry));
  }
  return base::nullopt;
}

Optional<AutofillProfile> AutofillProfileSyncDifferenceTracker::ReadEntry(
    const std::string& storage_key) {
  DCHECK(GetLocalOnlyEntries());
  auto iter = GetLocalOnlyEntries()->find(storage_key);
  if (iter != GetLocalOnlyEntries()->end()) {
    return *iter->second;
  }
  return base::nullopt;
}

void AutofillProfileSyncDifferenceTracker::DeleteFromLocal(
    const std::string& storage_key) {
  DCHECK(GetLocalOnlyEntries());
  delete_from_local_.insert(storage_key);
  GetLocalOnlyEntries()->erase(storage_key);
}

std::map<std::string, std::unique_ptr<AutofillProfile>>*
AutofillProfileSyncDifferenceTracker::GetLocalOnlyEntries() {
  if (!InitializeLocalOnlyEntriesIfNeeded()) {
    return nullptr;
  }
  return &local_only_entries_;
}

bool AutofillProfileSyncDifferenceTracker::
    InitializeLocalOnlyEntriesIfNeeded() {
  if (local_only_entries_initialized_) {
    return true;
  }

  std::vector<std::unique_ptr<AutofillProfile>> entries;
  if (!table_->GetAutofillProfiles(&entries)) {
    return false;
  }

  for (std::unique_ptr<AutofillProfile>& entry : entries) {
    std::string storage_key = GetStorageKeyFromAutofillProfile(*entry);
    local_only_entries_[storage_key] = std::move(entry);
  }

  local_only_entries_initialized_ = true;
  return true;
}

AutofillProfileInitialSyncDifferenceTracker::
    AutofillProfileInitialSyncDifferenceTracker(AutofillTable* table)
    : AutofillProfileSyncDifferenceTracker(table) {}

AutofillProfileInitialSyncDifferenceTracker::
    ~AutofillProfileInitialSyncDifferenceTracker() {}

Optional<ModelError>
AutofillProfileInitialSyncDifferenceTracker::IncorporateRemoteDelete(
    const std::string& storage_key) {
  // Remote delete is not allowed in initial sync.
  NOTREACHED();
  return base::nullopt;
}

Optional<ModelError> AutofillProfileInitialSyncDifferenceTracker::FlushToSync(
    std::vector<std::unique_ptr<AutofillProfile>>* profiles_to_upload_to_sync) {
  // First, flush standard updates to sync.
  AutofillProfileSyncDifferenceTracker::FlushToSync(profiles_to_upload_to_sync);

  // For initial sync, we additionally need to upload all local only entries.
  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }
  for (auto& pair : *GetLocalOnlyEntries()) {
    std::string storage_key = pair.first;
    // No deletions coming from remote are allowed for initial sync.
    DCHECK(delete_from_local_.count(storage_key) == 0);
    profiles_to_upload_to_sync->push_back(std::move(pair.second));
  }
  return base::nullopt;
}

Optional<ModelError>
AutofillProfileInitialSyncDifferenceTracker::MergeSimilarEntriesForInitialSync(
    const std::string& app_locale) {
  if (!GetLocalOnlyEntries()) {
    return ModelError(FROM_HERE, "Failed reading from WebDatabase.");
  }

  // This merge cannot happen on the fly during IncorporateRemoteSpecifics().
  // Namely, we do not want to merge a local entry with a _similar_ remote
  // entry if anoter perfectly fitting remote entry comes later during the
  // initial sync (a remote entry fits perfectly to a given local entry if
  // it has fully equal data or even the same storage key). After all the calls
  // to IncorporateRemoteSpecifics() are over, GetLocalOnlyEntries() only
  // contains unmatched entries that can be safely merged with similar remote
  // entries.

  AutofillProfileComparator comparator(app_locale);
  // Loop over all new remote entries to find merge candidates. Using
  // non-const reference because we want to update |remote| in place if
  // needed.
  for (std::unique_ptr<AutofillProfile>& remote : add_to_local_) {
    Optional<AutofillProfile> local =
        FindMergeableLocalEntry(*remote, comparator);
    if (!local) {
      continue;
    }

    DVLOG(2)
        << "[AUTOFILL SYNC] A similar profile to "
        << base::UTF16ToUTF8(remote->GetRawInfo(NAME_FIRST))
        << base::UTF16ToUTF8(remote->GetRawInfo(NAME_LAST))
        << " already exists with a different storage key; keep the remote key"
        << GetStorageKeyFromAutofillProfile(*remote)
        << ", merge local data into it and delete the local key"
        << GetStorageKeyFromAutofillProfile(*local);

    // For similar profile pairs, the local profile is always removed and its
    // content merged (if applicable) in the profile that came from sync.
    AutofillProfile remote_before_merge = *remote;
    remote->MergeDataFrom(*local, app_locale);
    if (!remote->EqualsForSyncPurposes(remote_before_merge)) {
      // We need to sync new changes in the entry back to the server.
      save_to_sync_.push_back(std::make_unique<AutofillProfile>(*remote));
      // |remote| is updated in place within |add_to_local_| so the newest
      // merged version is stored to local.
    }

    DeleteFromLocal(GetStorageKeyFromAutofillProfile(*local));
  }

  return base::nullopt;
}

Optional<AutofillProfile>
AutofillProfileInitialSyncDifferenceTracker::FindMergeableLocalEntry(
    const AutofillProfile& remote,
    const AutofillProfileComparator& comparator) {
  DCHECK(GetLocalOnlyEntries());

  // Both the remote and the local entry need to be non-verified to be
  // mergeable.
  if (remote.IsVerified()) {
    return base::nullopt;
  }

  // Check if there is a mergeable local profile.
  for (const auto& pair : *GetLocalOnlyEntries()) {
    const AutofillProfile& local_candidate = *pair.second;
    if (!local_candidate.IsVerified() &&
        comparator.AreMergeable(local_candidate, remote)) {
      return local_candidate;
    }
  }
  return base::nullopt;
}

}  // namespace autofill
