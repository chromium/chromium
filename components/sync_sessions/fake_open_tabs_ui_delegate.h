// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_FAKE_OPEN_TABS_UI_DELEGATE_H_
#define COMPONENTS_SYNC_SESSIONS_FAKE_OPEN_TABS_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/notimplemented.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/synced_session.h"

namespace sync_sessions {

class FakeOpenTabsUIDelegate : public OpenTabsUIDelegate {
 public:
  FakeOpenTabsUIDelegate();
  ~FakeOpenTabsUIDelegate() override;

  // OpenTabsUIDelegate overrides.
  bool GetAllForeignSessions(
      std::vector<raw_ptr<const SyncedSession, VectorExperimental>>* sessions)
      override;

  base::flat_map<std::string, base::Time>
  GetAllForeignSessionLastModifiedTimes() const override;

  bool GetForeignTab(const std::string& tag,
                     const SessionID tab_id,
                     const sessions::SessionTab** tab) override;

  void DeleteForeignSession(const std::string& tag) override;

  std::vector<const sessions::SessionWindow*> GetForeignSession(
      const std::string& tag) override;

  bool GetForeignSessionTabs(
      const std::string& tag,
      std::vector<const sessions::SessionTab*>* tabs) override;

  bool GetLocalSession(const SyncedSession** local) override;

  // Test helpers. FakeOpenTabsUIDelegate does not take ownership of the
  // SyncedSession instances, so they need to outlive the delegate.
  void SetForeignSessions(
      const std::vector<raw_ptr<const SyncedSession>>& sessions);

 private:
  std::vector<raw_ptr<const SyncedSession>> foreign_sessions_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_FAKE_OPEN_TABS_UI_DELEGATE_H_
