// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_

#include <string>

#include "base/memory/raw_ptr.h"

class PrefService;
class PrefRegistrySimple;

namespace sync_sessions {

// Use this for the unique machine tag used for session sync.
class SessionSyncPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit SessionSyncPrefs(PrefService* pref_service);

  SessionSyncPrefs(const SessionSyncPrefs&) = delete;
  SessionSyncPrefs& operator=(const SessionSyncPrefs&) = delete;

  ~SessionSyncPrefs();

  std::string GetLegacySyncSessionsGUID() const;
  void ClearLegacySyncSessionsGUID();

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

  void SetLegacySyncSessionsGUIDForTesting(const std::string& guid);

 private:
  const raw_ptr<PrefService> pref_service_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
