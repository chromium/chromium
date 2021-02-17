// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_
#define COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_

#include <string>
#include <vector>

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
  ~OpenTabsUIDelegateImpl() override;

  // OpenTabsUIDelegate implementation.
  bool GetAllForeignSessions(
      std::vector<const SyncedSession*>* sessions) override;
  bool GetForeignSession(
      const std::string& tag,
      std::vector<const sessions::SessionWindow*>* windows) override;
  bool GetForeignTab(const std::string& tag,
                     SessionID tab_id,
                     const sessions::SessionTab** tab) override;
  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override;
  void DeleteForeignSession(const std::string& tag) override;
  bool GetLocalSession(const SyncedSession** local_session) override;

 private:
  const SyncSessionsClient* const sessions_client_;
  const SyncedSessionTracker* session_tracker_;
  DeleteForeignSessionCallback delete_foreign_session_cb_;

  DISALLOW_COPY_AND_ASSIGN(OpenTabsUIDelegateImpl);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_OPEN_TABS_UI_DELEGATE_IMPL_H_
