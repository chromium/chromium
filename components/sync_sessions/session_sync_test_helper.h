// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_
#define COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_

#include <string>
#include <vector>

#include "components/sessions/core/session_id.h"
#include "components/sync_device_info/device_info.h"

namespace sync_pb {
class SessionSpecifics;
}

namespace sync_sessions {

class SessionSyncTestHelper {
 public:
  SessionSyncTestHelper() : max_tab_node_id_(0) {}

  SessionSyncTestHelper(const SessionSyncTestHelper&) = delete;
  SessionSyncTestHelper& operator=(const SessionSyncTestHelper&) = delete;

  // Builds an instance of a SessionHeader, wrapped in SessionSpecifics, without
  // any windows. The caller may later add windows via `AddWindowSpecifics()` or
  // otherwise.
  static sync_pb::SessionSpecifics BuildHeaderSpecificsWithoutWindows(
      const std::string& tag,
      const std::string& client_name,
      const syncer::DeviceInfo::FormFactor& device_form_factor =
          syncer::DeviceInfo::FormFactor::kUnknown);

  // Adds window to a SessionSpecifics object.
  static void AddWindowSpecifics(SessionID window_id,
                                 const std::vector<SessionID>& tab_list,
                                 sync_pb::SessionSpecifics* meta);

  // Build a SessionSpecifics object with essential data needed for a tab and a
  // `tab_node_id`.
  static sync_pb::SessionSpecifics BuildTabSpecifics(
      const std::string& tag,
      const std::string& title,
      const std::string& virtual_url,
      SessionID window_id,
      SessionID tab_id,
      int tab_node_id);

  // Convenience overload of BuildTabSpecifics that uses sample title and URL.
  // Uses a monotonically increasing variable to generate tab_node_ids and avoid
  // conflicts.
  sync_pb::SessionSpecifics BuildTabSpecifics(const std::string& tag,
                                              SessionID window_id,
                                              SessionID tab_id);

  // Convenience overload of BuildTabSpecifics with custom title and URL. Uses a
  // monotonically increasing variable to generate tab_node_ids and avoid
  // conflicts.
  sync_pb::SessionSpecifics BuildTabSpecifics(const std::string& tag,
                                              const std::string& title,
                                              const std::string& virtual_url,
                                              SessionID window_id,
                                              SessionID tab_id);

  void Reset();

 private:
  int max_tab_node_id_;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_SESSION_SYNC_TEST_HELPER_H_
