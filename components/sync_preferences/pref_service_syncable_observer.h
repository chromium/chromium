// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_OBSERVER_H_
#define COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_OBSERVER_H_

namespace sync_preferences {

class PrefServiceSyncableObserver {
 public:
  // Invoked when PrefService::IsSyncing() changes.
  virtual void OnIsSyncingChanged() = 0;

 protected:
  virtual ~PrefServiceSyncableObserver() = default;
};

}  // namespace sync_preferences

#endif  // COMPONENTS_SYNC_PREFERENCES_PREF_SERVICE_SYNCABLE_OBSERVER_H_
