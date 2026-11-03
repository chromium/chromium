// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/session_sync_prefs.h"

#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace sync_sessions {
namespace {

const char kLocalDataOutOfSync[] = "sync.local_data_out_of_sync";

// A dictionary mapping foreign session tags (see
// `SyncedSession::GetSessionTag()`) to `true` for synced devices whose tabs
// the user has chosen to hide in this profile via
// `chrome://settings/deviceTabVisibility`. Sessions are visible by default
// and removed from the dictionary when unhidden or once the foreign session
// is no longer tracked by session sync (see
// `SessionSyncBridge::DoGarbageCollection()`).
constexpr char kDeviceTabVisibilityHiddenSessions[] =
    "sync.device_tab_visibility_hidden_sessions";

}  // namespace

// static
void SessionSyncPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kLocalDataOutOfSync, false);
  registry->RegisterDictionaryPref(kDeviceTabVisibilityHiddenSessions);
}

SessionSyncPrefs::SessionSyncPrefs(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      kDeviceTabVisibilityHiddenSessions,
      base::BindRepeating(&SessionSyncPrefs::NotifySessionVisibilityChanged,
                          base::Unretained(this)));
}

SessionSyncPrefs::~SessionSyncPrefs() = default;

bool SessionSyncPrefs::GetLocalDataOutOfSync() {
  return pref_service_->GetBoolean(kLocalDataOutOfSync);
}

void SessionSyncPrefs::SetLocalDataOutOfSync(bool local_data_out_of_sync) {
  pref_service_->SetBoolean(kLocalDataOutOfSync, local_data_out_of_sync);
}

SessionVisibility SessionSyncPrefs::GetSessionVisibility(
    std::string_view session_tag) const {
  return pref_service_->GetDict(kDeviceTabVisibilityHiddenSessions)
                 .contains(session_tag)
             ? SessionVisibility::kHidden
             : SessionVisibility::kVisible;
}

void SessionSyncPrefs::SetSessionVisibility(std::string_view session_tag,
                                            SessionVisibility visibility) {
  // Avoid a redundant pref change notification.
  if (GetSessionVisibility(session_tag) == visibility) {
    return;
  }

  ScopedDictPrefUpdate update(pref_service_,
                              kDeviceTabVisibilityHiddenSessions);
  if (visibility == SessionVisibility::kHidden) {
    update->Set(session_tag, true);
  } else {
    update->Remove(session_tag);
  }
}

void SessionSyncPrefs::GarbageCollectUnknownHiddenSessions(
    const std::set<std::string>& tracked_session_tags) {
  const base::DictValue& hidden_sessions =
      pref_service_->GetDict(kDeviceTabVisibilityHiddenSessions);
  if (hidden_sessions.empty()) {
    return;
  }

  std::vector<std::string> unknown_tags;
  for (const auto [session_tag, value] : hidden_sessions) {
    if (!tracked_session_tags.contains(session_tag)) {
      unknown_tags.push_back(session_tag);
    }
  }

  if (unknown_tags.empty()) {
    return;
  }

  ScopedDictPrefUpdate update(pref_service_,
                              kDeviceTabVisibilityHiddenSessions);
  for (const std::string& tag : unknown_tags) {
    update->Remove(tag);
  }
}

base::CallbackListSubscription
SessionSyncPrefs::SubscribeToSessionVisibilityChanged(
    const base::RepeatingClosure& cb) {
  return session_visibility_changed_closure_list_.Add(cb);
}

void SessionSyncPrefs::NotifySessionVisibilityChanged() {
  session_visibility_changed_closure_list_.Notify();
}

}  // namespace sync_sessions
