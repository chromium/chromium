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
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "chrome/test/chromedriver/net/pipe_writer_posix.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "net/base/completion_once_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

std::pair<std::string, int> ReadAll(base::File read_pipe) {
  std::string received_message;
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(4096);
  int rv = 0;
  while (true) {
    rv = read_pipe.ReadAtCurrentPos(buffer->data(), buffer->size());
    if (rv <= 0) {
      break;
    }
    received_message.insert(received_message.end(), buffer->data(),
                            buffer->data() + rv);
  }
  return std::make_pair(std::move(received_message), rv);
}

int WriteAll(std::unique_ptr<PipeWriterPosix> writer, std::string message) {
  scoped_refptr<net::IOBufferWithSize> buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(4096);
  auto callback = [](base::RepeatingClosure extra_callback, int* written,
                     int rv) {
    *written = rv;
    extra_callback.Run();
  };
  int rv = 0;
  const int to_send = static_cast<int>(message.size());
  int sent = 0;
  while (sent < to_send) {
    base::RunLoop run_loop;
    int written = 0;
    int count = std::min(buffer->size(), to_send - sent);
    std::copy(std::next(message.begin(), sent),
              std::next(message.begin(), sent + count), buffer->data());
    rv = writer->Write(
        buffer.get(), count,
        base::BindOnce(callback, run_loop.QuitClosure(), &written));
    if (rv == net::ERR_IO_PENDING) {
      run_loop.Run();
      rv = written;
    }
    if (rv <= 0) {
      sent = rv;
      break;
    }
    sent += rv;
  }
  return sent;
}

}  // namespace

class PipeWriterPosixTest : public testing::Test {
 protected:
  PipeWriterPosixTest() : long_timeout_(base::Minutes(1)) {}
  ~PipeWriterPosixTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  Timeout long_timeout() const { return Timeout(long_timeout_); }

  bool CreatePipeWriter(std::unique_ptr<PipeWriterPosix>* writer,
                        base::File* read_pipe) {
    base::ScopedPlatformFile read_file;
    base::ScopedPlatformFile write_file;
    if (!base::CreatePipe(&read_file, &write_file)) {
      VLOG(0) << "unable to create a pipe";
      return false;
    }
    if (!base::SetCloseOnExec(write_file.get())) {
      VLOG(0) << "unable to label the parent pipes as close on exec";
      return false;
    }
    *read_pipe = base::File(std::move(read_file));
    *writer = std::make_unique<PipeWriterPosix>();
    (*writer)->Bind(std::move(write_file));
    return true;
  }

  void WriteAll(scoped_refptr<base::TaskRunner> task_runner,
                std::unique_ptr<PipeWriterPosix> writer,
                std::string message,
                base::OnceCallback<void(int)> callback) {
    task_runner->PostTask(
        FROM_HERE, base::BindOnce(&::WriteAll, std::move(writer), message)
                       .Then(base::BindPostTask(
                           task_environment_.GetMainThreadTaskRunner(),
                           std::move(callback))));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  const base::TimeDelta long_timeout_;
};

TEST_F(PipeWriterPosixTest, Ctor) {
  PipeWriterPosix writer;
  EXPECT_FALSE(writer.IsConnected());
}

TEST_F(PipeWriterPosixTest, CreateAndBind) {
  std::unique_ptr<PipeWriterPosix> writer;
  base::File read_pipe;
  EXPECT_TRUE(CreatePipeWriter(&writer, &read_pipe));
  EXPECT_TRUE(writer->IsConnected());
}

TEST_F(PipeWriterPosixTest, Close) {
  std::unique_ptr<PipeWriterPosix> writer;
  base::File read_pipe;
  EXPECT_TRUE(CreatePipeWriter(&writer, &read_pipe));
  EXPECT_TRUE(writer->IsConnected());
  writer->Close();
  EXPECT_FALSE(writer->IsConnected());
}

TEST_F(PipeWriterPosixTest, Write) {
  std::unique_ptr<PipeWriterPosix> writer;
  base::File read_pipe;
  EXPECT_TRUE(CreatePipeWriter(&writer, &read_pipe));
  EXPECT_TRUE(writer->IsConnected());

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  base::Thread thread("WriterThread");
  ASSERT_TRUE(thread.StartWithOptions(std::move(options)));
  std::pair<std::string, int> received;
  base::RunLoop run_loop;
  std::string sent_message = "Hello, pipes!";
  writer->DetachFromThread();
  int written = 0;
  auto callback =
      base::BindOnce([](int* written, int result) { *written = result; },
                     base::Unretained(&written))
          .Then(run_loop.QuitClosure());
  WriteAll(thread.task_runner(), std::move(writer), sent_message,
           std::move(callback));

  auto [received_message, code] = ReadAll(std::move(read_pipe));

  EXPECT_EQ(sent_message, received_message);
  EXPECT_EQ(0, code);

  run_loop.Run();
  const int expected_message_size = static_cast<int>(sent_message.size());
  EXPECT_EQ(expected_message_size, written);
}

TEST_F(PipeWriterPosixTest, WriteToClosed) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeWriterPosix> writer;
  base::File read_pipe;
  EXPECT_TRUE(CreatePipeWriter(&writer, &read_pipe));
  read_pipe.Close();
  EXPECT_TRUE(writer->IsConnected());
  std::string message = "Hello, World!";
  scoped_refptr<net::StringIOBuffer> buffer =
      base::MakeRefCounted<net::StringIOBuffer>(message);
  EXPECT_EQ(net::ERR_CONNECTION_RESET,
            writer->Write(buffer.get(), buffer->size(),
                          base::BindOnce([](int rv) {})));
  EXPECT_FALSE(writer->IsConnected());
}

TEST_F(PipeWriterPosixTest, WriteLarge) {
  std::unique_ptr<PipeWriterPosix> writer;
  base::File read_pipe;
  EXPECT_TRUE(CreatePipeWriter(&writer, &read_pipe));
  EXPECT_TRUE(writer->IsConnected());

  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  base::Thread thread("WriterThread");
  ASSERT_TRUE(thread.StartWithOptions(std::move(options)));
  std::pair<std::string, int> received;
  base::RunLoop run_loop;
  std::string sent_message(1 << 20, 'a');
  writer->DetachFromThread();
  int written = 0;
  auto callback =
      base::BindOnce([](int* written, int result) { *written = result; },
                     base::Unretained(&written))
          .Then(run_loop.QuitClosure());
  WriteAll(thread.task_runner(), std::move(writer), sent_message,
           std::move(callback));

  auto [received_message, code] = ReadAll(std::move(read_pipe));

  EXPECT_EQ(sent_message, received_message);
  EXPECT_EQ(0, code);

  run_loop.Run();
  const int expected_message_size = static_cast<int>(sent_message.size());
  EXPECT_EQ(expected_message_size, written);
}
