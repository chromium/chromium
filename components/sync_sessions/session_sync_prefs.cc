// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace sync_sessions {
namespace {

// The GUID session sync will use to identify this client, even across sync
// disable/enable events.
const char kSyncSessionsGUID[] = "sync.session_sync_guid";

}  // namespace

// static
void SessionSyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kSyncSessionsGUID, std::string());
}

SessionSyncPrefs::SessionSyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

SessionSyncPrefs::~SessionSyncPrefs() {}

std::string SessionSyncPrefs::GetSyncSessionsGUID() const {
  return pref_service_->GetString(kSyncSessionsGUID);
}

void SessionSyncPrefs::SetSyncSessionsGUID(const std::string& guid) {
  pref_service_->SetString(kSyncSessionsGUID, guid);
}

}  // namespace sync_sessions
