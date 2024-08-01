// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cmath>
#include <memory>
#include <string>

#include "base/compiler_specific.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <fcntl.h>
#endif

#include "base/command_line.h"
#include "base/files/platform_file.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_POSIX)
#include "base/posix/eintr_wrapper.h"
#elif BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace {

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
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
    return base::GetMultiProcessTestChildBaseCommandLine();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  const base::TimeDelta long_timeout_;
};

#if BUILDFLAG(IS_WIN)
const char kIoPipesParamName[] = "remote-debugging-io-pipes";
#endif

enum {
  kSuccess = 0,
  kReadError = 1,
  kWriteError = 2,
  kInvalidInPipe = 3,
  kInvalidOutPipe = 4,
  kIoPipesNotFound = 5,
  kIoPipesAreMalformed = 6,
};

#if BUILDFLAG(IS_POSIX)
int ReadFromPipeNoBestEffort(base::PlatformFile file_in,
                             char* buffer,
                             int size) {
  return HANDLE_EINTR(read(file_in, buffer, size));
}
#elif BUILDFLAG(IS_WIN)
int ReadFromPipeNoBestEffort(base::PlatformFile file_in,
                             char* buffer,
                             int size) {
  unsigned long received = 0;
  if (!::ReadFile(file_in, buffer, size, &received, nullptr)) {
    return (GetLastError() == ERROR_BROKEN_PIPE) ? 0 : -1;
  }
  return static_cast<int>(received);
}
#endif

#if BUILDFLAG(IS_POSIX)
int WriteToPipeNoBestEffort(base::PlatformFile file_out,
                            const char* buffer,
                            int size) {
  return HANDLE_EINTR(write(file_out, buffer, size));
}
#elif BUILDFLAG(IS_WIN)
int WriteToPipeNoBestEffort(base::PlatformFile file_out,
                            const char* buffer,
                            int size) {
  unsigned long written = 0;
  if (!::WriteFile(file_out, buffer, size, &written, nullptr)) {
    return -1;
  }
  return static_cast<int>(written);
}
#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
int WriteToPipe(base::PlatformFile file_out, const char* buffer, int size) {
  int offset = 0;
  int rv = 0;
  for (; offset < size; offset += rv) {
    rv = WriteToPipeNoBestEffort(file_out, buffer + offset, size - offset);
    if (rv < 0) {
      return rv;
    }
  }
  return offset;
}
#endif

#if BUILDFLAG(IS_WIN)
HANDLE ParseHandle(const std::string& serialized_handle) {
  uint32_t handle_as_uin32;
  if (!base::StringToUint(serialized_handle, &handle_as_uin32)) {
    return INVALID_HANDLE_VALUE;
  }
  HANDLE handle = base::win::Uint32ToHandle(handle_as_uin32);
  if (GetFileType(handle) != FILE_TYPE_PIPE) {
    return INVALID_HANDLE_VALUE;
  }
  return handle;
}
#endif

MULTIPROCESS_TEST_MAIN(PipeEchoProcess) {
  const int capacity = 1024;
  base::ScopedPlatformFile file_in;
  base::ScopedPlatformFile file_out;
#if BUILDFLAG(IS_POSIX)
  file_in = base::ScopedPlatformFile(3);
  file_out = base::ScopedPlatformFile(4);
#elif BUILDFLAG(IS_WIN)
  base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(kIoPipesParamName)) {
    return kIoPipesNotFound;
  }
  std::string io_pipes = cmd_line->GetSwitchValueASCII(kIoPipesParamName);
  std::vector<std::string> pipe_names = base::SplitString(
      io_pipes, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pipe_names.size() != 2) {
    return kIoPipesAreMalformed;
  }
  std::string in_pipe = pipe_names[0];
  std::string out_pipe = pipe_names[1];
  base::win::ScopedHandle read_handle(ParseHandle(in_pipe));
  if (!read_handle.is_valid()) {
    return kInvalidInPipe;
  }
  base::win::ScopedHandle write_handle(ParseHandle(out_pipe));
  if (!write_handle.is_valid()) {
    return kInvalidOutPipe;
  }
  file_in = std::move(read_handle);
  file_out = std::move(write_handle);
#endif
  std::vector<char> buffer(capacity);
  while (true) {
    int bytes_read =
        ReadFromPipeNoBestEffort(file_in.get(), buffer.data(), buffer.size());
    // read_bytes < 0 means an error
    // read_bytes == 0 means EOF
    if (bytes_read < 0) {
      return kReadError;
    }
    if (bytes_read == 0) {
      // EOF
      break;
    }
    int bytes_written = WriteToPipe(file_out.get(), buffer.data(), bytes_read);
    if (bytes_written < 0) {
      return kWriteError;
    }
  }
  return kSuccess;
}

}  // namespace

TEST_F(PipeBuilderTest, Ctor) {
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, NoProtocolModeIsProvided) {
  PipeBuilder pipe_builder;
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options, &command).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

TEST_F(PipeBuilderTest, CborIsUnsupported) {
  PipeBuilder pipe_builder;
  EXPECT_STREQ("cbor", PipeBuilder::kCborProtocolMode);
  pipe_builder.SetProtocolMode(PipeBuilder::kCborProtocolMode);
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options, &command).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)

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
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options, &command)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
  EXPECT_TRUE(StatusOk(pipe_builder.CloseChildEndpoints()));
  std::unique_ptr<SyncWebSocket> socket = pipe_builder.TakeSocket();
  EXPECT_NE(nullptr, socket.get());
}

TEST_F(PipeBuilderTest, SendAndReceive) {
  PipeBuilder pipe_builder;
  pipe_builder.SetProtocolMode(PipeBuilder::kAsciizProtocolMode);
  base::CommandLine command = CreateCommandLine();
  base::LaunchOptions options;
  EXPECT_TRUE(StatusOk(pipe_builder.SetUpPipes(&options, &command)));
  EXPECT_TRUE(StatusOk(pipe_builder.BuildSocket()));
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

#else  // unsupported platforms

TEST_F(PipeBuilderTest, PlatformIsUnsupported) {
  EXPECT_FALSE(PipeBuilder::PlatformIsSupported());
  PipeBuilder pipe_builder;
  base::LaunchOptions options;
  EXPECT_TRUE(pipe_builder.SetUpPipes(&options).IsError());
  EXPECT_TRUE(pipe_builder.BuildSocket().IsError());
  EXPECT_TRUE(pipe_builder.CloseChildEndpoints().IsError());
  EXPECT_EQ(nullptr, pipe_builder.TakeSocket().get());
}

#endif
