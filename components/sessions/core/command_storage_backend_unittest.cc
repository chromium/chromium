// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_backend.h"

#include <stddef.h>

#include <array>
#include <limits>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
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
using SessionType = CommandStorageManager::SessionType;

struct TestData {
  SessionCommand::id_type command_id;
  std::string data;
};

std::unique_ptr<SessionCommand> CreateCommandFromData(const TestData& data) {
  std::unique_ptr<SessionCommand> command = std::make_unique<SessionCommand>(
      data.command_id,
      static_cast<SessionCommand::size_type>(data.data.size()));
  if (!data.data.empty()) {
    command->contents().copy_from(base::as_byte_span(data.data));
  }
  return command;
}

struct TestParams {
  SessionType session_type;
};

}  // namespace

class CommandStorageBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    init_path_ = temp_dir_.GetPath();
    sessions_dir_ = init_path_.Append(kSessionsDirectory);
    base::CreateDirectory(sessions_dir_);
  }

  void AssertCommandEqualsData(const TestData& data,
                               const SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_EQ(command->contents(), base::as_byte_span(data.data));
  }

  void AssertCommandsEqualsData(
      base::span<const TestData> data,
      const std::vector<std::unique_ptr<SessionCommand>>& commands) {
    ASSERT_EQ(data.size(), commands.size());
    for (size_t i = 0; i < data.size(); ++i) {
      EXPECT_NO_FATAL_FAILURE(
          AssertCommandEqualsData(data[i], commands[i].get()));
    }
  }

  scoped_refptr<CommandStorageBackend> CreateBackend(
      const SessionType session_type,
      base::Clock* clock = nullptr) {
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), init_path_, session_type,
        clock);
  }

  // Functions that call into private members of CommandStorageBackend.
  std::optional<CommandStorageBackend::SessionInfo> GetLastSessionInfo(
      CommandStorageBackend* backend) {
    // Force `last_session_info_` to be updated.
    backend->InitIfNecessary();
    return backend->last_session_info_;
  }

  std::vector<base::FilePath> GetSessionFilePathsSortedByReverseTimestamp(
      SessionType session_type) {
    auto infos = CommandStorageBackend::GetSessionFilesSortedByReverseTimestamp(
        init_path_, session_type);
    std::vector<base::FilePath> result;
    for (const auto& info : infos) {
      result.push_back(info.path);
    }
    return result;
  }

  // Helper for calling CommandStorageBackend::FilePathFromTime() with a time
  // delta in microseconds.
  static base::FilePath FilePathFromTime(SessionType type,
                                         const base::FilePath& path,
                                         uint64_t time_delta_microseconds) {
    return CommandStorageBackend::FilePathFromTime(
        type, path,
        base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(time_delta_microseconds)));
  }

  base::FilePath GetTestFilePath(const std::string& test_data_filename) {
    base::FilePath test_file_path;
    if (!base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT,
                                &test_file_path)) {
      return base::FilePath();
    }
    return test_file_path.AppendASCII("components")
        .AppendASCII("test")
        .AppendASCII("data")
        .AppendASCII("sessions")
        .AppendASCII(test_data_filename);
  }

  bool copyTestDataToSessionFile(const std::string& test_data_filename,
                                 const std::string& session_filename) {
    base::FilePath test_file_path = GetTestFilePath(test_data_filename);
    if (!base::PathExists(test_file_path)) {
      return false;
    }
    return base::CopyFile(test_file_path,
                          sessions_dir().AppendASCII(session_filename));
  }

  // The path that is passed to `CreateBackend`.
  const base::FilePath& init_path() const { return init_path_; }

  // The path to the directory that contains the session files.
  const base::FilePath& sessions_dir() const { return sessions_dir_; }

 private:
  base::test::TaskEnvironment task_environment_;
  base::FilePath init_path_;     // Passed to CommandStorageBackend constructor.
  base::FilePath sessions_dir_;  // The directory containing the session files.
  base::ScopedTempDir temp_dir_;
};

// Most tests are parameterized to run for each session type (see
// Non-parameterized tests for CommandStorageBackend.
// `CommandStorageBackendParamTest` below).  Test that do not use parameters
// are grouped here into `CommandStorageBackendTest`.

TEST_F(CommandStorageBackendTest, GetSessionFiles_AppRestore) {
  EXPECT_TRUE(CommandStorageBackend::GetSessionFilePaths(
                  init_path(), SessionType::kAppRestore)
                  .empty());
  // Not a valid name, as doesn't contain timestamp separator.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Apps 123"), ""));
  // Valid name.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Apps_124"), ""));
  // Valid name, but should not be returned as beginning doesn't match.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Foo_125"), ""));
  auto paths = CommandStorageBackend::GetSessionFilePaths(
      init_path(), SessionType::kAppRestore);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("Apps_124", paths.begin()->BaseName().MaybeAsASCII());
}

TEST_F(CommandStorageBackendTest, GetSessionFiles_SessionRestore) {
  EXPECT_TRUE(CommandStorageBackend::GetSessionFilePaths(
                  init_path(), SessionType::kSessionRestore)
                  .empty());
  // Not a valid name, as doesn't contain timestamp separator.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Session 123"), ""));
  // Valid name.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Session_124"), ""));
  // Valid name, but should not be returned as beginning doesn't match.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Foo_125"), ""));
  auto paths = CommandStorageBackend::GetSessionFilePaths(
      init_path(), SessionType::kSessionRestore);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("Session_124", paths.begin()->BaseName().MaybeAsASCII());
}

TEST_F(CommandStorageBackendTest, GetSessionFiles_TabRestore) {
  EXPECT_TRUE(CommandStorageBackend::GetSessionFilePaths(
                  init_path(), SessionType::kTabRestore)
                  .empty());
  // Not a valid name, as doesn't contain timestamp separator.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Tabs 123"), ""));
  // Valid name.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Tabs_124"), ""));
  // Valid name, but should not be returned as beginning doesn't match.
  ASSERT_TRUE(base::WriteFile(sessions_dir().AppendASCII("Foo_125"), ""));
  auto paths = CommandStorageBackend::GetSessionFilePaths(
      init_path(), SessionType::kTabRestore);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ("Tabs_124", paths.begin()->BaseName().MaybeAsASCII());
}

// Test that the a file with an invalid name won't be used.
TEST_F(CommandStorageBackendTest, DeterminePreviousSessionInvalid) {
  const auto prev_path =
      sessions_dir().Append(FILE_PATH_LITERAL("Session_invalid"));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_FALSE(last_session_info);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV1) {
  // V1 files do not contain markers.
  // They were used in production prior to commit 223e5cd on 2021-05-25.
  ASSERT_TRUE(copyTestDataToSessionFile("Session-v1NoMarker", "Session_1234"));

  // V1 files are no longer supported.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  ASSERT_FALSE(
      backend->IsValidFileForTest(sessions_dir().AppendASCII("Session_1234")));
  SessionCommands commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV2) {
  // V2 files are encrypted and do not contain markers.
  // They could have been written prior to commit 223e5cd on 2021-05-25.
  // They were never used in production.
  ASSERT_TRUE(
      copyTestDataToSessionFile("Session-v2NoMarkerEncrypted", "Session_1234"));

  // V2 files are no longer supported.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  ASSERT_FALSE(
      backend->IsValidFileForTest(sessions_dir().AppendASCII("Session_1234")));
  SessionCommands commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV3) {
  // V3 files contain markers.
  // They have been used in production from early 2021 through at least
  // 2026-02.
  ASSERT_TRUE(
      copyTestDataToSessionFile("Session-v3WithMarker", "Session_1234"));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  SessionCommands commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(1u, commands.size());
  AssertCommandEqualsData(TestData({1, "a"}), commands[0].get());
}

TEST_F(CommandStorageBackendTest, WriteSessionFileV3) {
  // This test ensures that we don't accidentally change the format of V3 files.
  // If you intend to change the output file format, then you should create a
  // new test data file, and update this test to read that new file.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath written_path = backend->current_path_for_testing();

  // Ensure that the file is fully written and contains the expected data.
  base::SequencedTaskRunner* task_runner = backend->owning_task_runner();
  backend.reset();
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  const base::FilePath expected_data_path =
      GetTestFilePath("Session-v3WithMarker");
  base::MemoryMappedFile written_file;
  ASSERT_TRUE(written_file.Initialize(written_path));
  base::MemoryMappedFile expected_data_file;
  ASSERT_TRUE(expected_data_file.Initialize(expected_data_path));
  ASSERT_EQ(expected_data_file.length(), written_file.length());
  ASSERT_EQ(expected_data_file.bytes(), written_file.bytes());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV3With2Appends) {
  // V3 files contain markers.
  // They have been used in production from early 2021 through at least
  // 2026-02.
  // This test file was created using 2 calls to backend->AppendCommands,
  // which results in the Marker being in the middle of the file (between
  // the "banana" and "coconut" commands).  See also related test
  // `WriteSessionFileV3With2Appends`.
  ASSERT_TRUE(
      copyTestDataToSessionFile("Session-v3With2Appends", "Session_1234"));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  SessionCommands commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(4u, commands.size());
  AssertCommandEqualsData(TestData({1, "apple"}), commands[0].get());
  AssertCommandEqualsData(TestData({2, "banana"}), commands[1].get());
  AssertCommandEqualsData(TestData({3, "coconut"}), commands[2].get());
  AssertCommandEqualsData(TestData({4, "durian"}), commands[3].get());
}

TEST_F(CommandStorageBackendTest, WriteSessionFileV3With2Appends) {
  // This test ensures that we don't accidentally change the format of V3 files.
  // If you intend to change the output file format, then you should create a
  // new test data file, and update this test to read that new file.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  SessionCommands first_commands;
  first_commands.push_back(CreateCommandFromData(TestData({1, "apple"})));
  first_commands.push_back(CreateCommandFromData(TestData({2, "banana"})));
  backend->AppendCommands(std::move(first_commands), true, base::DoNothing());
  // Since truncate=true, a marker is written after the "banana" command.
  SessionCommands second_commands;
  second_commands.push_back(CreateCommandFromData(TestData({3, "coconut"})));
  second_commands.push_back(CreateCommandFromData(TestData({4, "durian"})));
  backend->AppendCommands(std::move(second_commands), false, base::DoNothing());
  // Since truncate=false, no marker is written after the "durian" command.
  const base::FilePath written_path = backend->current_path_for_testing();

  // Ensure that the file is fully written and contains the expected data.
  base::SequencedTaskRunner* task_runner = backend->owning_task_runner();
  backend.reset();
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  const base::FilePath expected_data_path =
      GetTestFilePath("Session-v3With2Appends");
  base::MemoryMappedFile written_file;
  ASSERT_TRUE(written_file.Initialize(written_path));
  base::MemoryMappedFile expected_data_file;
  ASSERT_TRUE(expected_data_file.Initialize(expected_data_path));
  ASSERT_EQ(expected_data_file.length(), written_file.length());
  ASSERT_EQ(expected_data_file.bytes(), written_file.bytes());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV4) {
  // V4 files contain markers and are encrypted.
  // They have never been used in production, but could have been written from
  // early 2021 through at least 2026-02.
  ASSERT_TRUE(copyTestDataToSessionFile("Session-v4WithMarkerEncrypted",
                                        "Session_1234"));

  // V4 files are no longer supported.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  ASSERT_FALSE(
      backend->IsValidFileForTest(sessions_dir().AppendASCII("Session_1234")));
  SessionCommands commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

// Parameterized tests for CommandStorageBackend.
class CommandStorageBackendParamTest
    : public CommandStorageBackendTest,
      public testing::WithParamInterface<TestParams> {
 protected:
  scoped_refptr<CommandStorageBackend> CreateBackend() {
    return CommandStorageBackendTest::CreateBackend(GetParam().session_type);
  }
  scoped_refptr<CommandStorageBackend> CreateBackend(base::Clock* clock) {
    return CommandStorageBackendTest::CreateBackend(GetParam().session_type,
                                                    clock);
  }
};

TEST_P(CommandStorageBackendParamTest, SimpleWrite) {
  SessionType session_type = GetParam().session_type;
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(13234316721694577)));
  scoped_refptr<CommandStorageBackend> backend = CreateBackend(&test_clock);
  auto data = std::to_array<TestData>({
      {1, "a"},
      {2, "bc"},
      {3, "def"},
  });
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data[0]));
  commands.push_back(CreateCommandFromData(data[1]));
  commands.push_back(CreateCommandFromData(data[2]));
  bool write_error = false;

  backend->AppendCommands(
      std::move(commands), true,
      base::BindLambdaForTesting([&write_error]() { write_error = true; }));

  EXPECT_FALSE(write_error);
  const base::FilePath path = backend->current_path_for_testing();
  EXPECT_TRUE(base::PathExists(path));
  base::FilePath expected_path =
      FilePathFromTime(session_type, init_path(), 13234316721694577);
  EXPECT_EQ(expected_path, path);
  EXPECT_GT(base::GetFileSize(path), 0);
}

TEST_P(CommandStorageBackendParamTest, SimpleReadWrite) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto data = std::to_array<TestData>({
      {1, "a"},
      {2, "bc"},
      {3, "def"},
  });
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data[0]));
  commands.push_back(CreateCommandFromData(data[1]));
  commands.push_back(CreateCommandFromData(data[2]));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = CreateBackend();  // Necessary to recognize the file we just wrote.
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[1].get());
  AssertCommandEqualsData(data[2], commands[2].get());
}

TEST_P(CommandStorageBackendParamTest, RandomData) {
  auto data = std::to_array<TestData>({
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
  });

  for (size_t i = 0; i < std::size(data); ++i) {
    scoped_refptr<CommandStorageBackend> backend = CreateBackend();
    SessionCommands commands;
    if (i != 0) {
      // Read previous data.
      commands = backend->ReadLastSessionCommands().commands;
      ASSERT_EQ(i, commands.size());
      for (auto j = commands.begin(); j != commands.end(); ++j) {
        AssertCommandEqualsData(data[j - commands.begin()], j->get());
      }

      backend->AppendCommands(std::move(commands), true, base::DoNothing());
      commands = SessionCommands{};
    }
    commands.push_back(CreateCommandFromData(data[i]));
    backend->AppendCommands(std::move(commands), i == 0, base::DoNothing());
  }
}

TEST_P(CommandStorageBackendParamTest, BigData) {
  auto data = std::to_array<TestData>({
      {1, "a"},
      {2, "ab"},
  });

  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  std::vector<std::unique_ptr<SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const SessionCommand::size_type big_size =
      CommandStorageBackend::kFileReadBufferSize + 100;
  const SessionCommand::id_type big_id = 50;
  std::unique_ptr<SessionCommand> big_command =
      std::make_unique<SessionCommand>(big_id, big_size);
  big_command->contents()[0] = 'a';
  big_command->contents()[big_size - 1] = 'z';
  commands.push_back(std::move(big_command));
  commands.push_back(CreateCommandFromData(data[1]));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = nullptr;
  backend = CreateBackend();

  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[2].get());

  EXPECT_EQ(big_id, commands[1]->id());
  ASSERT_EQ(big_size, commands[1]->size());
  EXPECT_EQ('a', commands[1]->contents()[0]);
  EXPECT_EQ('z', commands[1]->contents()[big_size - 1]);
}

TEST_P(CommandStorageBackendParamTest, MarkerOnly) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

TEST_P(CommandStorageBackendParamTest, AppendCommandsTwice) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto data = std::to_array<TestData>({
      {1, "a"},
      {2, "bc"},
      {3, "def"},
  });

  // Write the first command.
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data[0]));
  bool write_error = false;
  backend->AppendCommands(
      std::move(commands), /*truncate=*/true,
      base::BindLambdaForTesting([&write_error]() { write_error = true; }));
  EXPECT_FALSE(write_error);

  // Append the next two commands to the same file.
  commands.clear();
  commands.push_back(CreateCommandFromData(data[1]));
  commands.push_back(CreateCommandFromData(data[2]));
  backend->AppendCommands(
      std::move(commands), /*truncate=*/false,
      base::BindLambdaForTesting([&write_error]() { write_error = true; }));
  EXPECT_FALSE(write_error);

  // Read it back in and verify all 3 commands are present.
  backend->MoveCurrentSessionToLastSession();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[1].get());
  AssertCommandEqualsData(data[2], commands[2].get());
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_P(CommandStorageBackendParamTest, Truncate) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.clear();
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
}

std::unique_ptr<SessionCommand> CreateCommandWithMaxSize() {
  const size_type max_size_value = std::numeric_limits<size_type>::max();
  std::unique_ptr<SessionCommand> command =
      std::make_unique<SessionCommand>(11, max_size_value);
  for (int i = 0; i < max_size_value; ++i) {
    command->contents()[i] = i;
  }
  return command;
}

TEST_P(CommandStorageBackendParamTest, MaxSizeType) {
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
  EXPECT_EQ(commands[0]->contents(),
            expected_command->contents().first(expected_size));
}

TEST_P(CommandStorageBackendParamTest, IsValidFileWithInvalidFiles) {
  const auto file_path = sessions_dir().AppendASCII("Session_123");
  base::WriteFile(file_path, "z");  // invalid file contents
  EXPECT_FALSE(CommandStorageBackend::IsValidFileForTest(file_path));

  base::WriteFile(file_path, "a longer string that does not match header");
  EXPECT_FALSE(CommandStorageBackend::IsValidFileForTest(file_path));
}

TEST_P(CommandStorageBackendParamTest, IsNotValidFileWithoutMarker) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  const auto path = backend->current_path_for_testing();
  backend->AppendCommands({}, true, base::DoNothing());
  backend = nullptr;

  EXPECT_FALSE(CommandStorageBackend::IsValidFileForTest(path));
}

TEST_P(CommandStorageBackendParamTest, DeleteLastSession) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = CreateBackend();  // Necessary to recognize the file we just wrote.
  backend->DeleteLastSession();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());

  // Also confirm deletion with a new backend.
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_TRUE(commands.empty());
}

TEST_P(CommandStorageBackendParamTest, ReadEmptyCommands) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  backend->MoveCurrentSessionToLastSession();

  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(0U, commands.size());
}

// Test parsing the timestamp of a session from the path.
TEST_P(CommandStorageBackendParamTest, TimestampFromPath) {
  // Test parsing the timestamp from a valid session.
  const auto test_path_1 = sessions_dir().Append(FILE_PATH_LITERAL("Tabs_0"));
  base::Time result_time_1;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_1, result_time_1));
  EXPECT_EQ(base::Time(), result_time_1);

  const auto test_path_2 =
      sessions_dir().Append(FILE_PATH_LITERAL("Session_13234316721694577"));
  base::Time result_time_2;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_2, result_time_2));
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(13234316721694577)),
            result_time_2);

  // Test attempting to parse invalid file names.
  const auto invalid_path_1 = sessions_dir().AppendASCII("Session_nonsense");
  base::Time invalid_result_1;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_1,
                                                        invalid_result_1));

  const auto invalid_path_2 = sessions_dir().AppendASCII("Arbitrary");
  base::Time invalid_result_2;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_2,
                                                        invalid_result_2));
}

// Test serializing a timestamp to string.
TEST_P(CommandStorageBackendParamTest, FilePathFromTime) {
  const SessionType session_type = GetParam().session_type;
  const auto result_path_1 = FilePathFromTime(session_type, init_path(), 0);
  const auto result_path_2 =
      FilePathFromTime(session_type, init_path(), 13234316721694577);

  switch (session_type) {
    case SessionType::kAppRestore:
      EXPECT_EQ(sessions_dir().AppendASCII("Apps_0"), result_path_1);
      EXPECT_EQ(sessions_dir().AppendASCII("Apps_13234316721694577"),
                result_path_2);
      break;
    case SessionType::kTabRestore:
      EXPECT_EQ(sessions_dir().AppendASCII("Tabs_0"), result_path_1);
      EXPECT_EQ(sessions_dir().AppendASCII("Tabs_13234316721694577"),
                result_path_2);
      break;
    case SessionType::kSessionRestore:
      EXPECT_EQ(sessions_dir().AppendASCII("Session_0"), result_path_1);
      EXPECT_EQ(sessions_dir().AppendASCII("Session_13234316721694577"),
                result_path_2);
      break;
  }
}

// Test that the previous session is empty if no session files exist.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionEmpty) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  ASSERT_FALSE(GetLastSessionInfo(backend.get()));
}

// Test that the previous session is selected correctly when a file is present.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionSingle) {
  const auto prev_path =
      FilePathFromTime(GetParam().session_type, init_path(), 13235178308836991);
  ASSERT_TRUE(base::CreateDirectory(sessions_dir()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_TRUE(last_session_info);
  ASSERT_EQ(prev_path, last_session_info->path);
}

// Test that the previous session is selected correctly when multiple session
// files are present.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionMultiple) {
  SessionType session_type = GetParam().session_type;
  const auto prev_path =
      FilePathFromTime(session_type, init_path(), 13235178308836991);
  const auto old_path_1 =
      FilePathFromTime(session_type, init_path(), 13235178308548874);
  const auto old_path_2 = FilePathFromTime(session_type, init_path(), 0);
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));
  ASSERT_TRUE(base::WriteFile(old_path_1, ""));
  ASSERT_TRUE(base::WriteFile(old_path_2, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_TRUE(last_session_info);
  ASSERT_EQ(prev_path, last_session_info->path);
}

// Tests that MoveCurrentSessionToLastSession deletes the last session file.
TEST_P(CommandStorageBackendParamTest,
       MoveCurrentSessionToLastDeletesLastSession) {
  const auto last_session =
      FilePathFromTime(GetParam().session_type, init_path(), 13235178308836991);
  ASSERT_TRUE(base::CreateDirectory(last_session.DirName()));
  ASSERT_TRUE(base::WriteFile(last_session, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  char buffer[1];
  ASSERT_EQ(0, base::ReadFile(last_session, buffer, 0));
  backend->MoveCurrentSessionToLastSession();
  ASSERT_EQ(-1, base::ReadFile(last_session, buffer, 0));
}

TEST_P(CommandStorageBackendParamTest,
       GetSessionFilePathsAreSortedByReverseTimestamp) {
  SessionType session_type = GetParam().session_type;
  auto path_130 = FilePathFromTime(session_type, init_path(), 130);
  auto path_120 = FilePathFromTime(session_type, init_path(), 120);
  auto path_125 = FilePathFromTime(session_type, init_path(), 125);
  auto path_128 = FilePathFromTime(session_type, init_path(), 128);
  ASSERT_TRUE(base::WriteFile(path_130, ""));
  ASSERT_TRUE(base::WriteFile(path_120, ""));
  ASSERT_TRUE(base::WriteFile(path_125, ""));
  ASSERT_TRUE(base::WriteFile(path_128, ""));

  auto paths = GetSessionFilePathsSortedByReverseTimestamp(session_type);
  ASSERT_EQ(4u, paths.size());
  EXPECT_EQ(path_130, paths[0]);
  EXPECT_EQ(path_128, paths[1]);
  EXPECT_EQ(path_125, paths[2]);
  EXPECT_EQ(path_120, paths[3]);
}

TEST_P(CommandStorageBackendParamTest, UseMarkerWithoutValidMarker) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;
  // There should be no commands as a valid marker was not written.
  ASSERT_TRUE(commands.empty());

  // As there was no valid marker, there should be no last session file.
  EXPECT_FALSE(GetLastSessionInfo(backend.get()));
}

TEST_P(CommandStorageBackendParamTest, NewFileOnTruncate) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path1 = backend->current_path_for_testing();

  // Path shouldn't change if truncate is false.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), false, base::DoNothing());
  EXPECT_EQ(path1, backend->current_path_for_testing());

  // Path should change on truncate, and `path1` should not be removed.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path2 = backend->current_path_for_testing();
  EXPECT_TRUE(!path2.empty());
  EXPECT_NE(path1, path2);
  EXPECT_TRUE(base::PathExists(path1));
  EXPECT_TRUE(base::PathExists(path2));

  // Repeat. This time `path1` should be removed.
  commands.clear();
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path3 = backend->current_path_for_testing();
  EXPECT_TRUE(!path3.empty());
  EXPECT_NE(path1, path3);
  EXPECT_NE(path2, path3);
  EXPECT_FALSE(base::PathExists(path1));
  EXPECT_TRUE(base::PathExists(path2));
  EXPECT_TRUE(base::PathExists(path3));
}

TEST_P(CommandStorageBackendParamTest, AppendCommandsCallbackRunOnError) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->ForceAppendCommandsToFailForTesting();
  base::RunLoop run_loop;
  backend->AppendCommands({}, true, run_loop.QuitClosure());
  run_loop.Run();
}

TEST_P(CommandStorageBackendParamTest, RestoresFileWithMarkerAfterFailure) {
  // Write `data` and a marker.
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {11, "X"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  EXPECT_TRUE(backend->IsFileOpenForTesting());

  // Make appending fail, which should close the file.
  backend->ForceAppendCommandsToFailForTesting();
  backend->AppendCommands({}, false, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpenForTesting());

  // Append again, with another fail. Should attempt to reopen file and file.
  backend->ForceAppendCommandsToFailForTesting();
  backend->AppendCommands({}, true, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpenForTesting());

  // Reopen and read last session. Should get `data` and marker.
  backend = nullptr;
  backend = CreateBackend();
  backend->AppendCommands({}, false, base::DoNothing());
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(1u, commands.size());
  AssertCommandEqualsData(data, commands[0].get());
}

TEST_P(CommandStorageBackendParamTest, PathTimeIncreases) {
  base::SimpleTestClock test_clock;
  test_clock.SetNow(base::Time::Now());
  scoped_refptr<CommandStorageBackend> backend = CreateBackend(&test_clock);
  // Write `data` and a marker.
  struct TestData data = {11, "X"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  const base::FilePath path1 = backend->current_path_for_testing();
  EXPECT_FALSE(path1.empty());
  base::Time path1_time;
  EXPECT_TRUE(CommandStorageBackend::TimestampFromPath(path1, path1_time));

  test_clock.Advance(base::Seconds(-1));
  SessionCommands commands2;
  commands2.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands2), true, base::DoNothing());
  const base::FilePath path2 = backend->current_path_for_testing();
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

std::string TestParamNameGenerator(
    const testing::TestParamInfo<TestParams>& param_info) {
  switch (param_info.param.session_type) {
    case SessionType::kAppRestore:
      return "AppRestore";
    case SessionType::kSessionRestore:
      return "SessionRestore";
    case SessionType::kTabRestore:
      return "TabRestore";
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CommandStorageBackendParamTest,
    ::testing::Values(TestParams(SessionType::kAppRestore),
                      TestParams(SessionType::kSessionRestore),
                      TestParams(SessionType::kTabRestore)),
    TestParamNameGenerator);

}  // namespace sessions
