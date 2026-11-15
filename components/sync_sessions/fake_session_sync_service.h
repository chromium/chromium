// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FAKE_SESSION_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_FAKE_SESSION_SYNC_SERVICE_H_

#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/sessions_global_id_mapper.h"

class PrefService;

namespace sync_sessions {

// Fake `SessionSyncService` implementation for unit and browser tests.
// Owns a `FakeOpenTabsUIDelegate` and, when constructed with a `PrefService*`,
// forwards `SessionSyncPrefs` visibility changes to
// `SubscribeToForeignSessionsChanged()` subscribers to match
// `SessionSyncServiceImpl`.
class FakeSessionSyncService : public SessionSyncService {
 public:
  FakeSessionSyncService();
  // `pref_service` must not be null and must outlive this object.
  explicit FakeSessionSyncService(PrefService* pref_service);

  FakeSessionSyncService(const FakeSessionSyncService&) = delete;
  FakeSessionSyncService& operator=(const FakeSessionSyncService&) = delete;

  ~FakeSessionSyncService() override;

  void NotifyForeignSessionsChanged();

  // Controls whether `GetOpenTabsUIDelegate()` returns
  // `&open_tabs_ui_delegate_` (when `enabled` is true, the default) or
  // `nullptr` (simulating tab sync being disabled).
  void SetOpenTabsUIDelegateEnabled(bool enabled);

  // SessionSyncService overrides.
  syncer::GlobalIdMapper* GetGlobalIdMapper() const override;
  FakeOpenTabsUIDelegate* GetOpenTabsUIDelegate() override;
  base::CallbackListSubscription SubscribeToForeignSessionsChanged(
      const base::RepeatingClosure& cb) override;
  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override;

 private:
  std::optional<SessionSyncPrefs> session_sync_prefs_;
  base::CallbackListSubscription session_visibility_subscription_;
  base::RepeatingClosureList subscriber_list_;
  bool is_open_tabs_ui_delegate_enabled_ = true;
  FakeOpenTabsUIDelegate open_tabs_ui_delegate_;
  mutable SessionsGlobalIdMapper global_id_mapper_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FAKE_SESSION_SYNC_SERVICE_H_
