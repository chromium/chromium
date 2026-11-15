// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/fake_session_sync_service.h"

#include "base/check.h"
#include "base/functional/bind.h"

namespace sync_sessions {

FakeSessionSyncService::FakeSessionSyncService() = default;

FakeSessionSyncService::FakeSessionSyncService(PrefService* pref_service) {
  CHECK(pref_service);
  session_sync_prefs_.emplace(pref_service);
  session_visibility_subscription_ =
      session_sync_prefs_->SubscribeToSessionVisibilityChanged(
          base::BindRepeating(
              &FakeSessionSyncService::NotifyForeignSessionsChanged,
              base::Unretained(this)));
}

FakeSessionSyncService::~FakeSessionSyncService() = default;

void FakeSessionSyncService::NotifyForeignSessionsChanged() {
  subscriber_list_.Notify();
}

void FakeSessionSyncService::SetOpenTabsUIDelegateEnabled(bool enabled) {
  is_open_tabs_ui_delegate_enabled_ = enabled;
}

syncer::GlobalIdMapper* FakeSessionSyncService::GetGlobalIdMapper() const {
  return &global_id_mapper_;
}

FakeOpenTabsUIDelegate* FakeSessionSyncService::GetOpenTabsUIDelegate() {
  return is_open_tabs_ui_delegate_enabled_ ? &open_tabs_ui_delegate_ : nullptr;
}

base::CallbackListSubscription
FakeSessionSyncService::SubscribeToForeignSessionsChanged(
    const base::RepeatingClosure& cb) {
  return subscriber_list_.Add(cb);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
FakeSessionSyncService::GetControllerDelegate() {
  return nullptr;
}

}  // namespace sync_sessions
