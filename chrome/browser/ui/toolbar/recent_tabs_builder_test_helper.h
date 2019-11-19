// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_
#define CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/sessions/core/session_id.h"

namespace sync_pb {
class SessionSpecifics;
}

namespace sync_sessions {
class OpenTabsUIDelegate;
}

namespace syncer {
class ModelTypeProcessor;
struct UpdateResponseData;
}

// Utility class to help add recent tabs for testing.
class RecentTabsBuilderTestHelper {
 public:
  RecentTabsBuilderTestHelper();
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
                      const base::string16& title);
  int GetTabCount(int session_index, int window_index);
  SessionID GetTabID(int session_index, int window_index, int tab_index);
  base::Time GetTabTimestamp(int session_index,
                             int window_index,
                             int tab_index);
  base::string16 GetTabTitle(int session_index,
                       int window_index,
                       int tab_index);

  void ExportToSessionSync(syncer::ModelTypeProcessor* processor);
  void VerifyExport(sync_sessions::OpenTabsUIDelegate* delegate);

  std::vector<base::string16> GetTabTitlesSortedByRecency();

 private:
  sync_pb::SessionSpecifics BuildHeaderSpecifics(int session_index);
  void AddWindowToHeaderSpecifics(int session_index,
                                  int window_index,
                                  sync_pb::SessionSpecifics* specifics);
  sync_pb::SessionSpecifics BuildTabSpecifics(int session_index,
                                              int window_index,
                                              int tab_index);
  std::unique_ptr<syncer::UpdateResponseData> BuildUpdateResponseData(
      const sync_pb::SessionSpecifics& specifics,
      base::Time timestamp);

  struct TabInfo;
  struct WindowInfo;
  struct SessionInfo;

  std::vector<SessionInfo> sessions_;
  base::Time start_time_;

  int max_tab_node_id_ = 0;
  int next_response_version_ = 1;

  DISALLOW_COPY_AND_ASSIGN(RecentTabsBuilderTestHelper);
};

#endif  // CHROME_BROWSER_UI_TOOLBAR_RECENT_TABS_BUILDER_TEST_HELPER_H_
