// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_

#include <string>

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sync_preferences {

struct SyncablePrefMetadata {
  // A unique ID corresponding to each syncable preference.
  int syncable_pref_id_;
};

// This class provides an interface to define the list of syncable
// preferences (and in the future, some additional metadata).
// PrefModelAssociatorClient uses the interface to verify if a preference is
// syncable. Platform-specific preferences should be part of individual
// implementations of this interface.
// TODO(crbug.com/1401271): Consider adding more information about the listed
// preferences, for eg. distinguishing between SYNCABLE_PREF,
// SYNCABLE_PRIORITY_PREF, SYNCABLE_OS_PREF, and SYNCABLE_OS_PRIORITY_PREF.
class SyncablePrefsDatabase {
 public:
  SyncablePrefsDatabase() = default;
  virtual ~SyncablePrefsDatabase() = default;
  SyncablePrefsDatabase(const SyncablePrefsDatabase&) = delete;
  SyncablePrefsDatabase& operator=(const SyncablePrefsDatabase&) = delete;

  // Returns the metadata associated to the pref and null if `pref_name` is not
  // syncable.
  virtual absl::optional<SyncablePrefMetadata> GetSyncablePrefMetadata(
      const std::string& pref_name) const = 0;

  // Returns true if `pref_name` is part of the allowlist of syncable
  // preferences.
  bool IsPreferenceSyncable(const std::string& pref_name) const;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
