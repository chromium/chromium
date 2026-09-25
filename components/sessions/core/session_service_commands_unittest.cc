// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/session_service_commands.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/pickle.h"
#include "components/sessions/core/serialized_navigation_entry.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_id.h"
#include "components/sessions/core/session_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace sessions {
namespace {

// Runs a replay of `commands` and returns the result, discarding the
// out-params the caller of this helper does not care about.
SessionReplayResult ReplayCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands,
    std::vector<std::unique_ptr<SessionWindow>>* valid_windows) {
  SessionID active_window_id = SessionID::InvalidValue();
  std::string platform_session_id;
  std::set<SessionID> discarded_window_ids;
  return RestoreSessionFromCommands(commands, valid_windows, &active_window_id,
                                    &platform_session_id,
                                    &discarded_window_ids);
}

// Builds the commands describing one normal window holding one tab that has a
// navigation, i.e. the minimum for a restorable session.
std::vector<std::unique_ptr<SessionCommand>> BuildRestorableSession(
    SessionID window_id,
    SessionID tab_id) {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(
      CreateSetWindowTypeCommand(window_id, SessionWindow::TYPE_NORMAL));
  commands.push_back(CreateSetTabWindowCommand(window_id, tab_id));
  commands.push_back(CreateSetTabIndexInWindowCommand(tab_id, 0));
  SerializedNavigationEntry nav;
  nav.set_index(0);
  nav.set_virtual_url(GURL("http://google.com"));
  commands.push_back(CreateUpdateTabNavigationCommand(tab_id, nav));
  return commands;
}

TEST(SessionServiceCommandsTest, ReplayResultNoCommands) {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kNoCommands,
            ReplayCommands(commands, &valid_windows));
  EXPECT_TRUE(valid_windows.empty());
}

TEST(SessionServiceCommandsTest, ReplayResultSuccess) {
  std::vector<std::unique_ptr<SessionCommand>> commands =
      BuildRestorableSession(SessionID::NewUnique(), SessionID::NewUnique());
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kSuccess,
            ReplayCommands(commands, &valid_windows));
  EXPECT_EQ(1u, valid_windows.size());
}

TEST(SessionServiceCommandsTest, ReplayResultAllCommandsClosed) {
  const SessionID window_id = SessionID::NewUnique();
  std::vector<std::unique_ptr<SessionCommand>> commands =
      BuildRestorableSession(window_id, SessionID::NewUnique());
  commands.push_back(CreateWindowClosedCommand(window_id));
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kAllCommandsClosed,
            ReplayCommands(commands, &valid_windows));
  EXPECT_TRUE(valid_windows.empty());
}

TEST(SessionServiceCommandsTest, ReplayResultAllTabsPrunedNoNavigations) {
  const SessionID window_id = SessionID::NewUnique();
  const SessionID tab_id = SessionID::NewUnique();
  // A window with a tab, but the tab never got a navigation.
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(
      CreateSetWindowTypeCommand(window_id, SessionWindow::TYPE_NORMAL));
  commands.push_back(CreateSetTabWindowCommand(window_id, tab_id));
  commands.push_back(CreateSetTabIndexInWindowCommand(tab_id, 0));
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kAllTabsPrunedNoNavigations,
            ReplayCommands(commands, &valid_windows));
  EXPECT_TRUE(valid_windows.empty());
}

TEST(SessionServiceCommandsTest, ReplayResultAllWindowsHadNoValidTabs) {
  // A window that never had a tab at all.
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(CreateSetWindowTypeCommand(SessionID::NewUnique(),
                                                SessionWindow::TYPE_NORMAL));
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kAllWindowsHadNoValidTabs,
            ReplayCommands(commands, &valid_windows));
  EXPECT_TRUE(valid_windows.empty());
}

TEST(SessionServiceCommandsTest, ReplayResultCorruptedCommand) {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(CreateSetWindowTypeCommand(SessionID::NewUnique(),
                                                SessionWindow::TYPE_NORMAL));
  // An id no version of Chrome writes, so replay aborts here.
  commands.push_back(std::make_unique<SessionCommand>(250, base::Pickle()));
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kCorruptedCommand,
            ReplayCommands(commands, &valid_windows));
  EXPECT_TRUE(valid_windows.empty());
}

// Corruption is reported even when the commands replayed before the bad one
// produced a usable window, because everything after it was lost.
TEST(SessionServiceCommandsTest, ReplayResultCorruptedCommandAfterValidWindow) {
  std::vector<std::unique_ptr<SessionCommand>> commands =
      BuildRestorableSession(SessionID::NewUnique(), SessionID::NewUnique());
  commands.push_back(std::make_unique<SessionCommand>(250, base::Pickle()));
  std::vector<std::unique_ptr<SessionWindow>> valid_windows;

  EXPECT_EQ(SessionReplayResult::kCorruptedCommand,
            ReplayCommands(commands, &valid_windows));
  EXPECT_EQ(1u, valid_windows.size());
}

}  // namespace
}  // namespace sessions
