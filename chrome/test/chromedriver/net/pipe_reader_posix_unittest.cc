// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cmath>
#include <string>

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/net/pipe_reader_posix.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class PipeReaderPosixTest : public testing::Test {
 protected:
  PipeReaderPosixTest() : long_timeout_(base::Minutes(1)) {}
  ~PipeReaderPosixTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  Timeout long_timeout() const { return Timeout(long_timeout_); }

  bool CreatePipeReader(std::unique_ptr<PipeReaderPosix>* reader,
                        base::File* write_pipe) {
    base::ScopedPlatformFile read_file;
    base::ScopedPlatformFile write_file;
    if (!base::CreatePipe(&read_file, &write_file)) {
      VLOG(0) << "unable to create a pipe";
      return false;
    }
    if (!base::SetCloseOnExec(read_file.get())) {
      VLOG(0) << "unable to label the pipes as close on exec";
      return false;
    }
    *write_pipe = base::File(std::move(write_file));
    *reader = std::make_unique<PipeReaderPosix>();
    (*reader)->Bind(std::move(read_file));
    return true;
  }

  std::pair<std::string, int> ReadAll(PipeReaderPosix* reader) {
    scoped_refptr<net::IOBufferWithSize> buffer =
        base::MakeRefCounted<net::IOBufferWithSize>(4096);
    auto callback = [](base::RepeatingClosure extra_callback, int* received,
                       int rv) {
      *received = rv;
      extra_callback.Run();
    };
    int rv = 0;
    std::string received_message;
    while (true) {
      base::RunLoop run_loop;
      int received = 0;
      rv = reader->Read(
          buffer.get(), buffer->size(),
          base::BindOnce(callback, run_loop.QuitClosure(), &received));
      if (rv == net::ERR_IO_PENDING) {
        run_loop.Run();
        rv = received;
      }
      if (rv <= 0) {
        break;
      }
      received_message.insert(received_message.end(), buffer->data(),
                              buffer->data() + rv);
    }
    return std::make_pair(std::move(received_message), rv);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(PipeReaderPosixTest, Ctor) {
  PipeReaderPosix reader;
  EXPECT_FALSE(reader.IsConnected());
}

TEST_F(PipeReaderPosixTest, CreateAndBind) {
  std::unique_ptr<PipeReaderPosix> reader;
  base::File write_pipe;
  EXPECT_TRUE(CreatePipeReader(&reader, &write_pipe));
  EXPECT_TRUE(reader->IsConnected());
}

TEST_F(PipeReaderPosixTest, Close) {
  std::unique_ptr<PipeReaderPosix> reader;
  base::File write_pipe;
  EXPECT_TRUE(CreatePipeReader(&reader, &write_pipe));
  EXPECT_TRUE(reader->IsConnected());
  reader->Close();
  EXPECT_FALSE(reader->IsConnected());
}

TEST_F(PipeReaderPosixTest, Read) {
  std::unique_ptr<PipeReaderPosix> reader;
  base::File write_pipe;
  EXPECT_TRUE(CreatePipeReader(&reader, &write_pipe));
  EXPECT_TRUE(reader->IsConnected());

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  base::Thread thread("WriterThread");
  ASSERT_TRUE(thread.StartWithOptions(std::move(options)));
  const std::string sent_message = "Hello, World!";
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::File write_pipe, std::string message) {
            write_pipe.WriteAtCurrentPos(base::as_byte_span(message));
          },
          std::move(write_pipe), sent_message));

  std::pair<std::string, int> received = ReadAll(reader.get());

  EXPECT_EQ(sent_message, received.first);
  EXPECT_TRUE(received.second == 0 ||
              received.second == net::ERR_CONNECTION_CLOSED);
  EXPECT_FALSE(reader->IsConnected());
}

TEST_F(PipeReaderPosixTest, ReadClosed) {
  std::unique_ptr<PipeReaderPosix> reader;
  base::File write_pipe;
  EXPECT_TRUE(CreatePipeReader(&reader, &write_pipe));
  EXPECT_TRUE(reader->IsConnected());
  write_pipe.Close();
  std::string received_message(1024, ' ');
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(received_message);
  EXPECT_EQ(net::ERR_CONNECTION_CLOSED,
            reader->Read(buffer.get(), buffer->size(),
                         base::BindOnce([](int rv) {})));
  EXPECT_FALSE(reader->IsConnected());
}

TEST_F(PipeReaderPosixTest, ReadLarge) {
  std::unique_ptr<PipeReaderPosix> reader;
  base::File write_pipe;
  EXPECT_TRUE(CreatePipeReader(&reader, &write_pipe));
  EXPECT_TRUE(reader->IsConnected());

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  base::Thread thread("WriterThread");
  ASSERT_TRUE(thread.StartWithOptions(std::move(options)));
  const int expected_message_size = 1 << 20;
  std::string sent_message(expected_message_size, 'a');
  thread.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::File write_pipe, std::string message) {
            write_pipe.WriteAtCurrentPos(base::as_byte_span(message));
          },
          std::move(write_pipe), sent_message));

  std::pair<std::string, int> received = ReadAll(reader.get());

  EXPECT_EQ(sent_message, received.first);
  EXPECT_TRUE(received.second == 0 ||
              received.second == net::ERR_CONNECTION_CLOSED);
  EXPECT_FALSE(reader->IsConnected());
}
