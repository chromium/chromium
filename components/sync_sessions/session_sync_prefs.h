// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_

#include <string>

#include "base/macros.h"

class PrefService;
class PrefRegistrySimple;

namespace sync_sessions {

// Use this for the unique machine tag used for session sync.
class SessionSyncPrefs {
 public:
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  explicit SessionSyncPrefs(PrefService* pref_service);
  ~SessionSyncPrefs();

  std::string GetSyncSessionsGUID() const;
  void SetSyncSessionsGUID(const std::string& guid);

 private:
  PrefService* const pref_service_;

  DISALLOW_COPY_AND_ASSIGN(SessionSyncPrefs);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_PREFS_H_
