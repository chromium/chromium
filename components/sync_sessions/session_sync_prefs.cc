// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace sync_sessions {
namespace {

// Legacy GUID to identify this client, no longer newly populated by modern
// clients but honored if present.
const char kLegacySyncSessionsGUID[] = "sync.session_sync_guid";

const char kLocalDataOutOfSync[] = "sync.local_data_out_of_sync";

}  // namespace

// static
void SessionSyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(kLegacySyncSessionsGUID, std::string());
  registry->RegisterBooleanPref(kLocalDataOutOfSync, false);
}

SessionSyncPrefs::SessionSyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

SessionSyncPrefs::~SessionSyncPrefs() = default;

std::string SessionSyncPrefs::GetLegacySyncSessionsGUID() const {
  return pref_service_->GetString(kLegacySyncSessionsGUID);
}

void SessionSyncPrefs::ClearLegacySyncSessionsGUID() {
  pref_service_->ClearPref(kLegacySyncSessionsGUID);
}

bool SessionSyncPrefs::GetLocalDataOutOfSync() {
  return pref_service_->GetBoolean(kLocalDataOutOfSync);
}

void SessionSyncPrefs::SetLocalDataOutOfSync(bool local_data_out_of_sync) {
  pref_service_->SetBoolean(kLocalDataOutOfSync, local_data_out_of_sync);
}

void SessionSyncPrefs::SetLegacySyncSessionsGUIDForTesting(
    const std::string& guid) {
  pref_service_->SetString(kLegacySyncSessionsGUID, guid);
}

}  // namespace sync_sessions
