// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_

#include <string>

#include "base/notreached.h"
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
  // TODO(crbug.com/1422534): Mark method as pure virtual once
  // overridden in all implementations.
  virtual absl::optional<SyncablePrefMetadata> GetSyncablePrefMetadata(
      const std::string& pref_name) const;

  // Returns true if `pref_name` is part of the allowlist of syncable
  // preferences.
  // TODO(crbug.com/1422534): Unmark as virtual and use default implementation
  // once GetSyncablePrefMetadata() has been implemented by all.
  // Note: This is marked as pure virtual but still has implementation to not
  // give the false impression that the default impl is ready for use.
  virtual bool IsPreferenceSyncable(const std::string& pref_name) const = 0;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCABLE_PREFS_DATABASE_H_
