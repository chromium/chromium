// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_backend.h"

#include <stddef.h>
#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MakeRefCounted;

namespace sessions {

using size_type = SessionCommand::size_type;

namespace {

using SessionCommands = std::vector<std::unique_ptr<SessionCommand>>;

struct TestData {
  SessionCommand::id_type command_id;
  std::string data;
};

std::unique_ptr<SessionCommand> CreateCommandFromData(const TestData& data) {
  std::unique_ptr<SessionCommand> command = std::make_unique<SessionCommand>(
      data.command_id,
      static_cast<SessionCommand::size_type>(data.data.size()));
  if (!data.data.empty())
    memcpy(command->contents(), data.data.c_str(), data.data.size());
  return command;
}

}  // namespace

class CommandStorageBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Session"));
    restore_path_ =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("SessionTestDirs"));
    base::CreateDirectory(restore_path_);
  }

  void AssertCommandEqualsData(const TestData& data, SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  scoped_refptr<CommandStorageBackend> CreateBackend() {
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), file_path_,
        CommandStorageManager::SessionType::kOther);
  }

  scoped_refptr<CommandStorageBackend> CreateBackendWithRestoreType() {
    const CommandStorageManager::SessionType type =
        CommandStorageManager::SessionType::kSessionRestore;
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), restore_path_, type);
  }

  // Functions that call into private members of CommandStorageBackend.
  void SetPath(CommandStorageBackend* backend, const base::FilePath& path) {
    backend->SetPath(path);
  }

  void DetermineLastSessionFile(CommandStorageBackend* backend) {
    backend->DetermineLastSessionFile();
  }

  base::Optional<CommandStorageBackend::SessionInfo> GetLastSessionInfo(
      CommandStorageBackend* backend) {
    return backend->last_session_info_;
  }

  const base::FilePath& file_path() const { return file_path_; }

  const base::FilePath& restore_path() const { return restore_path_; }

 private:
  base::test::TaskEnvironment task_environment_;
  // Path used when a SessionType is not supplied.
  base::FilePath file_path_;
  // Path used by CreateBackendWithRestoreType().
  base::FilePath restore_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(CommandStorageBackendTest, SimpleReadWriteEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, key);
  ASSERT_TRUE(commands.empty());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadCurrentSessionCommands(key);

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  // Repeat, but with the wrong key.
  backend = nullptr;
  ++(key[0]);
  backend = CreateBackend();
  commands = backend->ReadCurrentSessionCommands(key);
  EXPECT_TRUE(commands.empty());
}

TEST_F(CommandStorageBackendTest, RandomDataEncrypted) {
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

  const std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  for (size_t i = 0; i < base::size(data); ++i) {
    scoped_refptr<CommandStorageBackend> backend = CreateBackend();
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      commands = backend->ReadCurrentSessionCommands(key);
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j)
        AssertCommandEqualsData(data[j - commands.begin()], j->get());

      backend->AppendCommands(std::move(commands), true, key);
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), i == 0,
                            i == 0 ? key : std::vector<uint8_t>());
  }
}

TEST_F(CommandStorageBackendTest, BigDataEncrypted) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
  };

  const std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  std::vector<std::unique_ptr<SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const SessionCommand::size_type big_size =
      CommandStorageBackend::kFileReadBufferSize + 100;
  const SessionCommand::id_type big_id = 50;
  std::unique_ptr<SessionCommand> big_command =
      std::make_unique<SessionCommand>(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(std::move(big_command));
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(std::move(commands), true, key);

  backend = nullptr;
  backend = CreateBackend();

  commands = backend->ReadCurrentSessionCommands(key);
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
}

TEST_F(CommandStorageBackendTest, EmptyCommandEncrypted) {
  TestData empty_command;
  empty_command.command_id = 1;
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands empty_commands;
  empty_commands.push_back(CreateCommandFromData(empty_command));
  std::vector<uint8_t> key2 = key;
  ++(key2[0]);
  backend->AppendCommands(std::move(empty_commands), true, key2);

  backend = nullptr;
  backend = CreateBackend();
  std::vector<std::unique_ptr<SessionCommand>> commands =
      backend->ReadCurrentSessionCommands(key2);
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(empty_command, commands[0].get());
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(CommandStorageBackendTest, TruncateEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true, key);

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.push_back(CreateCommandFromData(second_data));
  std::vector<uint8_t> key2 = key;
  ++(key2[0]);
  backend->AppendCommands(std::move(commands), true, key2);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadCurrentSessionCommands(key2);

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
}

std::unique_ptr<SessionCommand> CreateCommandWithMaxSize() {
  const size_type max_size_value = std::numeric_limits<size_type>::max();
  std::unique_ptr<SessionCommand> command =
      std::make_unique<SessionCommand>(11, max_size_value);
  for (int i = 0; i <= max_size_value; ++i)
    (command->contents())[i] = i;
  return command;
}

TEST_F(CommandStorageBackendTest, MaxSizeTypeEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();

  SessionCommands commands;
  commands.push_back(CreateCommandWithMaxSize());
  backend->AppendCommands(std::move(commands), true, key);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadCurrentSessionCommands(key);

  // Encryption restricts the main size, and results in truncation.
  ASSERT_EQ(1U, commands.size());
  auto expected_command = CreateCommandWithMaxSize();
  EXPECT_EQ(expected_command->id(), (commands[0])->id());
  const size_type expected_size =
      expected_command->size() -
      CommandStorageBackend::kEncryptionOverheadInBytes -
      sizeof(SessionCommand::id_type);
  ASSERT_EQ(expected_size, (commands[0])->size());
  EXPECT_TRUE(memcmp(commands[0]->contents(), expected_command->contents(),
                     expected_size) == 0);
}

TEST_F(CommandStorageBackendTest, MaxSizeType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();

  SessionCommands commands;
  commands.push_back(CreateCommandWithMaxSize());
  backend->AppendCommands(std::move(commands), true);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadCurrentSessionCommands({});

  ASSERT_EQ(1U, commands.size());
  auto expected_command = CreateCommandWithMaxSize();
  EXPECT_EQ(expected_command->id(), (commands[0])->id());
  const size_type expected_size =
      expected_command->size() - sizeof(SessionCommand::id_type);
  ASSERT_EQ(expected_size, (commands[0])->size());
  EXPECT_TRUE(memcmp(commands[0]->contents(), expected_command->contents(),
                     expected_size) == 0);
}

TEST_F(CommandStorageBackendTest, IsValidFileWithInvalidFiles) {
  base::WriteFile(file_path(), "z");
  EXPECT_FALSE(CommandStorageBackend::IsValidFile(file_path()));

  base::WriteFile(file_path(), "a longer string that does not match header");
  EXPECT_FALSE(CommandStorageBackend::IsValidFile(file_path()));
}

TEST_F(CommandStorageBackendTest, IsValidFileWithValidFile) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->AppendCommands({}, true);
  backend = nullptr;

  EXPECT_TRUE(CommandStorageBackend::IsValidFile(file_path()));
}

TEST_F(CommandStorageBackendTest, SimpleReadWriteWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false);
  ASSERT_TRUE(commands.empty());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands();

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  commands.clear();

  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands();

  ASSERT_EQ(0U, commands.size());

  // Make sure we can delete.
  backend->DeleteLastSession();
  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(0U, commands.size());
}

TEST_F(CommandStorageBackendTest, RandomDataWithRestoreType) {
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
    scoped_refptr<CommandStorageBackend> backend =
        CreateBackendWithRestoreType();
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

TEST_F(CommandStorageBackendTest, BigDataWithRestoreType) {
  struct TestData data[] = {
      {1, "a"},
      {2, "ab"},
  };

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  std::vector<std::unique_ptr<SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const SessionCommand::size_type big_size =
      CommandStorageBackend::kFileReadBufferSize + 100;
  const SessionCommand::id_type big_id = 50;
  std::unique_ptr<SessionCommand> big_command =
      std::make_unique<SessionCommand>(big_id, big_size);
  reinterpret_cast<char*>(big_command->contents())[0] = 'a';
  reinterpret_cast<char*>(big_command->contents())[big_size - 1] = 'z';
  commands.push_back(std::move(big_command));
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(std::move(commands), false);

  backend = nullptr;
  backend = CreateBackendWithRestoreType();

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

TEST_F(CommandStorageBackendTest, EmptyCommandWithRestoreType) {
  TestData empty_command;
  empty_command.command_id = 1;
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
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
TEST_F(CommandStorageBackendTest, TruncateWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
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
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands();

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());

  commands.clear();
}

// Test parsing the timestamp of a session from the path.
TEST_F(CommandStorageBackendTest, TimestampFromPathWithRestoreType) {
  const auto base_dir = base::FilePath(kSessionsDirectory);

  // Test parsing the timestamp from a valid session.
  const auto test_path_1 = base_dir.Append(FILE_PATH_LITERAL("Tabs_0"));
  base::Time result_time_1;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_1, result_time_1));
  EXPECT_EQ(base::Time(), result_time_1);

  const auto test_path_2 =
      base_dir.Append(FILE_PATH_LITERAL("Session_13234316721694577"));
  base::Time result_time_2;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_2, result_time_2));
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::TimeDelta::FromMicroseconds(13234316721694577)),
            result_time_2);

  // Test attempting to parse invalid file names.
  const auto invalid_path_1 =
      base_dir.Append(FILE_PATH_LITERAL("Session_nonsense"));
  base::Time invalid_result_1;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_1,
                                                        invalid_result_1));

  const auto invalid_path_2 = base_dir.Append(FILE_PATH_LITERAL("Arbitrary"));
  base::Time invalid_result_2;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_2,
                                                        invalid_result_2));
}

// Test serializing a timestamp to string.
TEST_F(CommandStorageBackendTest, FilePathFromTimeWithRestoreType) {
  const auto base_dir = base::FilePath(kSessionsDirectory);
  const auto test_time_1 = base::Time();
  const auto result_path_1 = CommandStorageBackend::FilePathFromTime(
      CommandStorageManager::SessionType::kSessionRestore, base::FilePath(),
      test_time_1);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Session_0")), result_path_1);

  const auto test_time_2 = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(13234316721694577));
  const auto result_path_2 = CommandStorageBackend::FilePathFromTime(
      CommandStorageManager::SessionType::kTabRestore, base::FilePath(),
      test_time_2);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Tabs_13234316721694577")),
            result_path_2);
}

// Test migrating a session from the old format.
TEST_F(CommandStorageBackendTest, ReadLegacySessionWithRestoreType) {
  // Create backend with some data.
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();

  // Set to legacy path for testing.
  const auto legacy_path = restore_path().Append(kLegacyCurrentSessionFileName);
  SetPath(backend.get(), legacy_path);

  const struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true);
  EXPECT_TRUE(base::PathExists(legacy_path));

  // Reset backend and ensure we loaded from the legacy session.
  backend = nullptr;
  backend = CreateBackendWithRestoreType();
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
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands();
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
  EXPECT_FALSE(base::PathExists(legacy_path));
}

// Test that the previous session is empty if no session files exist.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionEmptyWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  DetermineLastSessionFile(backend.get());
  ASSERT_FALSE(GetLastSessionInfo(backend.get()));
}

// Test that the previous session is selected correctly when a file is present.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionSingleWithRestoreType) {
  const auto prev_path = restore_path().Append(
      base::FilePath(kSessionsDirectory)
          .Append(FILE_PATH_LITERAL("Session_13235178308836991")));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  DetermineLastSessionFile(backend.get());
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_TRUE(last_session_info);
  ASSERT_EQ(prev_path, last_session_info->path);
}

// Test that the previous session is selected correctly when multiple session
// files are present.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionMultipleWithRestoreType) {
  const auto sessions_dir =
      restore_path().Append(base::FilePath(kSessionsDirectory));
  const auto prev_path =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308836991"));
  const auto old_path_1 =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308548874"));
  const auto old_path_2 = sessions_dir.Append(FILE_PATH_LITERAL("Session_0"));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));
  ASSERT_EQ(0, base::WriteFile(old_path_1, "", 0));
  ASSERT_EQ(0, base::WriteFile(old_path_2, "", 0));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  DetermineLastSessionFile(backend.get());
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_TRUE(last_session_info);
  ASSERT_EQ(prev_path, last_session_info->path);
}

// Test that the a file with an invalid name won't be used.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionInvalidWithRestoreType) {
  const auto prev_path =
      restore_path().Append(base::FilePath(kSessionsDirectory)
                                .Append(FILE_PATH_LITERAL("Session_invalid")));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_EQ(0, base::WriteFile(prev_path, "", 0));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  DetermineLastSessionFile(backend.get());
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_FALSE(last_session_info);
}

// Tests that MoveCurrentSessionToLastSession deletes the last session file.
TEST_F(CommandStorageBackendTest,
       MoveCurrentSessionToLastDeletesLastSessionWithRestoreType) {
  const auto sessions_dir =
      restore_path().Append(base::FilePath(kSessionsDirectory));
  const auto last_session =
      sessions_dir.Append(FILE_PATH_LITERAL("Session_13235178308548874"));
  ASSERT_TRUE(base::CreateDirectory(last_session.DirName()));
  ASSERT_EQ(0, base::WriteFile(last_session, "", 0));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  char buffer[1];
  ASSERT_EQ(0, base::ReadFile(last_session, buffer, 0));
  backend->MoveCurrentSessionToLastSession();
  ASSERT_EQ(-1, base::ReadFile(last_session, buffer, 0));
}

}  // namespace sessions
