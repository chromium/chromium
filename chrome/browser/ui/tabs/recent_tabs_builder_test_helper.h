// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_TEST_HELPER_H_
#define CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_TEST_HELPER_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sessions/core/session_id.h"

namespace sync_pb {
class SessionSpecifics;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}

namespace syncer {
class DataTypeProcessor;
struct UpdateResponseData;
}  // namespace syncer

// Utility class to help add recent tabs for testing.
class RecentTabsBuilderTestHelper {
 public:
  RecentTabsBuilderTestHelper();

  RecentTabsBuilderTestHelper(const RecentTabsBuilderTestHelper&) = delete;
  RecentTabsBuilderTestHelper& operator=(const RecentTabsBuilderTestHelper&) =
      delete;

  ~RecentTabsBuilderTestHelper();

  void AddSession();
  int GetSessionCount();
  SessionID GetSessionID(int session_index);
  base::Time GetSessionTimestamp(int session_index);

  void AddWindow(int session_index);
  int GetWindowCount(int session_index);
  SessionID GetWindowID(int session_index, int window_index);

  void AddTab(int session_index, int window_index);
  void AddTabWithInfo(int session_index,
                      int window_index,
                      base::Time timestamp,
                      const std::u16string& title);
  int GetTabCount(int session_index, int window_index);
  SessionID GetTabID(int session_index, int window_index, int tab_index);
  base::Time GetTabTimestamp(int session_index,
                             int window_index,
                             int tab_index);
  std::u16string GetTabTitle(int session_index,
                             int window_index,
                             int tab_index);

  void ExportToSessionSync(syncer::DataTypeProcessor* processor);
  void VerifyExport(sync_sessions::OpenTabsUIDelegate* delegate);

  std::vector<std::u16string> GetTabTitlesSortedByRecency();

 private:
  sync_pb::SessionSpecifics BuildHeaderSpecifics(int session_index);
  void AddWindowToHeaderSpecifics(int session_index,
                                  int window_index,
                                  sync_pb::SessionSpecifics* specifics);
  sync_pb::SessionSpecifics BuildTabSpecifics(int session_index,
                                              int window_index,
                                              int tab_index);
  syncer::UpdateResponseData BuildUpdateResponseData(
      const sync_pb::SessionSpecifics& specifics,
      base::Time timestamp);

  struct TabInfo;
  struct WindowInfo;
  struct SessionInfo;

  std::vector<SessionInfo> sessions_;
  base::Time start_time_;

  int max_tab_node_id_ = 0;
  int next_response_version_ = 1;
};

#endif  // CHROME_BROWSER_UI_TABS_RECENT_TABS_BUILDER_TEST_HELPER_H_
