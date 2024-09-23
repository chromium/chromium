// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_

#include <optional>
#include <ostream>
#include <string_view>

#include "base/check.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/sync/base/data_type.h"

namespace sync_preferences {

enum class PrefSensitivity {
  // The pref is not sensitive and does not require any additional opt-ins.
  kNone,
  // The pref contains sensitive information and requires history opt-in to
  // allow syncing.
  kSensitiveRequiresHistory,
};

enum class MergeBehavior {
  // The account value wins. This is the default behavior. Any pref update is
  // applied to both - the local value as well as the account value.
  kNone,
  // For dictionary values, all entries in `account_value` and `local_value` are
  // merged recursively. In case of a conflict, the entry from `account_value`
  // wins. With a DualLayerUserPrefStore, pref updates are split between the
  // account value and the local value and only the relevant updates are applied
  // to each.
  kMergeableDict,
  // For list values, all entries of `account_value` come first, followed by all
  // entries in the `local_value`. Any repeating entry in `local_value` is
  // omitted. Any pref update overwrites both - the local value as well as the
  // account value.
  kMergeableListWithRewriteOnUpdate,
  // A custom merge logic has been implemented for this pref.
  kCustom
};

// This class represents the metadata corresponding to a syncable preference.
class SyncablePrefMetadata {
 public:
  constexpr SyncablePrefMetadata(int syncable_pref_id,
                                 syncer::DataType data_type,
                                 PrefSensitivity pref_sensitivity,
                                 MergeBehavior merge_behavior)
      : syncable_pref_id_(syncable_pref_id),
        data_type_(data_type),
        pref_sensitivity_(pref_sensitivity),
        merge_behaviour_(merge_behavior) {
    CHECK(data_type_ == syncer::PREFERENCES ||
          data_type_ == syncer::PRIORITY_PREFERENCES
#if BUILDFLAG(IS_CHROMEOS_ASH)
          || data_type_ == syncer::OS_PREFERENCES ||
          data_type_ == syncer::OS_PRIORITY_PREFERENCES
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
          )
        << "Invalid type " << data_type_
        << " for syncable pref with id=" << syncable_pref_id_;
  }

  // Returns the unique ID corresponding to the syncable preference.
  int syncable_pref_id() const { return syncable_pref_id_; }
  // Returns the data type of the pref, i.e. PREFERENCES, PRIORITY_PREFERENCES,
  // OS_PREFERENCES or OS_PRIORITY_PREFERENCES.
  syncer::DataType data_type() const { return data_type_; }

  // Returns the sensitivity of the pref. It is used to determine whether the
  // pref requires history opt-in.
  PrefSensitivity pref_sensitivity() const { return pref_sensitivity_; }

  // Returns whether the pref requires history opt-in to be synced.
  bool is_history_opt_in_required() const {
    return pref_sensitivity() == PrefSensitivity::kSensitiveRequiresHistory;
  }

  MergeBehavior merge_behavior() const { return merge_behaviour_; }

 private:
  int syncable_pref_id_;
  syncer::DataType data_type_;
  PrefSensitivity pref_sensitivity_;
  MergeBehavior merge_behaviour_;
};

// This class provides an interface to define the list of syncable
// preferences (and in the future, some additional metadata).
// PrefModelAssociatorClient uses the interface to verify if a preference is
// syncable. Platform-specific preferences should be part of individual
// implementations of this interface.
class SyncablePrefsDatabase {
 public:
  SyncablePrefsDatabase() = default;
  virtual ~SyncablePrefsDatabase() = default;
  SyncablePrefsDatabase(const SyncablePrefsDatabase&) = delete;
  SyncablePrefsDatabase& operator=(const SyncablePrefsDatabase&) = delete;

  // Returns the metadata associated to the pref and null if `pref_name` is not
  // syncable.
  virtual std::optional<SyncablePrefMetadata> GetSyncablePrefMetadata(
      std::string_view pref_name) const = 0;

  // Returns true if `pref_name` is part of the allowlist of syncable
  // preferences.
  bool IsPreferenceSyncable(std::string_view pref_name) const;

  // Return true if `pref_name` is a mergeable syncable preference.
  // Note: `pref_name` must be syncable.
  bool IsPreferenceMergeable(std::string_view pref_name) const;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
