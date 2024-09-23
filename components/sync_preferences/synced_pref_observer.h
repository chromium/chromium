// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_SYNCED_PREF_OBSERVER_H_
#define COMPONENTS_SYNC_PREFERENCES_SYNCED_PREF_OBSERVER_H_

#include <string_view>

namespace sync_preferences {

class SyncedPrefObserver {
 public:
  virtual void OnSyncedPrefChanged(std::string_view path, bool from_sync) {}

  // Invoked if the pref path is listed in the init sync list. This is called in
  // these situations:
  // 1) once after you enable Chrome Sync, and then after you disable Chrome
  // Sync and enable it again.
  // 2) every time you open a profile and sync is enabled.
  virtual void OnStartedSyncing(std::string_view path) {}
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_SYNCED_PREF_OBSERVER_H_
