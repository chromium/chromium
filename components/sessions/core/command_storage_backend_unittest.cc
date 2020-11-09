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
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/sessions/core/command_storage_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::MakeRefCounted;

using size_type = sessions::SessionCommand::size_type;
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

class CommandStorageBackendTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    path_ = temp_dir_.GetPath().Append(FILE_PATH_LITERAL("Session"));
  }

  void AssertCommandEqualsData(const TestData& data,
                               sessions::SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->size());
    EXPECT_TRUE(
        memcmp(command->contents(), data.data.c_str(), command->size()) == 0);
  }

  scoped_refptr<CommandStorageBackend> CreateBackend() {
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), path_);
  }

  base::test::TaskEnvironment task_environment_;
  base::FilePath path_;
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
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands;

  commands.push_back(CreateCommandFromData(data[0]));
  const sessions::SessionCommand::size_type big_size =
      CommandStorageBackend::kFileReadBufferSize + 100;
  const sessions::SessionCommand::id_type big_id = 50;
  std::unique_ptr<sessions::SessionCommand> big_command =
      std::make_unique<sessions::SessionCommand>(big_id, big_size);
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
  std::vector<std::unique_ptr<sessions::SessionCommand>> commands =
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
  base::WriteFile(path_, "z");
  EXPECT_FALSE(CommandStorageBackend::IsValidFile(path_));

  base::WriteFile(path_, "a longer string that does not match header");
  EXPECT_FALSE(CommandStorageBackend::IsValidFile(path_));
}

TEST_F(CommandStorageBackendTest, IsValidFileWithValidFile) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->AppendCommands({}, true);
  backend = nullptr;

  EXPECT_TRUE(CommandStorageBackend::IsValidFile(path_));
}

}  // namespace sessions
