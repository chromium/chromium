// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_MOCK_OPEN_TABS_UI_DELEGATE_H_
#define COMPONENTS_SYNC_SESSIONS_MOCK_OPEN_TABS_UI_DELEGATE_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sessions {
struct SessionTab;
struct SessionWindow;
}  // namespace sessions

namespace sync_sessions {

struct SyncedSession;

class MockOpenTabsUIDelegate : public OpenTabsUIDelegate {
 public:
  MockOpenTabsUIDelegate();

  MockOpenTabsUIDelegate(const MockOpenTabsUIDelegate&) = delete;
  MockOpenTabsUIDelegate& operator=(const MockOpenTabsUIDelegate&) = delete;

  ~MockOpenTabsUIDelegate() override;

  MOCK_METHOD(bool,
              GetAllForeignSessions,
              ((std::vector<raw_ptr<const SyncedSession, VectorExperimental>> *
                sessions)),
              (override));
  MOCK_METHOD((base::flat_map<std::string, base::Time>),
              GetAllForeignSessionLastModifiedTimes,
              (),
              (const, override));
  MOCK_METHOD(bool,
              GetForeignTab,
              (const std::string& tag,
               SessionID tab_id,
               const sessions::SessionTab** tab),
              (override));
  MOCK_METHOD(void, DeleteForeignSession, (const std::string& tag), (override));
  MOCK_METHOD(std::vector<const sessions::SessionWindow*>,
              GetForeignSession,
              (const std::string& tag),
              (override));
  MOCK_METHOD(bool,
              GetForeignSessionTabs,
              (const std::string& tag,
               std::vector<const sessions::SessionTab*>* tabs),
              (override));
  MOCK_METHOD(bool, GetLocalSession, (const SyncedSession** local), (override));
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_MOCK_OPEN_TABS_UI_DELEGATE_H_
