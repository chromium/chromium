// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_

#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"

namespace sync_pb {
class SessionSpecifics;
}

namespace sync_sessions {

struct SyncedSession;

class SessionSyncTestHelper {
 public:
  SessionSyncTestHelper() : max_tab_node_id_(0) {}

  SessionSyncTestHelper(const SessionSyncTestHelper&) = delete;
  SessionSyncTestHelper& operator=(const SessionSyncTestHelper&) = delete;

  static void BuildSessionSpecifics(const std::string& tag,
                                    sync_pb::SessionSpecifics* meta);

  static void AddWindowSpecifics(SessionID window_id,
                                 const std::vector<SessionID>& tab_list,
                                 sync_pb::SessionSpecifics* meta);

  static void VerifySyncedSession(
      const std::string& tag,
      const std::vector<std::vector<SessionID>>& windows,
      const SyncedSession& session);

  // Build a SessionSpecifics object with a tab and sample data. Uses a
  // monotonically increasing variable to generate tab_node_ids and avoid
  // conflicts.
  sync_pb::SessionSpecifics BuildTabSpecifics(const std::string& tag,
                                              SessionID window_id,
                                              SessionID tab_id);

  // Overload of BuildTabSpecifics to allow forcing a specific tab_node_id.
  // Typically only useful to test reusing tab_node_ids.
  sync_pb::SessionSpecifics BuildTabSpecifics(const std::string& tag,
                                              SessionID window_id,
                                              SessionID tab_id,
                                              int tab_node_id);

  sync_pb::SessionSpecifics BuildForeignSession(
      const std::string& tag,
      const std::vector<SessionID>& tab_list,
      std::vector<sync_pb::SessionSpecifics>* tabs);

  void Reset();

 private:
  int max_tab_node_id_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_
