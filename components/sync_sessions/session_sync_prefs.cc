// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace sync_sessions {
namespace {

const char kLocalDataOutOfSync[] = "sync.local_data_out_of_sync";

}  // namespace

// static
void SessionSyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLocalDataOutOfSync, false);
}

SessionSyncPrefs::SessionSyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

SessionSyncPrefs::~SessionSyncPrefs() = default;

bool SessionSyncPrefs::GetLocalDataOutOfSync() {
  return pref_service_->GetBoolean(kLocalDataOutOfSync);
}

void SessionSyncPrefs::SetLocalDataOutOfSync(bool local_data_out_of_sync) {
  pref_service_->SetBoolean(kLocalDataOutOfSync, local_data_out_of_sync);
}

}  // namespace sync_sessions
