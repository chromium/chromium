// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_MOCK_TAB_RESTORE_SERVICE_H_
#define COMPONENTS_COMMERCE_CORE_MOCK_TAB_RESTORE_SERVICE_H_

#include <vector>

#include "components/sessions/core/tab_restore_service.h"
#include "testing/gmock/include/gmock/gmock.h"

enum class WindowOpenDisposition;
class SessionID;

namespace sessions {
class TabRestoreServiceObserver;
class LiveTab;
class LiveTabContext;
}  // namespace sessions

namespace tab_groups {
class TabGroupId;
}  // namespace tab_groups

class MockTabRestoreService : public sessions::TabRestoreService {
 public:
  MockTabRestoreService();
  ~MockTabRestoreService() override;

  MOCK_METHOD(void,
              AddObserver,
              (sessions::TabRestoreServiceObserver * observer),
              (override));
  MOCK_METHOD(void,
              RemoveObserver,
              (sessions::TabRestoreServiceObserver * observer),
              (override));

  MOCK_METHOD(std::optional<SessionID>,
              CreateHistoricalTab,
              (sessions::LiveTab * live_tab, int index),
              (override));

  MOCK_METHOD(void,
              CreateHistoricalGroup,
              (sessions::LiveTabContext * context,
               const tab_groups::TabGroupId& id),
              (override));

  MOCK_METHOD(void,
              GroupClosed,
              (const tab_groups::TabGroupId& group),
              (override));

  MOCK_METHOD(void,
              GroupCloseStopped,
              (const tab_groups::TabGroupId& group),
              (override));

  MOCK_METHOD(void,
              BrowserClosing,
              (sessions::LiveTabContext * context),
              (override));

  MOCK_METHOD(void,
              BrowserClosed,
              (sessions::LiveTabContext * context),
              (override));

  MOCK_METHOD(void, ClearEntries, (), (override));

  MOCK_METHOD(void,
              DeleteNavigationEntries,
              (const DeletionPredicate& predicate),
              (override));

  MOCK_METHOD(const Entries&, entries, (), (const, override));

  MOCK_METHOD(std::vector<sessions::LiveTab*>,
              RestoreMostRecentEntry,
              (sessions::LiveTabContext * context),
              (override));

  MOCK_METHOD(void, RemoveEntryById, (SessionID id), (override));

  MOCK_METHOD(std::vector<sessions::LiveTab*>,
              RestoreEntryById,
              (sessions::LiveTabContext * context,
               SessionID id,
               WindowOpenDisposition disposition),
              (override));

  MOCK_METHOD(void, LoadTabsFromLastSession, (), (override));

  MOCK_METHOD(bool, IsLoaded, (), (const, override));

  MOCK_METHOD(void, DeleteLastSession, (), (override));

  MOCK_METHOD(bool, IsRestoring, (), (const, override));
};

#endif  // COMPONENTS_COMMERCE_CORE_MOCK_TAB_RESTORE_SERVICE_H_
