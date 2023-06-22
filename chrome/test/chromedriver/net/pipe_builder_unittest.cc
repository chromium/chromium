// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <memory>
#include <string>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

namespace {

#if BUILDFLAG(IS_POSIX)
testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}
#endif

class PipeBuilderTest : public testing::Test {
 protected:
  PipeBuilderTest() : long_timeout_(base::Minutes(1)) {}
  ~PipeBuilderTest() override = default;

  Timeout long_timeout() const { return Timeout(long_timeout_); }

  base::CommandLine CreateCommandLine() {
    base::CommandLine command(base::GetMultiProcessTestChildBaseCommandLine());
    command.AppendArg("--remote-debugging-in-pipe=3");
    command.AppendArg("--remote-debugging-out-pipe=4");
    return command;
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  const base::TimeDelta long_timeout_;

 protected:
  base::FilePath test_helper_path_;
};

const char kInPipeParamName[] = "remote-debugging-in-pipe";
const char kOutPipeParamName[] = "remote-debugging-out-pipe";

enum {
  kCopyContentSuccess = 0,
  kCopyContentReadError = 1,
  kCopyContentWriteError = 2,
  kCopyContentInvalidInHandle = 3,
  kCopyContentInvalidOutHandle = 4,
  kInvokationError = 1000,
};

MULTIPROCESS_TEST_MAIN(PipeEchoProcess) {
  const int capacity = 1024;
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kInPipeParamName) ||
      !cmd_line->HasSwitch(kOutPipeParamName)) {
    return kInvokationError;
  }
  std::string in_pipe = cmd_line->GetSwitchValueASCII(kInPipeParamName);
  std::string out_pipe = cmd_line->GetSwitchValueASCII(kOutPipeParamName);
  std::vector<char> buffer(capacity);
  int fd_in = -1;
  int fd_out = -1;
  if (!base::StringToInt(in_pipe, &fd_in) ||
      !base::StringToInt(out_pipe, &fd_out)) {
    return kInvokationError;
  }
  base::File fin;
  base::File fout;
#if BUILDFLAG(IS_WIN)
  intptr_t handle = _get_osfhandle(fd_in);
  HANDLE read_handle =
      handle <= 0 ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(handle);
  if (read_handle == INVALID_HANDLE_VALUE) {
    return kCopyContentInvalidInHandle;
  }
  handle = _get_osfhandle(fd_out);
  HANDLE write_handle =
      handle <= 0 ? INVALID_HANDLE_VALUE : reinterpret_cast<HANDLE>(handle);
  if (write_handle == INVALID_HANDLE_VALUE) {
    return kCopyContentInvalidOutHandle;
  }
  fin = base::File(read_handle);
  fout = base::File(write_handle);
#else
  fin = base::File(fd_in, true);
  fout = base::File(fd_out, true);
#endif
  while (true) {
    int bytes_read =
        fin.ReadAtCurrentPosNoBestEffort(buffer.data(), buffer.size());
    // read_bytes < 0 means an error
    // read_bytes == 0 means EOF
    if (bytes_read < 0) {
      return kCopyContentReadError;
    }
    if (bytes_read == 0) {
      // EOF
      break;
    }
    int bytes_written = fout.WriteAtCurrentPos(buffer.data(), bytes_read);
    if (bytes_written < 0) {
      return kCopyContentWriteError;
    }
  }
  return kCopyContentSuccess;
}

}  // namespace

TEST_F(PipeBuilderTest, Ctor) {
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, NoProtocolModeIsProvided) {
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, CborIsUnsupported) {
  PipeBuilder pipe_builder;
  EXPECT_STREQ("cbor", PipeBuilder::kCborProtocolMode);
  pipe_builder.SetProtocolMode(PipeBuilder::kCborProtocolMode);
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#if BUILDFLAG(IS_POSIX)

TEST_F(PipeBuilderTest, PlatfformIsSupported) {
  EXPECT_TRUE(PipeBuilder::PlatformIsSupported());
}

TEST_F(PipeBuilderTest, CloseChildEndpointsWhenNotStarted) {
  PipeBuilder pipe_builder;
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
}

TEST_F(PipeBuilderTest, EmptyStringProtocolMode) {
  PipeBuilder pipe_builder;
  pipe_builder.SetProtocolMode("");
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
  std::unique_ptr<SyncWebSocket> socket = pipe_builder.TakeSocket();
  EXPECT_NE(nullptr, socket.get());
}

TEST_F(PipeBuilderTest, SendAndReceive) {
  PipeBuilder pipe_builder;
  pipe_builder.SetProtocolMode(PipeBuilder::kAsciizProtocolMode);
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
  base::CommandLine command = CreateCommandLine();
  options.fds_to_remap.emplace_back(1, 1);
  options.fds_to_remap.emplace_back(2, 2);
  base::Process process =
      base::SpawnMultiProcessTestChild("PipeEchoProcess", command, options);
  ASSERT_TRUE(process.IsValid());
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
  std::unique_ptr<SyncWebSocket> socket = pipe_builder.TakeSocket();
  EXPECT_NE(nullptr, socket.get());
  EXPECT_TRUE(socket->Connect(GURL()));
  const std::string sent_message = "Hello, pipes!";
  EXPECT_TRUE(socket->Send(sent_message));
  EXPECT_TRUE(socket->IsConnected());
  std::string received_message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            socket->ReceiveNextMessage(&received_message, long_timeout()));
  EXPECT_TRUE(socket->IsConnected());
  EXPECT_EQ(sent_message, received_message);
  socket.reset();
  int exit_code = -1;
  process.WaitForExit(&exit_code);
  EXPECT_EQ(0, exit_code);
}

#else

TEST_F(PipeBuilderTest, PlatformIsUnsupported) {
  EXPECT_FALSE(PipeBuilder::PlatformIsSupported());
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_TRUE(pipe_builder.CloseChildEndpoints().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#endif  // Posix only tests
