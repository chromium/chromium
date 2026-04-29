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
#include "base/numerics/byte_conversions.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/test/test_file_util.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
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
  bool encrypted;
};

}  // namespace

class CommandStorageBackendTest : public testing::Test {
 public:
  CommandStorageBackendTest() : os_crypt_(CreateOSCryptAsync()) {}

 protected:
  using ReadStatus = CommandStorageBackend::ReadStatus;
  using WriteStatus = CommandStorageBackend::WriteStatus;
  using ReadCommandsResult = CommandStorageBackend::ReadCommandsResult;

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    init_path_ = temp_dir_.GetPath();
    sessions_dir_ = init_path_.Append(kSessionsDirectory);
    encrypted_sessions_dir_ = init_path_.Append(kEncryptedSessionsDirectory);
    base::CreateDirectory(sessions_dir_);
    base::CreateDirectory(encrypted_sessions_dir_);
  }

  void AssertCommandEqualsData(const TestData& data,
                               const SessionCommand* command) {
    EXPECT_EQ(data.command_id, command->id());
    EXPECT_EQ(data.data.size(), command->contents().size());
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
      bool encrypted,
      base::Clock* clock = nullptr) {
    std::unique_ptr<os_crypt_async::Encryptor> encryptor;
    if (encrypted) {
      base::RunLoop run_loop;
      os_crypt_->GetInstance(
          base::BindLambdaForTesting([&](os_crypt_async::Encryptor e) {
            encryptor =
                std::make_unique<os_crypt_async::Encryptor>(std::move(e));
            run_loop.Quit();
          }));
      run_loop.Run();
    }
    return MakeRefCounted<CommandStorageBackend>(
        task_environment_.GetMainThreadTaskRunner(), init_path_, session_type,
        std::move(encryptor), clock);
  }

  // Functions that call into private members of CommandStorageBackend.
  std::optional<CommandStorageBackend::SessionInfo> GetLastSessionInfo(
      CommandStorageBackend* backend) {
    // Force `last_session_info_` to be updated.
    backend->InitIfNecessary();
    return backend->last_session_info_;
  }

  std::vector<base::FilePath> GetSessionFilePathsSortedByReverseTimestamp(
      SessionType session_type,
      bool encrypted) {
    auto infos = CommandStorageBackend::GetSessionFilesSortedByReverseTimestamp(
        init_path_, session_type, encrypted);
    std::vector<base::FilePath> result;
    for (const auto& info : infos) {
      result.push_back(info.path);
    }
    return result;
  }

  void ForceAppendCommandsToFail(CommandStorageBackend* backend,
                                 WriteStatus status) {
    backend->ForceAppendCommandsToFailForTesting(status);
  }

  // Helper for calling CommandStorageBackend::GetFilePath() with a time
  // delta in microseconds.
  static base::FilePath GetFilePath(SessionType type,
                                    const base::FilePath& path,
                                    uint64_t time_delta_microseconds,
                                    bool encrypted) {
    return CommandStorageBackend::GetFilePath(
        type, path,
        base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(time_delta_microseconds)),
        encrypted);
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

  bool CopyTestDataToSessionFile(const std::string& test_data_filename,
                                 const std::string& session_filename,
                                 bool encrypted) {
    base::FilePath test_file_path = GetTestFilePath(test_data_filename);
    if (!base::PathExists(test_file_path)) {
      return false;
    }
    return base::CopyFile(
        test_file_path, sessions_dir(encrypted).AppendASCII(session_filename));
  }

  std::string GetHistogramName(SessionType type,
                               bool encrypted,
                               std::string_view operation,
                               std::string_view slice,
                               std::string_view metric) {
    return CommandStorageBackend::GetHistogramNameForTesting(
        type, encrypted, operation, slice, metric);
  }

  // The path that is passed to `CreateBackend`.
  const base::FilePath& init_path() const { return init_path_; }

  // The path to the directory that contains the session files.
  const base::FilePath& sessions_dir(bool encrypted) const {
    return encrypted ? encrypted_sessions_dir_ : sessions_dir_;
  }

  const std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_;

 private:
  // Creates an OSCryptAsync that uses a predefined key.
  std::unique_ptr<os_crypt_async::OSCryptAsync> CreateOSCryptAsync() {
    std::string key_string;
    CHECK(
        base::ReadFileToString(GetTestFilePath("OSCryptKey-k1"), &key_string));
    std::vector<uint8_t> key_data(key_string.begin(), key_string.end());
    std::vector<std::pair<size_t, std::unique_ptr<os_crypt_async::KeyProvider>>>
        providers;
    providers.emplace_back(/*precedence=*/10u,
                           std::make_unique<TestKeyProvider>("k1", key_data));
    return std::make_unique<os_crypt_async::OSCryptAsync>(std::move(providers));
  }

  // A test key provider that takes a fixed key.
  // TODO(crbug.com/492549013): Refactor to share code with other tests.
  class TestKeyProvider : public os_crypt_async::KeyProvider {
   public:
    explicit TestKeyProvider(const std::string& tag,
                             base::span<const uint8_t> key)
        : tag_(tag), key_(key.begin(), key.end()) {}

   private:
    void GetKey(KeyCallback callback) final {
      std::move(callback).Run(
          tag_, os_crypt_async::Encryptor::Key(
                    key_, os_crypt_async::mojom::Algorithm::kAES256GCM));
    }

    bool UseForEncryption() final { return true; }
    bool IsCompatibleWithOsCryptSync() final { return false; }

    const std::string tag_;
    const std::vector<uint8_t> key_;
  };

  base::test::TaskEnvironment task_environment_;
  base::FilePath init_path_;  // Passed to CommandStorageBackend constructor.
  // The directory containing the cleartext session files.
  base::FilePath sessions_dir_;
  // The directory containing the encrypted session files.
  base::FilePath encrypted_sessions_dir_;
  base::ScopedTempDir temp_dir_;
};

// Most tests are parameterized to run for each session type (see
// Non-parameterized tests for CommandStorageBackend.
// `CommandStorageBackendParamTest` below).  Test that do not use parameters
// are grouped here into `CommandStorageBackendTest`.

TEST_F(CommandStorageBackendTest, ReadSessionFileV1) {
  // V1 files do not contain markers.
  // They were used in production prior to commit 223e5cd on 2021-05-25.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      CopyTestDataToSessionFile("Session-v1NoMarker", "Session_1234", false));

  // V1 files are no longer supported.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ASSERT_FALSE(backend->IsValidFileForTest(
      sessions_dir(false).AppendASCII("Session_1234")));
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_TRUE(result.error_reading);
  ASSERT_TRUE(result.commands.empty());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kUnsupportedVersion, 1);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV2) {
  // V2 files are encrypted and do not contain markers.
  // They could have been written prior to commit 223e5cd on 2021-05-25.
  // They were never used in production.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v2NoMarkerEncrypted",
                                        "Session_1234", false));

  // V2 files are no longer supported.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ASSERT_FALSE(backend->IsValidFileForTest(
      sessions_dir(false).AppendASCII("Session_1234")));
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_TRUE(result.commands.empty());
  ASSERT_TRUE(result.error_reading);
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kUnsupportedVersion, 1);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV3) {
  // V3 files contain markers.
  // They have been used in production from early 2021 through at least
  // 2026-02.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(
      CopyTestDataToSessionFile("Session-v3WithMarker", "Session_1234", false));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_EQ(1u, result.commands.size());
  ASSERT_FALSE(result.error_reading);
  AssertCommandEqualsData(TestData({1, "a"}), result.commands[0].get());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kSuccess, 1);
}

TEST_F(CommandStorageBackendTest, WriteSessionFileV3) {
  // This test ensures that we don't accidentally change the format of V3
  // files. If you intend to change the output file format, then you should
  // create a new test data file, and update this test to read that new file.
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  bool write_error_occurred = false;
  backend->AppendCommands(std::move(commands), true,
                          base::BindLambdaForTesting([&write_error_occurred]() {
                            write_error_occurred = true;
                          }));
  const base::FilePath written_path = backend->current_path_for_testing();

  // Ensure that the file is fully written and contains the expected data.
  base::SequencedTaskRunner* task_runner = backend->owning_task_runner();
  backend.reset();
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_FALSE(write_error_occurred);
  const base::FilePath expected_data_path =
      GetTestFilePath("Session-v3WithMarker");
  base::MemoryMappedFile written_file;
  ASSERT_TRUE(written_file.Initialize(written_path));
  base::MemoryMappedFile expected_data_file;
  ASSERT_TRUE(expected_data_file.Initialize(expected_data_path));
  ASSERT_EQ(expected_data_file.length(), written_file.length());
  ASSERT_EQ(expected_data_file.bytes(), written_file.bytes());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext.AppendCommands."
      "Truncate.Status",
      WriteStatus::kSuccess, 1);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV3With2Appends) {
  // V3 files contain markers.
  // They have been used in production from early 2021 through at least
  // 2026-02.
  // This test file was created using 2 calls to backend->AppendCommands,
  // which results in the Marker being in the middle of the file (between
  // the "banana" and "coconut" commands).  See also related test
  // `WriteSessionFileV3With2Appends`.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v3With2Appends",
                                        "Session_1234", false));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_FALSE(result.error_reading);
  ASSERT_EQ(4u, result.commands.size());
  AssertCommandEqualsData(TestData({1, "apple"}), result.commands[0].get());
  AssertCommandEqualsData(TestData({2, "banana"}), result.commands[1].get());
  AssertCommandEqualsData(TestData({3, "coconut"}), result.commands[2].get());
  AssertCommandEqualsData(TestData({4, "durian"}), result.commands[3].get());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kSuccess, 1);
}

TEST_F(CommandStorageBackendTest, WriteSessionFileV3With2Appends) {
  // This test ensures that we don't accidentally change the format of V3
  // files. If you intend to change the output file format, then you should
  // create a new test data file, and update this test to read that new file.
  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
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

TEST_F(CommandStorageBackendTest, ReadSessionFileV3WithEncryptorAvailable) {
  base::HistogramTester histogram_tester;
  // This is unexpected as the encrypted sessions directory will typically only
  // contain encrypted files. But it's supported for backward compatibility.
  ASSERT_TRUE(
      CopyTestDataToSessionFile("Session-v3WithMarker", "Session_1234", true));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/true);
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_EQ(1u, result.commands.size());
  ASSERT_FALSE(result.error_reading);
  AssertCommandEqualsData(TestData({1, "a"}), result.commands[0].get());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Encrypted."
      "ReadLastSessionCommands.Status",
      ReadStatus::kSuccess, 1);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV4) {
  // V4 files contain markers and are encrypted.
  // They have never been used in production, but could have been written from
  // early 2021 through at least 2026-02.  They are no longer supported.
  base::HistogramTester histogram_tester;
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v4WithMarkerEncrypted",
                                        "Session_1234", false));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ASSERT_FALSE(backend->IsValidFileForTest(
      sessions_dir(false).AppendASCII("Session_1234")));
  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_TRUE(result.error_reading);
  ASSERT_TRUE(result.commands.empty());
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kUnsupportedVersion, 1);
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV5) {
  // V5 files contain encrypted commands.
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v5", "Session_1234", true));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/true);
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_EQ(1u, result.commands.size());
  AssertCommandEqualsData(TestData({1, "a"}), result.commands[0].get());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV5With2Appends) {
  // V5 files contain encrypted commands.
  // This test file was created using 2 calls to backend->AppendCommands,
  // which results in the Marker being in the middle of the file (between
  // the "banana" and "coconut" commands).  See also related test
  // `WriteSessionFileV3With2Appends`.
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v5With2Appends",
                                        "Session_1234", true));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/true);
  SessionCommands commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(4u, commands.size());
  AssertCommandEqualsData(TestData({1, "apple"}), commands[0].get());
  AssertCommandEqualsData(TestData({2, "banana"}), commands[1].get());
  AssertCommandEqualsData(TestData({3, "coconut"}), commands[2].get());
  AssertCommandEqualsData(TestData({4, "durian"}), commands[3].get());
}

TEST_F(CommandStorageBackendTest, ReadSessionFileV5FailsWithoutEncryptor) {
  // Encrypted V5 files cannot be read if an encryptor is not present.
  base::HistogramTester histogram_tester;
  // This is unexpected as the cleartext sessions directory will typically only
  // contain cleartext files.
  ASSERT_TRUE(CopyTestDataToSessionFile("Session-v5", "Session_1234", false));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore, /*encrypted=*/false);
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_TRUE(result.commands.empty());
  ASSERT_TRUE(result.error_reading);
  histogram_tester.ExpectUniqueSample(
      "Session.CommandStorageBackend.SessionRestore.Cleartext."
      "ReadLastSessionCommands.Status",
      ReadStatus::kUnsupportedVersion, 1);
}

TEST_F(CommandStorageBackendTest, GetHistogramName) {
  // An error when AppendCommands fails for a SessionRestore, Cleartext
  // backend.
  EXPECT_EQ(GetHistogramName(SessionType::kSessionRestore, /*encrypted=*/false,
                             "AppendCommands", "Truncate", "Status"),
            "Session.CommandStorageBackend.SessionRestore.Cleartext."
            "AppendCommands.Truncate.Status");

  // Different encryption (Encrypted vs Cleartext)
  EXPECT_EQ(GetHistogramName(SessionType::kSessionRestore, /*encrypted=*/true,
                             "AppendCommands", "Truncate", "Status"),
            "Session.CommandStorageBackend.SessionRestore.Encrypted."
            "AppendCommands.Truncate.Status");

  // Different metric (Duration vs Status)
  EXPECT_EQ(GetHistogramName(SessionType::kSessionRestore, /*encrypted=*/false,
                             "AppendCommands", "Truncate", "Duration"),
            "Session.CommandStorageBackend.SessionRestore.Cleartext."
            "AppendCommands.Truncate.Duration");

  // Different session type (AppRestore vs SessionRestore)
  EXPECT_EQ(GetHistogramName(SessionType::kAppRestore, /*encrypted=*/false,
                             "AppendCommands", "Truncate", "Status"),
            "Session.CommandStorageBackend.AppRestore.Cleartext."
            "AppendCommands.Truncate.Status");

  // Different slice (Append vs Truncate)
  EXPECT_EQ(
      GetHistogramName(SessionType::kSessionRestore, /*encrypted=*/false,
                       "AppendCommands", "Append", "Status"),
      "Session.CommandStorageBackend.SessionRestore.Cleartext.AppendCommands."
      "Append.Status");

  // Different operation (ReadLastSessionCommands vs AppendCommands)
  EXPECT_EQ(GetHistogramName(SessionType::kSessionRestore, /*encrypted=*/false,
                             "ReadLastSessionCommands", "", "Status"),
            "Session.CommandStorageBackend.SessionRestore.Cleartext."
            "ReadLastSessionCommands.Status");
}

// Parameterized tests for CommandStorageBackend.
class CommandStorageBackendParamTest
    : public CommandStorageBackendTest,
      public testing::WithParamInterface<TestParams> {
 protected:
  scoped_refptr<CommandStorageBackend> CreateBackend() {
    return CommandStorageBackendTest::CreateBackend(GetParam().session_type,
                                                    GetParam().encrypted);
  }
  scoped_refptr<CommandStorageBackend> CreateBackend(base::Clock* clock) {
    return CommandStorageBackendTest::CreateBackend(
        GetParam().session_type, GetParam().encrypted, clock);
  }
  scoped_refptr<CommandStorageBackend> CreateBackend(SessionType session_type) {
    return CommandStorageBackendTest::CreateBackend(session_type,
                                                    GetParam().encrypted);
  }

  base::FilePath GetFilePath(uint64_t time_delta_microseconds) {
    return CommandStorageBackendTest::GetFilePath(
        GetParam().session_type, init_path(), time_delta_microseconds,
        GetParam().encrypted);
  }

  std::string GetHistogramName(std::string_view operation,
                               std::string_view slice,
                               std::string_view metric) {
    return CommandStorageBackendTest::GetHistogramName(
        GetParam().session_type, GetParam().encrypted, operation, slice,
        metric);
  }
};

TEST_P(CommandStorageBackendParamTest, SimpleWrite) {
  base::HistogramTester histogram_tester;
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
  bool write_error_occurred = false;

  backend->AppendCommands(std::move(commands), true,
                          base::BindLambdaForTesting([&write_error_occurred]() {
                            write_error_occurred = true;
                          }));

  EXPECT_FALSE(write_error_occurred);
  const base::FilePath path = backend->current_path_for_testing();
  EXPECT_TRUE(base::PathExists(path));
  base::FilePath expected_path = GetFilePath(13234316721694577);
  EXPECT_EQ(expected_path, path);
  EXPECT_GT(base::GetFileSize(path), 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("AppendCommands", "Truncate", "Status"),
      WriteStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Duration"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "FileSize"), 1);
}

TEST_P(CommandStorageBackendParamTest, SimpleReadWrite) {
  base::HistogramTester histogram_tester;
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

  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
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
  ASSERT_EQ(big_size, commands[1]->contents().size());
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
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto data = std::to_array<TestData>({
      {1, "a"},
      {2, "bc"},
      {3, "def"},
  });

  // Write the first command.
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data[0]));
  bool write_error_occurred = false;
  backend->AppendCommands(std::move(commands), /*truncate=*/true,
                          base::BindLambdaForTesting([&write_error_occurred]() {
                            write_error_occurred = true;
                          }));
  EXPECT_FALSE(write_error_occurred);

  // Append the next two commands to the same file.
  commands.clear();
  commands.push_back(CreateCommandFromData(data[1]));
  commands.push_back(CreateCommandFromData(data[2]));
  backend->AppendCommands(std::move(commands), /*truncate=*/false,
                          base::BindLambdaForTesting([&write_error_occurred]() {
                            write_error_occurred = true;
                          }));
  EXPECT_FALSE(write_error_occurred);

  // Read it back in and verify all 3 commands are present.
  backend->MoveCurrentSessionToLastSession();
  commands = backend->ReadLastSessionCommands().commands;
  ASSERT_EQ(3U, commands.size());
  AssertCommandEqualsData(data[0], commands[0].get());
  AssertCommandEqualsData(data[1], commands[1].get());
  AssertCommandEqualsData(data[2], commands[2].get());
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Status"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Duration"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "FileSize"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Append", "Status"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Append", "Duration"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Append", "FileSize"), 1);
}

// Writes a command, appends another command with reset to true, then reads
// making sure we only get back the second command.
TEST_P(CommandStorageBackendParamTest, Truncate) {
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData first_data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(first_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Status"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Duration"), 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "FileSize"), 1);

  // Write another command, this time resetting the file when appending.
  struct TestData second_data = {2, "b"};
  commands.clear();
  commands.push_back(CreateCommandFromData(second_data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Status"), 2);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "Duration"), 2);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("AppendCommands", "Truncate", "FileSize"), 2);

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;

  // And make sure we get back the expected data.
  ASSERT_EQ(1U, commands.size());
  AssertCommandEqualsData(second_data, commands[0].get());
}

std::unique_ptr<SessionCommand> CreateCommandWithSize(size_type size) {
  std::unique_ptr<SessionCommand> command =
      std::make_unique<SessionCommand>(11, size);
  for (int i = 0; i < size; ++i) {
    command->contents()[i] = i;
  }
  return command;
}

TEST_P(CommandStorageBackendParamTest, MaximumSizeCommandContents) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  commands.push_back(CreateCommandWithSize(SessionCommand::kMaxContentSize));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(1U, commands.size());
  ASSERT_EQ(SessionCommand::kMaxContentSize, (commands[0])->contents().size());
  auto expected_command =
      CreateCommandWithSize(SessionCommand::kMaxContentSize);
  EXPECT_EQ(expected_command->id(), (commands[0])->id());
  // No truncation should occur, so the contents should be the same.
  EXPECT_EQ(expected_command->contents(), (commands[0])->contents());
}

TEST_P(CommandStorageBackendParamTest, CommandContentsExceedMaximum) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  size_type exceeding_size =
      std::numeric_limits<SessionCommand::size_type>::max();
  ASSERT_GT(exceeding_size, SessionCommand::kMaxContentSize);
  commands.push_back(CreateCommandWithSize(exceeding_size));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  // Read it back in.
  backend = nullptr;
  backend = CreateBackend();
  commands = backend->ReadLastSessionCommands().commands;

  ASSERT_EQ(1U, commands.size());
  // The command should be truncated to the maximum size but otherwise the
  // same.
  ASSERT_EQ(SessionCommand::kMaxContentSize, (commands[0])->contents().size());
  auto expected_command = CreateCommandWithSize(exceeding_size);
  EXPECT_EQ(expected_command->id(), (commands[0])->id());
  EXPECT_EQ(expected_command->contents().first(SessionCommand::kMaxContentSize),
            (commands[0])->contents());
}

TEST_P(CommandStorageBackendParamTest, GetSessionFiles) {
  bool encrypted = GetParam().encrypted;
  SessionType type = GetParam().session_type;

  EXPECT_TRUE(
      GetSessionFilePathsSortedByReverseTimestamp(type, encrypted).empty());

  base::FilePath valid_path = GetFilePath(124);
  std::string valid_name = valid_path.BaseName().MaybeAsASCII();

  // Not a valid name, as doesn't contain timestamp separator.
  std::string invalid_name = valid_name;
  std::replace(invalid_name.begin(), invalid_name.end(), '_', ' ');
  ASSERT_TRUE(
      base::WriteFile(sessions_dir(encrypted).AppendASCII(invalid_name), ""));

  // Valid name.
  ASSERT_TRUE(base::WriteFile(valid_path, ""));

  // Valid name, but should not be returned as "Foo" doesn't match.
  ASSERT_TRUE(
      base::WriteFile(sessions_dir(encrypted).AppendASCII("Foo_125"), ""));

  auto paths = GetSessionFilePathsSortedByReverseTimestamp(type, encrypted);
  ASSERT_EQ(1u, paths.size());
  EXPECT_EQ(valid_name, paths.begin()->BaseName().MaybeAsASCII());
}

// Test that the a file with an invalid name won't be used.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionInvalid) {
  if (GetParam().session_type != SessionType::kSessionRestore) {
    GTEST_SKIP() << "This test is only for SessionRestore.";
  }
  bool encrypted = GetParam().encrypted;
  const auto prev_path =
      sessions_dir(encrypted).Append(FILE_PATH_LITERAL("Session_invalid"));
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend =
      CreateBackend(SessionType::kSessionRestore);
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_FALSE(last_session_info);
}

TEST_P(CommandStorageBackendParamTest, IsValidFileWithInvalidFiles) {
  bool encrypted = GetParam().encrypted;
  const auto file_path = sessions_dir(encrypted).AppendASCII("Session_123");
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
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {1, "a"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());

  backend = CreateBackend();  // Necessary to recognize the file we just wrote.
  backend->DeleteLastSession();
  ReadCommandsResult result = backend->ReadLastSessionCommands();
  ASSERT_FALSE(result.error_reading);
  ASSERT_TRUE(result.commands.empty());
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kNoFile, 1);

  // Also confirm deletion with a new backend.
  backend = CreateBackend();
  result = backend->ReadLastSessionCommands();
  ASSERT_FALSE(result.error_reading);
  ASSERT_TRUE(result.commands.empty());
  histogram_tester.ExpectBucketCount(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kNoFile, 2);
}

TEST_P(CommandStorageBackendParamTest, ReadEmptyCommands) {
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  SessionCommands commands;
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  backend->MoveCurrentSessionToLastSession();

  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_FALSE(result.error_reading);
  ASSERT_EQ(0U, result.commands.size());
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
}

TEST_P(CommandStorageBackendParamTest, ReadErrorWithEmptyFile) {
  base::HistogramTester histogram_tester;
  const auto path = GetFilePath(1234);
  ASSERT_TRUE(base::WriteFile(path, ""));
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();

  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_TRUE(result.error_reading);
  ASSERT_EQ(0U, result.commands.size());
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kFileEmpty, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
}

TEST_P(CommandStorageBackendParamTest, ReadErrorWithInvalidHeader) {
  base::HistogramTester histogram_tester;
  const auto path = GetFilePath(1234);
  const char kInvalidHeader[] = "INVALID_HEADER";
  ASSERT_TRUE(base::WriteFile(path, kInvalidHeader));
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();

  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_TRUE(result.error_reading);
  ASSERT_EQ(0U, result.commands.size());
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kInvalidHeader, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
}

TEST_P(CommandStorageBackendParamTest, ReadErrorWithCommandSizeZero) {
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->AppendCommands({}, true, base::DoNothing());
  const base::FilePath path = backend->current_path_for_testing();
  ASSERT_TRUE(base::PathExists(path));

  // Close the file before appending to it, which is required on Windows.
  base::SequencedTaskRunner* task_runner = backend->owning_task_runner();
  backend.reset();
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Write an invalid command (size_field_value = 0).
  uint8_t bad_command_data[] = {0u, 0u};
  ASSERT_TRUE(base::AppendToFile(path, bad_command_data));

  backend = CreateBackend();
  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_TRUE(result.error_reading);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kInvalidCommand, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
}

TEST_P(CommandStorageBackendParamTest, ReadErrorWithIncompleteCommand) {
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  backend->AppendCommands({}, true, base::DoNothing());
  const base::FilePath path = backend->current_path_for_testing();
  ASSERT_TRUE(base::PathExists(path));

  // Close the file before appending to it, which is required on Windows.
  base::SequencedTaskRunner* task_runner = backend->owning_task_runner();
  backend.reset();
  base::RunLoop run_loop;
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();

  // Write a bad command (2 bytes indicating a size of 10, but no more data).
  uint8_t bad_command_data[] = {10u, 0u};
  ASSERT_TRUE(base::AppendToFile(path, bad_command_data));

  // Reading it back should result in an error.
  backend = CreateBackend();
  ReadCommandsResult result = backend->ReadLastSessionCommands();

  ASSERT_TRUE(result.error_reading);
  ASSERT_EQ(0U, result.commands.size());
  histogram_tester.ExpectUniqueSample(
      GetHistogramName("ReadLastSessionCommands", "", "Status"),
      ReadStatus::kInvalidCommand, 1);
  histogram_tester.ExpectTotalCount(
      GetHistogramName("ReadLastSessionCommands", "", "Duration"), 1);
}

// Test parsing the timestamp of a session from the path.
TEST_P(CommandStorageBackendParamTest, TimestampFromPath) {
  // Test parsing the timestamp from a valid session.
  bool encrypted = GetParam().encrypted;
  const base::FilePath test_path_1 = GetFilePath(0);
  base::Time result_time_1;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_1, result_time_1));
  EXPECT_EQ(base::Time(), result_time_1);

  const base::FilePath test_path_2 = GetFilePath(13234316721694577);
  base::Time result_time_2;
  EXPECT_TRUE(
      CommandStorageBackend::TimestampFromPath(test_path_2, result_time_2));
  EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                base::Microseconds(13234316721694577)),
            result_time_2);

  // Test attempting to parse invalid file names.
  const auto invalid_path_1 =
      sessions_dir(encrypted).AppendASCII("Session_nonsense");
  base::Time invalid_result_1;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_1,
                                                        invalid_result_1));

  const auto invalid_path_2 = sessions_dir(encrypted).AppendASCII("Arbitrary");
  base::Time invalid_result_2;
  EXPECT_FALSE(CommandStorageBackend::TimestampFromPath(invalid_path_2,
                                                        invalid_result_2));
}

// Test serializing a timestamp to string.
TEST_P(CommandStorageBackendParamTest, GetFilePath) {
  const SessionType session_type = GetParam().session_type;
  bool encrypted = GetParam().encrypted;
  const base::FilePath result_path_1 = GetFilePath(0);
  const base::FilePath result_path_2 = GetFilePath(13234316721694577);

  switch (session_type) {
    case SessionType::kAppRestore:
      EXPECT_EQ(sessions_dir(encrypted).AppendASCII("Apps_0"), result_path_1);
      EXPECT_EQ(sessions_dir(encrypted).AppendASCII("Apps_13234316721694577"),
                result_path_2);
      break;
    case SessionType::kTabRestore:
      EXPECT_EQ(sessions_dir(encrypted).AppendASCII("Tabs_0"), result_path_1);
      EXPECT_EQ(sessions_dir(encrypted).AppendASCII("Tabs_13234316721694577"),
                result_path_2);
      break;
    case SessionType::kSessionRestore:
      EXPECT_EQ(sessions_dir(encrypted).AppendASCII("Session_0"),
                result_path_1);
      EXPECT_EQ(
          sessions_dir(encrypted).AppendASCII("Session_13234316721694577"),
          result_path_2);
      break;
  }
}

// Test that the previous session is empty if no session files exist.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionEmpty) {
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  ASSERT_FALSE(GetLastSessionInfo(backend.get()));
}

// Test that the previous session is selected correctly when a file is
// present.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionSingle) {
  const auto prev_path = GetFilePath(13235178308836991);
  ASSERT_TRUE(base::CreateDirectory(prev_path.DirName()));
  ASSERT_TRUE(base::WriteFile(prev_path, ""));

  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  auto last_session_info = GetLastSessionInfo(backend.get());
  ASSERT_TRUE(last_session_info);
  ASSERT_EQ(prev_path, last_session_info->path);
}

// Test that the previous session is selected correctly when multiple session
// files are present.
TEST_P(CommandStorageBackendParamTest, DeterminePreviousSessionMultiple) {
  base::FilePath prev_path = GetFilePath(13235178308836991);
  base::FilePath old_path_1 = GetFilePath(13235178308548874);
  base::FilePath old_path_2 = GetFilePath(0);
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
  base::FilePath last_session = GetFilePath(13235178308836991);
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
  bool encrypted = GetParam().encrypted;
  base::FilePath path_130 = GetFilePath(130);
  base::FilePath path_120 = GetFilePath(120);
  base::FilePath path_125 = GetFilePath(125);
  base::FilePath path_128 = GetFilePath(128);
  ASSERT_TRUE(base::CreateDirectory(path_130.DirName()));
  ASSERT_TRUE(base::WriteFile(path_130, ""));
  ASSERT_TRUE(base::WriteFile(path_120, ""));
  ASSERT_TRUE(base::WriteFile(path_125, ""));
  ASSERT_TRUE(base::WriteFile(path_128, ""));

  auto paths =
      GetSessionFilePathsSortedByReverseTimestamp(session_type, encrypted);
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
  // GetLastSessionInfo is a private member variable and is not supported
  // when using encryption.
  if (!GetParam().encrypted) {
    EXPECT_FALSE(GetLastSessionInfo(backend.get()));
  }
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
  base::HistogramTester histogram_tester;
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  ForceAppendCommandsToFail(backend.get(), WriteStatus::kSerializationError);
  base::RunLoop run_loop;

  backend->AppendCommands({}, true, run_loop.QuitClosure());
  run_loop.Run();

  histogram_tester.ExpectUniqueSample(
      GetHistogramName("AppendCommands", "Truncate", "Status"),
      WriteStatus::kSerializationError, 1);
}

TEST_P(CommandStorageBackendParamTest, RestoresFileWithMarkerAfterFailure) {
  base::HistogramTester histogram_tester;
  // Write `data` and a marker.
  scoped_refptr<CommandStorageBackend> backend = CreateBackend();
  struct TestData data = {11, "X"};
  SessionCommands commands;
  commands.push_back(CreateCommandFromData(data));
  backend->AppendCommands(std::move(commands), true, base::DoNothing());
  EXPECT_TRUE(backend->IsFileOpenForTesting());

  // Make appending fail, which should close the file.
  ForceAppendCommandsToFail(backend.get(), WriteStatus::kFileWriteError);
  backend->AppendCommands({}, false, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpenForTesting());
  histogram_tester.ExpectBucketCount(
      GetHistogramName("AppendCommands", "Append", "Status"),
      WriteStatus::kFileWriteError, 1);

  // Append again, with another fail. Should attempt to reopen file and file.
  ForceAppendCommandsToFail(backend.get(), WriteStatus::kSerializationError);
  backend->AppendCommands({}, true, base::DoNothing());
  EXPECT_FALSE(backend->IsFileOpenForTesting());
  histogram_tester.ExpectBucketCount(
      GetHistogramName("AppendCommands", "Truncate", "Status"),
      WriteStatus::kSerializationError, 1);

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
  std::string session_type_name;
  switch (param_info.param.session_type) {
    case SessionType::kAppRestore:
      session_type_name = "AppRestore";
      break;
    case SessionType::kSessionRestore:
      session_type_name = "SessionRestore";
      break;
    case SessionType::kTabRestore:
      session_type_name = "TabRestore";
      break;
  }
  std::string encryption_name =
      param_info.param.encrypted ? "Encrypted" : "Cleartext";
  return base::JoinString({session_type_name, encryption_name}, "_");
}

INSTANTIATE_TEST_SUITE_P(All,
                         CommandStorageBackendParamTest,
                         ::testing::Values(
// On iOS, SessionRestore and AppRestore do not use CommandStorageBackend.
#if !BUILDFLAG(IS_IOS)
                             TestParams(SessionType::kAppRestore, false),
                             TestParams(SessionType::kAppRestore, true),
                             TestParams(SessionType::kSessionRestore, false),
                             TestParams(SessionType::kSessionRestore, true),
#endif  // !BUILDFLAG(IS_IOS)
                             TestParams(SessionType::kTabRestore, false),
                             TestParams(SessionType::kTabRestore, true)),
                         TestParamNameGenerator);

}  // namespace sessions
