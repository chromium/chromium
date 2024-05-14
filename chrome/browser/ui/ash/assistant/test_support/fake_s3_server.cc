// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/test_support/fake_s3_server.h"

#include <memory>

#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chromeos/ash/services/assistant/service.h"
#include "chromeos/assistant/internal/internal_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::assistant {

namespace {

// TODO(b/258750971): remove when internal assistant codes are migrated to
// namespace ash.
using ::chromeos::assistant::kFakeS3ServerBinary;
using ::chromeos::assistant::kFakeS3ServerBinaryV2;
using ::chromeos::assistant::kGenerateTokenInstructions;

// Folder where the S3 communications are stored when running in replay mode.
constexpr char kTestDataFolder[] = "chromeos/assistant/internal/test_data/";

// Fake device id passed to Libassistant. By fixing this we ensure it remains
// consistent between the current session and the value stored in the stored
// test data.
// This must be a 16 characters hex string or it will be rejected.
constexpr char kDeviceId[] = "11112222333344445555666677778888";

base::FilePath GetExecutableDir() {
  base::FilePath result;
  base::PathService::Get(base::DIR_EXE, &result);
  return result;
}

base::FilePath GetSourceDir() {
  base::FilePath result;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &result);
  return result;
}

std::string GetSanitizedTestName() {
  std::string test_name = base::ToLowerASCII(base::StringPrintf(
      "%s_%s",
      testing::UnitTest::GetInstance()->current_test_info()->test_suite_name(),
      testing::UnitTest::GetInstance()->current_test_info()->name()));
  // The test name may has `disabled_`. Remove it to match the data_file
  // name.
  base::ReplaceSubstringsAfterOffset(&test_name, 0, "disabled_", "");
  return test_name;
}

const std::string GetAccessTokenFromEnvironmentOrDie() {
  const char* token = std::getenv("TOKEN");
  CHECK(token && strlen(token))
      << "No token found in the environmental variable $TOKEN.\n"
      << kGenerateTokenInstructions;
  return token;
}

std::string FakeS3ModeToString(FakeS3Mode mode) {
  switch (mode) {
    case FakeS3Mode::kProxy:
      return "PROXY";
    case FakeS3Mode::kRecord:
      return "RECORD";
    case FakeS3Mode::kReplay:
      return "REPLAY";
  }
  NOTREACHED_IN_MIGRATION();
}

void AppendArgument(base::CommandLine* command_line,
                    const std::string& name,
                    const std::string& value) {
  // Note we can't use |AppendSwitchASCII| as that will add "<name>=<value>",
  // and the fake s3 server binary does not support '='.
  command_line->AppendArg(name);
  command_line->AppendArg(value);
}

}  // namespace

// Selects a port for the fake S3 server to use.
// This will use a file-based lock because different test shards might be trying
// to run fake S3 servers at the same time, and we need to ensure they use
// different ports.
class PortSelector {
 public:
  PortSelector() { SelectPort(); }
  PortSelector(PortSelector&) = delete;
  PortSelector& operator=(PortSelector&) = delete;
  ~PortSelector() {
    lock_file_.Close();
    base::DeletePathRecursively(GetLockFilePath());
  }

  int port() const { return port_; }

 private:
  // The first port we'll try to use. Randomly chosen to be outside of the range
  // of known ports.
  constexpr static int kStartPort = 23600;
  // Maximum number of ports we'll try before we give up and conclude no ports
  // are available (which really should not happen).
  constexpr static int kMaxAttempts = 20000;

  void SelectPort() {
    for (int offset = 0; offset + 1 < kMaxAttempts; offset += 2) {
      port_ = kStartPort + offset;
      lock_file_ = base::File(GetLockFilePath(), GetFileFlags());
      if (lock_file_.IsValid())
        return;
    }
    CHECK(false) << "Failed to find an available port.";
  }

  base::FilePath GetLockFilePath() const {
    std::string file_name = "port_" + base::NumberToString(port_) + "_lock";
    return GetLockFileDirectory().Append(file_name);
  }
  static base::FilePath GetLockFileDirectory() {
    base::FilePath result;
    bool success = base::GetTempDir(&result);
    EXPECT_TRUE(success);
    return result;
  }

  static int GetFileFlags() {
    return base::File::FLAG_CREATE | base::File::FLAG_WRITE;
  }

  // File exclusively opened on the file-system, to ensure no other fake S3
  // server uses the same port.
  base::File lock_file_;
  int port_;
};

FakeS3Server::FakeS3Server(int data_file_version)
    : data_file_version_(data_file_version),
      port_selector_(std::make_unique<PortSelector>()) {
  DCHECK_GT(data_file_version, 0);
}

FakeS3Server::~FakeS3Server() {
  Teardown();
}

void FakeS3Server::Setup(FakeS3Mode mode) {
  SetAccessTokenForMode(mode);
  StartS3ServerProcess(mode);
  SetFakeS3ServerURI();
  SetDeviceId();
}

void FakeS3Server::Teardown() {
  StopS3ServerProcess();
  UnsetDeviceId();
  UnsetFakeS3ServerURI();
}

std::string FakeS3Server::GetAccessToken() const {
  return access_token_;
}

void FakeS3Server::SetAccessTokenForMode(FakeS3Mode mode) {
  if (mode == FakeS3Mode::kProxy || mode == FakeS3Mode::kRecord) {
    access_token_ = GetAccessTokenFromEnvironmentOrDie();
  }
}

void FakeS3Server::SetFakeS3ServerURI() {
  // Note this must be stored in a local variable, as
  // `Service::OverrideS3ServerUriForTesting` does not take ownership of the
  // `const char *`.
  fake_s3_server_uri_ = "localhost:" + base::NumberToString(port());
  Service::OverrideS3ServerUriForTesting(fake_s3_server_uri_.c_str());
}

void FakeS3Server::SetDeviceId() {
  Service::OverrideDeviceIdForTesting(kDeviceId);
}

void FakeS3Server::UnsetDeviceId() {
  Service::OverrideDeviceIdForTesting(nullptr);
}

void FakeS3Server::UnsetFakeS3ServerURI() {
  Service::OverrideS3ServerUriForTesting(nullptr);
  fake_s3_server_uri_ = "";
}

void FakeS3Server::StartS3ServerProcess(FakeS3Mode mode) {
  if (process_running_) {
    LOG(WARNING)
        << "Called FakeS3Server::StartS3ServerProcess when already running.";
    return;
  }

  base::FilePath fake_s3_server_main;
  fake_s3_server_main =
      GetExecutableDir().Append(FILE_PATH_LITERAL(kFakeS3ServerBinaryV2));

  base::CommandLine command_line(fake_s3_server_main);
  AppendArgument(&command_line, "--port", base::NumberToString(port()));
  AppendArgument(&command_line, "--http_port",
                 base::NumberToString(port() + 1));
  AppendArgument(&command_line, "--mode", FakeS3ModeToString(mode));
  AppendArgument(&command_line, "--auth_token", GetAccessToken());
  AppendArgument(&command_line, "--test_data_file", GetTestDataFileName());

  fake_s3_server_ = base::LaunchProcess(command_line, base::LaunchOptions{});
  process_running_ = true;
}

void FakeS3Server::StopS3ServerProcess() {
  if (!process_running_) {
    LOG(WARNING)
        << "Called FakeS3Server::StopS3ServerProcess when already stopped.";
    return;
  }
  fake_s3_server_.Terminate(/*exit_code=*/0, /*wait=*/true);
  process_running_ = false;
}

std::string FakeS3Server::GetTestDataFileName() {
  auto create_file_path = [](const std::string& test_name, int version) {
    return GetSourceDir()
        .Append(FILE_PATH_LITERAL(kTestDataFolder))
        .Append(FILE_PATH_LITERAL(test_name + ".v" +
                                  base::NumberToString(version) +
                                  ".fake_s3.proto"));
  };
  // Look for the latest version of the data file, if not found, look for older
  // ones.
  auto data_file = create_file_path(GetSanitizedTestName(), data_file_version_);
  for (int version = data_file_version_ - 1;
       !base::PathExists(data_file) && version > 0; --version) {
    data_file = create_file_path(GetSanitizedTestName(), version);
  }

  return data_file.MaybeAsASCII();
}

int FakeS3Server::port() const {
  return port_selector_->port();
}

}  // namespace ash::assistant
