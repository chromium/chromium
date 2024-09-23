// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TEST_MATCHERS_H_
#define COMPONENTS_SYNC_SESSIONS_TEST_MATCHERS_H_

#include <map>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/sync/protocol/session_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace sync_sessions {

struct SyncedSession;

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<base::Time> session_start_time,
    testing::Matcher<std::vector<int>> window_ids,
    testing::Matcher<std::vector<int>> tab_ids);

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<std::vector<int>> window_ids,
    testing::Matcher<std::vector<int>> tab_ids);

// Convenience overloads.
testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    base::Time session_start_time,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids);
testing::Matcher<const sync_pb::SessionSpecifics&> MatchesHeader(
    testing::Matcher<std::string> session_tag,
    const std::vector<int>& window_ids,
    const std::vector<int>& tab_ids);

testing::Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<int> window_id,
    testing::Matcher<int> tab_id,
    testing::Matcher<int> tab_node_id,
    testing::Matcher<std::vector<std::string>> urls);

// Convenience overload.
testing::Matcher<const sync_pb::SessionSpecifics&> MatchesTab(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<int> window_id,
    testing::Matcher<int> tab_id,
    testing::Matcher<int> tab_node_id,
    const std::vector<std::string>& urls);

testing::Matcher<const SyncedSession*> MatchesSyncedSession(
    testing::Matcher<std::string> session_tag,
    testing::Matcher<std::map<int, std::vector<int>>> window_id_to_tabs);

// Convenience overload.
testing::Matcher<const SyncedSession*> MatchesSyncedSession(
    testing::Matcher<std::string> session_tag,
    const std::map<int, std::vector<int>>& window_id_to_tabs);

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TEST_MATCHERS_H_
