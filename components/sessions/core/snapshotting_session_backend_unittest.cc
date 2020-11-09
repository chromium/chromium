// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/snapshotting_command_storage_backend.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/sessions/core/session_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MakeRefCounted;

namespace sessions {
namespace {

using SessionCommands = std::vector<std::unique_ptr<sessions::SessionCommand>>;

struct TestData {
  sessions::SessionCommand::id_type command_id;
  std::string data;
};

std::unique_ptr<sessions::SessionCommand> CreateCommandFromData(
    const TestData& data) {
  std::unique_ptr<sessions::SessionCommand> command =
      std::make_unique<sessions::SessionCommand>(
          data.command_id,
          static_cast<sessions::SessionCommand::size_type>(data.data.size()));
  if (!data.data.empty())
    memcpy(command->contents(), data.data.c_str(), data.data.size());
  return command;
}

}  // namespace

class SnapshottingCommandStorageBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("SessionTestDirs"));
    base::CreateDirectory(path_);
  }

  void AssertCommandEqualsData(const TestData& data,
                               sessions::SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  scoped_refptr<SnapshottingCommandStorageBackend> CreateBackend() {
    return MakeRefCounted<SnapshottingCommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(),
        sessions::SnapshottingCommandStorageManager::SESSION_RESTORE, path_);
  }

  base::test::TaskEnvironment task_environment_;
  // Path used in testing.
  base::FilePath path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(SnapshottingCommandStorageBackendTest, SimpleReadWrite) {
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false);
  ASSERT_TRUE(commands.empty());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands();

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  commands.clear();

  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands();

  ASSERT_EQ(0U, commands.size());

  // Make sure we can delete.
  backend->DeleteLastSession();
  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(0U, commands.size());
}

TEST_F(SnapshottingCommandStorageBackendTest, RandomData) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
      {3, "abc"},
      {4, "abcd"},
      {5, "abcde"},
      {6, "abcdef"},
      {7, "abcdefg"},
      {8, "abcdefgh"},
      {9, "abcdefghi"},
      {10, "abcdefghij"},
      {11, "abcdefghijk"},
      {12, "abcdefghijkl"},
      {13, "abcdefghijklm"},
  };

  for (size_t i = 0; i < base::size(data); ++i) {
    scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      commands = backend->ReadLastSessionCommands();
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j)
        AssertCommandEqualsData(data[j - commands.begin()], j->get());

      backend->AppendCommands(std::move(commands), false);
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), false);
  }
}

TEST_F(SnapshottingCommandStorageBackendTest, BigData) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
  };

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const sessions::SessionCommand::size_type big_size =
      SnapshottingCommandStorageBackend::kFileReadBufferSize + 100;
  const sessions::SessionCommand::id_type big_id = 50;
  std::unique_ptr<sessions::SessionCommand> big_command =
      std::make_unique<sessions::SessionCommand>(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(std::move(big_command));
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(std::move(commands), false);

  backend = nullptr;
  backend = CreateBackend();

  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
  commands.clear();
}

TEST_F(SnapshottingCommandStorageBackendTest, EmptyCommand) {
  TestData empty_command;
  empty_command.command_id = 1;
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  SessionCommands empty_commands;
  empty_commands.push_back(CreateCommandFromData(empty_command));
  backend->AppendCommands(std::move(empty_commands), true);
  backend->MoveCurrentSessionToLastSession();

  SessionCommands commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(empty_command, commands[0].get());
  commands.clear();
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(SnapshottingCommandStorageBackendTest, Truncate) {
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), false);

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands();

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());

  commands.clear();
}

// Test parsing the timestamp of a session from the path.
TEST_F(SnapshottingCommandStorageBackendTest, TimestampFromPath) {
  const auto base_dir = base::FilePath(kSessionsDirectory);

  // Test parsing the timestamp from a valid session.
  const auto test_path_1 = base_dir.Append(FILE_PATH_LITERAL("Tabs_0"));
  base::Time result_time_1;
  EXPECT_TRUE(SnapshottingCommandStorageBackend::TimestampFromPath(
      test_path_1, result_time_1));
  EXPECT_EQ(base::Time(), result_time_1);

  const auto test_path_2 =
      base_dir.Append(FILE_PATH_LITERAL("Session_13234316721694577"));
  base::Time result_time_2;
  EXPECT_TRUE(SnapshottingCommandStorageBackend::TimestampFromPath(
      test_path_2, result_time_2));
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromMicroseconds(13234316721694577)),
            result_time_2);

  // Test attempting to parse invalid file names.
  const auto invalid_path_1 =
      base_dir.Append(FILE_PATH_LITERAL("Session_nonsense"));
  base::Time invalid_result_1;
  EXPECT_FALSE(SnapshottingCommandStorageBackend::TimestampFromPath(
      invalid_path_1, invalid_result_1));

  const auto invalid_path_2 = base_dir.Append(FILE_PATH_LITERAL("Arbitrary"));
  base::Time invalid_result_2;
  EXPECT_FALSE(SnapshottingCommandStorageBackend::TimestampFromPath(
      invalid_path_2, invalid_result_2));
}

// Test serializing a timestamp to string.
TEST_F(SnapshottingCommandStorageBackendTest, FilePathFromTime) {
  const auto base_dir = base::FilePath(kSessionsDirectory);
  const auto test_time_1 = base::Time();
  const auto result_path_1 =
      SnapshottingCommandStorageBackend::FilePathFromTime(
          SnapshottingCommandStorageManager::SessionType::SESSION_RESTORE,
          base::FilePath(), test_time_1);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Session_0")), result_path_1);

  const auto test_time_2 = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(13234316721694577));
  const auto result_path_2 =
      SnapshottingCommandStorageBackend::FilePathFromTime(
          SnapshottingCommandStorageManager::SessionType::TAB_RESTORE,
          base::FilePath(), test_time_2);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Tabs_13234316721694577")),
            result_path_2);
}

// Test migrating a session from the old format.
TEST_F(SnapshottingCommandStorageBackendTest, ReadLegacySession) {
  // Create backend with some data.
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();

  // Set to legacy path for testing.
  const auto legacy_path = path_.Append(kLegacyCurrentSessionFileName);
  backend->SetPath(legacy_path);

  const struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true);
  EXPECT_TRUE(base::PathExists(legacy_path));

  // Reset backend and ensure we loaded from the legacy session.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(first_data, commands[0].get());
  commands.clear();

  // Add data to new session.
  struct TestData second_data = {2, "b"};
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true);
  EXPECT_TRUE(base::PathExists(legacy_path));

  // Reset backend and ensure we loaded from the newer session, not the legacy
  // one.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
  EXPECT_FALSE(base::PathExists(legacy_path));
}

// Test that the previous session is empty if no session files exist.
TEST_F(SnapshottingCommandStorageBackendTest, DeterminePreviousSessionEmpty) {
  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  backend->DetermineLastSessionFile();
  ASSERT_FALSE(backend->last_session_info_);
}

// Test that the previous session is selected correctly when a file is present.
TEST_F(SnapshottingCommandStorageBackendTest, DeterminePreviousSessionSingle) {
  const auto prev_path =
      path_.Append(base::FilePath(kSessionsDirectory)
                       .Append(FILE_PATH_LITERAL("Session_13235178308836991")));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  backend->DetermineLastSessionFile();
  ASSERT_TRUE(backend->last_session_info_);
  ASSERT_EQ(prev_path, backend->last_session_info_->path);
}

// Test that the previous session is selected correctly when multiple session
// files are present.
TEST_F(SnapshottingCommandStorageBackendTest,
       DeterminePreviousSessionMultiple) {
  const auto sessions_dir = path_.Append(base::FilePath(kSessionsDirectory));
  const auto prev_path =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308836991"));
  const auto old_path_1 =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308548874"));
  const auto old_path_2 = sessions_dir.Append(FILE_PATH_LITERAL("Session_0"));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));
  ASSERT_EQ(0, base::WriteFile(old_path_1, "", 0));
  ASSERT_EQ(0, base::WriteFile(old_path_2, "", 0));

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  backend->DetermineLastSessionFile();
  ASSERT_TRUE(backend->last_session_info_);
  ASSERT_EQ(prev_path, backend->last_session_info_->path);
}

// Test that the a file with an invalid name won't be used.
TEST_F(SnapshottingCommandStorageBackendTest, DeterminePreviousSessionInvalid) {
  const auto prev_path =
      path_.Append(base::FilePath(kSessionsDirectory)
                       .Append(FILE_PATH_LITERAL("Session_invalid")));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  backend->DetermineLastSessionFile();
  ASSERT_FALSE(backend->last_session_info_);
}

// Tests that MoveCurrentSessionToLastSession deletes the last session file.
TEST_F(SnapshottingCommandStorageBackendTest,
       MoveCurrentSessionToLastDeletesLastSession) {
  const auto sessions_dir = path_.Append(base::FilePath(kSessionsDirectory));
  const auto last_session =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308548874"));
  ASSERT_TRUE(base::CreateDirectory(last_session.DirName()));
  ASSERT_EQ(0, base::WriteFile(last_session, "", 0));

  scoped_refptr<SnapshottingCommandStorageBackend> backend = CreateBackend();
  char buffer[1];
  ASSERT_EQ(0, base::ReadFile(last_session, buffer, 0));
  backend->MoveCurrentSessionToLastSession();
  ASSERT_EQ(-1, base::ReadFile(last_session, buffer, 0));
}

}  // namespace sessions
