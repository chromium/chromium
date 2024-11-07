// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/test/integration_test.h"

#include "base/compiler_specific.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <fcntl.h>
#endif

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/logging.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"

testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

IntegrationTest::IntegrationTest()
    : session_("IntegrationTestDefaultSession") {}

IntegrationTest::~IntegrationTest() = default;

void IntegrationTest::SetUpTestSuite() {
  InitLogging();
}

void IntegrationTest::SetUp() {
  http_server_.Start();

  const base::CommandLine* cur_proc_cmd =
      base::CommandLine::ForCurrentProcess();
  ASSERT_TRUE(cur_proc_cmd->HasSwitch("chrome"))
      << "path to chrome is not provided";
  base::FilePath path_to_chrome = cur_proc_cmd->GetSwitchValuePath("chrome");
  ASSERT_TRUE(base::PathExists(path_to_chrome))
      << "file not found: " << path_to_chrome;
  ASSERT_TRUE(user_data_dir_temp_dir_.CreateUniqueTempDir())
      << "cannot create temp dir for user data dir";
  base::CommandLine command(path_to_chrome);
  Switches switches = GetDesktopSwitches();
  switches.SetSwitch("remote-debugging-pipe");
  switches.SetSwitch("user-data-dir",
                     user_data_dir_temp_dir_.GetPath().AsUTF8Unsafe());
  switches.AppendToCommandLine(&command);

  base::LaunchOptions options;
  pipe_builder_.SetProtocolMode(PipeBuilder::kAsciizProtocolMode);
  Status status = pipe_builder_.SetUpPipes(&options, &command);
  ASSERT_TRUE(StatusOk(status));
  command.AppendArg("data:,");
#if BUILDFLAG(IS_POSIX)
  options.fds_to_remap.emplace_back(1, 1);
  options.fds_to_remap.emplace_back(2, 2);
#elif BUILDFLAG(IS_WIN)
  options.stdin_handle = INVALID_HANDLE_VALUE;
  options.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
  options.handles_to_inherit.push_back(options.stdout_handle);
  if (options.stderr_handle != options.stdout_handle) {
    options.handles_to_inherit.push_back(options.stderr_handle);
  }
#endif
  process_ = base::LaunchProcess(command, options);
  ASSERT_TRUE(process_.IsValid());

  int exit_code;
  base::TerminationStatus chrome_status =
      base::GetTerminationStatus(process_.Handle(), &exit_code);
  ASSERT_EQ(base::TERMINATION_STATUS_STILL_RUNNING, chrome_status);

  session_.w3c_compliant = true;

  browser_client_ = std::make_unique<DevToolsClientImpl>(
      DevToolsClientImpl::kBrowserwideDevToolsClientId, "");
}

void IntegrationTest::TearDown() {
  if (browser_client_) {
    Status status =
        browser_client_->SendCommand("Browser.close", base::Value::Dict());
    EXPECT_TRUE(StatusOk(status));
  }

  browser_client_.reset();

  if (process_.IsValid()) {
    process_.Close();
    int exit_code;
    if (!process_.WaitForExitWithTimeout(base::Seconds(10), &exit_code)) {
      process_.Terminate(0, true);
    }
  }

  http_server_.Stop();
}

Status IntegrationTest::SetUpConnection() {
  Status status{kOk};
  status = pipe_builder_.BuildSocket();
  if (status.IsError()) {
    return status;
  }
  std::unique_ptr<SyncWebSocket> socket = pipe_builder_.TakeSocket();
  if (!socket->Connect(GURL())) {
    return Status{kSessionNotCreated,
                  "failed to create a connection with the browser"};
  }
  status = browser_client_->SetSocket(std::move(socket));
  if (status.IsError()) {
    return status;
  }
  Timeout timeout{base::Seconds(10)};
  base::Value::Dict result;
  status = browser_client_->SendCommandAndGetResultWithTimeout(
      "Browser.getVersion", base::Value::Dict(), &timeout, &result);
  if (status.IsError()) {
    return status;
  }
  status = browser_info_.FillFromBrowserVersionResponse(result);
  pipe_builder_.CloseChildEndpoints();
  return status;
}
