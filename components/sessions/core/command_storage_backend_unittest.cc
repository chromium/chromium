// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/sessions/core/command_storage_backend.h"

#include <stddef.h>

#include <limits>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/sessions/core/command_storage_manager.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_service_commands.h"
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
  // testing::TestWithParam:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Session"));
    restore_path_ =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("SessionTestDirs"));
    base::CreateDirectory(restore_path_);
  }

  void AssertCommandEqualsData(const TestData& data,
                               const SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  void AssertCommandsEqualsData(
      const TestData* data,
      size_t data_length,
      const std::vector<std::unique_ptr<SessionCommand>>& commands) {
    ASSERT_EQ(data_length, commands.size());
    for (size_t i = 0; i < data_length; ++i)
      EXPECT_NO_FATAL_FAILURE(
          AssertCommandEqualsData(data[i], commands[i].get()));
  }

  scoped_refptr<CommandStorageBackend> CreateBackend(
      const std::vector<uint8_t>& decryption_key = {},
      base::Clock* clock = nullptr) {
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), file_path_,
        CommandStorageManager::SessionType::kOther, decryption_key, clock);
  }

  scoped_refptr<CommandStorageBackend> CreateBackendWithRestoreType() {
    const CommandStorageManager::SessionType type =
        CommandStorageManager::SessionType::kSessionRestore;
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), restore_path_, type);
  }

  // Functions that call into private members of CommandStorageBackend.
  std::optional<CommandStorageBackend::SessionInfo> GetLastSessionInfo(
      CommandStorageBackend* backend) {
    // Force `last_session_info_` to be updated.
    backend->InitIfNecessary();
    return backend->last_session_info_;
  }

  std::vector<base::FilePath> GetSessionFilePathsSortedByReverseTimestamp() {
    auto infos = CommandStorageBackend::GetSessionFilesSortedByReverseTimestamp(
        file_path_, CommandStorageManager::SessionType::kOther);
    std::vector<base::FilePath> result;
    for (const auto& info : infos)
      result.push_back(info.path);
    return result;
  }

  static base::FilePath FilePathFromTime(
      CommandStorageManager::SessionType type,
      const base::FilePath& path,
      base::Time time) {
    return CommandStorageBackend::FilePathFromTime(type, path, time);
  }

  const base::FilePath& file_path() const { return file_path_; }

  const base::FilePath& restore_path() const { return restore_path_; }

 private:
  base::test::TaskEnvironment task_environment_;
  // Path used by CreateBackend().
  base::FilePath file_path_;
  // Path used by CreateBackendWithRestoreType().
  base::FilePath restore_path_;
  base::ScopedTempDir temp_dir_;
};

TEST_F(CommandStorageBackendTest, MigrateOther) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const auto path = backend->current_path();
  EXPECT_EQ(file_path().DirName(), path.DirName());
  auto base_name = file_path().BaseName().value();
  EXPECT_EQ(base_name, path.BaseName().value().substr(0, base_name.length()));
  backend = nullptr;

  // Move the file to the original path. This gives the logic before kOther
  // started using timestamps.
  ASSERT_TRUE(base::PathExists(path));
  ASSERT_TRUE(base::Move(path, file_path()));

  // Create the backend, should get back the data written.
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  // Write some more data.
  struct TestData data2 = {1, "b"};
  commands.clear();
  commands.push_back(CreateCommandFromData(data2));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Recreate, verify updated data read back and the original file has been
  // removed.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;
  EXPECT_FALSE(base::PathExists(file_path()));
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data2, commands[0].get());
}

TEST_F(CommandStorageBackendTest, SimpleReadWriteEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend(key);
  commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  // Repeat, but with the wrong key.
  backend = nullptr;
  ++(key[0]);
  backend = CreateBackend(key);
  commands = backend->ReadLastSessionCommands().commands;
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
  for (size_t i = 0; i < std::size(data); ++i) {
    scoped_refptr<CommandStorageBackend> backend = CreateBackend(key);
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      commands = backend->ReadLastSessionCommands().commands;
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j)
        AssertCommandEqualsData(data[j - commands.begin()], j->get());

      backend->AppendCommands(std::move(commands), true, base::DoNothing(),
                              key);
      commands = SessionCommands{};
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), i == 0, base::DoNothing(),
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
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key);

  backend = nullptr;
  backend = CreateBackend(key);

  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
}

TEST_F(CommandStorageBackendTest, MarkerOnlyEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  std::vector<uint8_t> key2 = key;
  ++(key2[0]);
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key2);

  backend = nullptr;
  backend = CreateBackend(key2);
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(CommandStorageBackendTest, TruncateEncrypted) {
  std::vector<uint8_t> key = CommandStorageManager::CreateCryptoKey();
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key);

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.clear();
  commands.push_back(CreateCommandFromData(second_data));
  std::vector<uint8_t> key2 = key;
  ++(key2[0]);
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key2);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend(key2);
  commands = backend->ReadLastSessionCommands().commands;

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
  backend->AppendCommands(std::move(commands), true, base::DoNothing(), key);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend(key);
  commands = backend->ReadLastSessionCommands().commands;

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
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;

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

TEST_F(CommandStorageBackendTest, IsNotValidFileWithoutMarker) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  const auto path = backend->current_path();
  backend->AppendCommands({}, true, base::DoNothing());
  backend = nullptr;

  EXPECT_FALSE(CommandStorageBackend::IsValidFile(path));
}

TEST_F(CommandStorageBackendTest, SimpleReadWriteWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands.clear();
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(data, commands[0].get());

  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(0U, commands.size());

  // Make sure we can delete.
  backend->DeleteLastSession();
  commands = backend->ReadLastSessionCommands().commands;
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

  for (size_t i = 0; i < std::size(data); ++i) {
    scoped_refptr<CommandStorageBackend> backend =
        CreateBackendWithRestoreType();
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      commands = backend->ReadLastSessionCommands().commands;
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j)
        AssertCommandEqualsData(data[j - commands.begin()], j->get());

      // Write the previous data back.
      backend->AppendCommands(std::move(commands), true, base::DoNothing());
      commands.clear();
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), i == 0, base::DoNothing());
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
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = nullptr;
  backend = CreateBackendWithRestoreType();

  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', reinterpret_cast<char*>(commands[1]->contents())[0]);
  EXPECT_EQ('z',
            reinterpret_cast<char*>(commands[1]->contents())[big_size - 1]);
}

TEST_F(CommandStorageBackendTest, CommandWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  SessionCommands commands;
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  backend->MoveCurrentSessionToLastSession();

  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(0U, commands.size());
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_F(CommandStorageBackendTest, TruncateWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), false, base::DoNothing());
  commands.clear();

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands().commands;

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
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
                base::Microseconds(13234316721694577)),
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
  const auto result_path_1 =
      FilePathFromTime(CommandStorageManager::SessionType::kSessionRestore,
                       base::FilePath(), test_time_1);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Session_0")), result_path_1);

  const auto test_time_2 = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13234316721694577));
  const auto result_path_2 =
      FilePathFromTime(CommandStorageManager::SessionType::kTabRestore,
                       base::FilePath(), test_time_2);
  EXPECT_EQ(base_dir.Append(FILE_PATH_LITERAL("Tabs_13234316721694577")),
            result_path_2);
}

// Test that the previous session is empty if no session files exist.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionEmptyWithRestoreType) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  ASSERT_FALSE(GetLastSessionInfo(backend.get()));
}

// Test that the previous session is selected correctly when a file is present.
TEST_F(CommandStorageBackendTest,
       DeterminePreviousSessionSingleWithRestoreType) {
  const auto prev_path = restore_path().Append(
      base::FilePath(kSessionsDirectory)
          .Append(FILE_PATH_LITERAL("Session_13235178308836991")));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
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
  ASSERT_TRUE(base::WriteFile(prev_path, ""));
  ASSERT_TRUE(base::WriteFile(old_path_1, ""));
  ASSERT_TRUE(base::WriteFile(old_path_2, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
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
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
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
  ASSERT_TRUE(base::WriteFile(last_session, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  char buffer[1];
  ASSERT_EQ(0, base::ReadFile(last_session, buffer, 0));
  backend->MoveCurrentSessionToLastSession();
  ASSERT_EQ(-1, base::ReadFile(last_session, buffer, 0));
}

TEST_F(CommandStorageBackendTest, GetSessionFiles) {
  EXPECT_TRUE(CommandStorageBackend::GetSessionFilePaths(
                  file_path(), CommandStorageManager::kOther)
                  .empty());
  ASSERT_TRUE(base::WriteFile(file_path(), ""));
  // Not a valid name, as doesn't contain timestamp separator.
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session 123"), ""));
  // Valid name.
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session_124"), ""));
  // Valid name, but should not be returned as beginning doesn't match.
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Foo_125"), ""));
  auto paths = CommandStorageBackend::GetSessionFilePaths(
      file_path(), CommandStorageManager::kOther);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("Session_124", paths.begin()->BaseName().MaybeAsASCII());
}

TEST_F(CommandStorageBackendTest, TimestampSeparatorIsAscii) {
  // Code in WebLayer relies on the timestamp separator being ascii.
  ASSERT_TRUE(!base::FilePath(kTimestampSeparator).MaybeAsASCII().empty());
}

TEST_F(CommandStorageBackendTest, GetSessionFilesAreSortedByReverseTimestamp) {
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session_130"), ""));
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session_120"), ""));
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session_125"), ""));
  ASSERT_TRUE(
      base::WriteFile(file_path().DirName().AppendASCII("Session_128"), ""));
  auto paths = GetSessionFilePathsSortedByReverseTimestamp();
  ASSERT_EQ(4u, paths.size());
  EXPECT_EQ("Session_130", paths[0].BaseName().MaybeAsASCII());
  EXPECT_EQ("Session_128", paths[1].BaseName().MaybeAsASCII());
  EXPECT_EQ("Session_125", paths[2].BaseName().MaybeAsASCII());
  EXPECT_EQ("Session_120", paths[3].BaseName().MaybeAsASCII());
}

TEST_F(CommandStorageBackendTest, UseMarkerWithoutValidMarker) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackendWithRestoreType();
  commands = backend->ReadLastSessionCommands().commands;
  // There should be no commands as a valid marker was not written.
  ASSERT_TRUE(commands.empty());

  // As there was no valid marker, there should be no last session file.
  EXPECT_FALSE(GetLastSessionInfo(backend.get()));
}

// This test moves a previously written file into the expected location and
// ensures it's read. This is to verify reading hasn't changed in an
// incompatible manner.
TEST_F(CommandStorageBackendTest, ReadPreviouslyWrittenData) {
  base::FilePath test_data_path;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_path));
  test_data_path = test_data_path.AppendASCII("components")
                       .AppendASCII("test")
                       .AppendASCII("data")
                       .AppendASCII("sessions")
                       .AppendASCII("last_session");
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

  ASSERT_TRUE(base::CopyFile(
      test_data_path, restore_path().Append(kLegacyCurrentSessionFileName)));
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  AssertCommandsEqualsData(data, std::size(data),
                           backend->ReadLastSessionCommands().commands);
}

TEST_F(CommandStorageBackendTest, NewFileOnTruncate) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackendWithRestoreType();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path1 = backend->current_path();

  // Path shouldn't change if truncate is false.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false, base::DoNothing());
  EXPECT_EQ(path1, backend->current_path());

  // Path should change on truncate, and `path1` should not be removed.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path2 = backend->current_path();
  EXPECT_TRUE(!path2.empty());
  EXPECT_NE(path1, path2);
  EXPECT_TRUE(base::PathExists(path1));
  EXPECT_TRUE(base::PathExists(path2));

  // Repeat. This time `path1` should be removed.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path3 = backend->current_path();
  EXPECT_TRUE(!path3.empty());
  EXPECT_NE(path1, path3);
  EXPECT_NE(path2, path3);
  EXPECT_FALSE(base::PathExists(path1));
  EXPECT_TRUE(base::PathExists(path2));
  EXPECT_TRUE(base::PathExists(path3));
}

TEST_F(CommandStorageBackendTest, AppendCommandsCallbackRunOnError) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->ForceAppendCommandsToFailForTesting();
  base::RunLoop run_loop;
  backend->AppendCommands({}, true, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(CommandStorageBackendTest, RestoresFileWithMarkerAfterFailure) {
  // Write `data` and a marker.
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {11, "X"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  EXPECT_TRUE(backend->IsFileOpen());

  // Make appending fail, which should close the file.
  backend->ForceAppendCommandsToFailForTesting();
  backend->AppendCommands({}, false, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpen());

  // Append again, with another fail. Should attempt to reopen file and file.
  backend->ForceAppendCommandsToFailForTesting();
  backend->AppendCommands({}, true, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpen());

  // Reopen and read last session. Should get `data` and marker.
  backend = nullptr;
  backend = CreateBackend();
  backend->AppendCommands({}, false, base::DoNothing());
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(1u, commands.size());
  AssertCommandEqualsData(data, commands[0].get());
}

TEST_F(CommandStorageBackendTest, PathTimeIncreases) {
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  scoped_refptr<CommandStorageBackend> backend = CreateBackend({}, &test_clock);
  // Write `data` and a marker.
  struct TestData data = {11, "X"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path1 = backend->current_path();
  EXPECT_FALSE(path1.empty());
  base::Time path1_time;
  EXPECT_TRUE(CommandStorageBackend::TimestampFromPath(path1, path1_time));

  test_clock.Advance(base::Seconds(-1));
  SessionCommands commands2;
  commands2.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands2), true, base::DoNothing());
  const base::FilePath path2 = backend->current_path();
  EXPECT_FALSE(path2.empty());
  EXPECT_NE(path1, path2);
  base::Time path2_time;
  EXPECT_TRUE(CommandStorageBackend::TimestampFromPath(path2, path2_time));
  // Even though the current time is before the previous time, the timestamp
  // of the file should increase.
  EXPECT_GT(path2_time, path1_time);
  // Backend needs to be destroyed before test_clock so we don't end up with
  // dangling reference.
  backend.reset();
}

}  // namespace sessions
