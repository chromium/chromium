// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"

namespace sync_sessions {

class SyncSessionsClient;
class SyncedSessionTracker;

class OpenTabsUIDelegateImpl : public OpenTabsUIDelegate {
 public:
  using DeleteForeignSessionCallback =
      base::RepeatingCallback<void(const std::string&)>;

  // |sessions_client| and |session_tracker| must not be null and must outlive
  // this object. |delete_foreign_session_cb| allows to forward calls to
  // DeleteForeignSession() which this class doesn't implement.
  OpenTabsUIDelegateImpl(
      const SyncSessionsClient* sessions_client,
      const SyncedSessionTracker* session_tracker,
      const DeleteForeignSessionCallback& delete_foreign_session_cb);

  OpenTabsUIDelegateImpl(const OpenTabsUIDelegateImpl&) = delete;
  OpenTabsUIDelegateImpl& operator=(const OpenTabsUIDelegateImpl&) = delete;

  ~OpenTabsUIDelegateImpl() override;

  // OpenTabsUIDelegate implementation.
  bool GetAllForeignSessions(
      std::vector<raw_ptr<const SyncedSession, VectorExperimental>>* sessions)
      override;
  std::vector<const sessions::SessionWindow*> GetForeignSession(
      const std::string& tag) override;
  bool GetForeignTab(const std::string& tag,
                     SessionID tab_id,
                     const sessions::SessionTab** tab) override;
  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override;
  void DeleteForeignSession(const std::string& tag) override;
  bool GetLocalSession(const SyncedSession** local_session) override;

 private:
  const raw_ptr<const SyncSessionsClient> sessions_client_;
  const raw_ptr<const SyncedSessionTracker> session_tracker_;
  DeleteForeignSessionCallback delete_foreign_session_cb_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_
