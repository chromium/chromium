// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_

#include <set>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;
class PrefRegistrySimple;

namespace sync_sessions {

// Visibility state of a foreign session's tabs in this profile.
enum class SessionVisibility {
  kVisible,
  kHidden,
};

// Helper class for reading and writing preferences associated with session
// sync.
class SessionSyncPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit SessionSyncPrefs(PrefService* pref_service);

  SessionSyncPrefs(const SessionSyncPrefs&) = delete;
  SessionSyncPrefs& operator=(const SessionSyncPrefs&) = delete;

  ~SessionSyncPrefs();

  // Tracks whether our local representation of which sync nodes map to what
  // tabs (belonging to the current local session) is inconsistent.  This can
  // happen if a foreign client deems our session as "stale" and decides to
  // delete it. Rather than respond by bullishly re-creating our nodes
  // immediately, which could lead to ping-pong sequences, we give the benefit
  // of the doubt and hold off until another local navigation occurs, which
  // proves that we are still relevant.
  // This is stored across restarts to avoid multiple quick restarts from
  // potentially wiping local tab data.
  bool GetLocalDataOutOfSync();
  void SetLocalDataOutOfSync(bool local_data_out_of_sync);

  // Returns the visibility state of the foreign session identified by
  // `session_tag` in this profile.
  SessionVisibility GetSessionVisibility(std::string_view session_tag) const;

  // Updates the visibility state of the foreign session identified by
  // `session_tag` in this profile. Only hidden sessions are persisted in
  // preferences; unhiding a session removes the entry to keep the preference
  // sparse.
  void SetSessionVisibility(std::string_view session_tag,
                            SessionVisibility visibility);

  // Removes hidden session entries from preferences that are not present in
  // `tracked_session_tags`.
  void GarbageCollectUnknownHiddenSessions(
      const std::set<std::string>& tracked_session_tags);

  // Allows client code to be notified when foreign session visibility
  // preferences change.
  [[nodiscard]] base::CallbackListSubscription
  SubscribeToSessionVisibilityChanged(const base::RepeatingClosure& cb);

 private:
  void NotifySessionVisibilityChanged();

  const raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  base::RepeatingClosureList session_visibility_changed_closure_list_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
