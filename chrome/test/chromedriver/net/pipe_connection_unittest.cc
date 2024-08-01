// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <cmath>
#include <string>

#include "base/files/file_util.h"
#include "base/files/platform_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/test/chromedriver/net/pipe_connection.h"
#include "chrome/test/chromedriver/net/sync_websocket.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace {

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

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_WIN)
int ReadFromPipe(base::PlatformFile file_out, char* buffer, int size) {
  int offset = 0;
  int rv = 0;
  for (; offset < size; offset += rv) {
    rv = ReadFromPipeNoBestEffort(file_out, buffer + offset, size - offset);
    if (rv < 0) {
      return rv;
    }
  }
  return offset;
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

class PipeConnectionTest : public testing::Test {
 protected:
  PipeConnectionTest() : long_timeout_(base::Minutes(1)) {}
  ~PipeConnectionTest() override = default;

  void SetUp() override {}

  void TearDown() override {}

  Timeout long_timeout() const { return Timeout(long_timeout_); }

#if BUILDFLAG(IS_WIN)
  bool CreatePipeConnection(std::unique_ptr<PipeConnection>* connection,
                            base::ScopedPlatformFile* read_pipe,
                            base::ScopedPlatformFile* write_pipe) {
    base::ScopedPlatformFile parent_to_child_read_file;
    base::ScopedPlatformFile parent_to_child_write_file;
    base::ScopedPlatformFile child_to_parent_read_file;
    base::ScopedPlatformFile child_to_parent_write_file;
    HANDLE parent_to_child_read_handle;
    HANDLE parent_to_child_write_handle;
    HANDLE child_to_parent_read_handle;
    HANDLE child_to_parent_write_handle;
    if (!CreatePipe(&parent_to_child_read_handle, &parent_to_child_write_handle,
                    nullptr, 0)) {
      return false;
    }
    parent_to_child_read_file.Set(parent_to_child_read_handle);
    parent_to_child_write_file.Set(parent_to_child_write_handle);
    if (!CreatePipe(&child_to_parent_read_handle, &child_to_parent_write_handle,
                    nullptr, 0)) {
      return false;
    }
    child_to_parent_read_file.Set(child_to_parent_read_handle);
    child_to_parent_write_file.Set(child_to_parent_write_handle);
    *connection =
        std::make_unique<PipeConnection>(std::move(child_to_parent_read_file),
                                         std::move(parent_to_child_write_file));
    *read_pipe = std::move(parent_to_child_read_file);
    *write_pipe = std::move(child_to_parent_write_file);
    return true;
  }
#elif BUILDFLAG(IS_POSIX)
  bool CreatePipeConnection(std::unique_ptr<PipeConnection>* connection,
                            base::ScopedPlatformFile* read_pipe,
                            base::ScopedPlatformFile* write_pipe) {
    base::ScopedPlatformFile parent_to_child_read_file;
    base::ScopedPlatformFile parent_to_child_write_file;
    base::ScopedPlatformFile child_to_parent_read_file;
    base::ScopedPlatformFile child_to_parent_write_file;
    if (!base::CreatePipe(&parent_to_child_read_file,
                          &parent_to_child_write_file)) {
      return false;
    }
    if (!base::CreatePipe(&child_to_parent_read_file,
                          &child_to_parent_write_file)) {
      return false;
    }
    if (!base::SetCloseOnExec(child_to_parent_read_file.get()) ||
        !base::SetCloseOnExec(parent_to_child_write_file.get())) {
      return false;
    }
    *connection =
        std::make_unique<PipeConnection>(std::move(child_to_parent_read_file),
                                         std::move(parent_to_child_write_file));
    *read_pipe = std::move(parent_to_child_read_file);
    *write_pipe = std::move(child_to_parent_write_file);
    return true;
  }
#endif

  bool Start(PipeConnection* connection) {
    return connection->Connect(GURL(""));
  }

  void SendResponse(base::PlatformFile pipe, const std::string& message) {
    WriteToPipe(pipe, message.c_str(), message.size() + 1);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  const base::TimeDelta long_timeout_;
};

}  // namespace

TEST_F(PipeConnectionTest, Ctor) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_FALSE(connection->HasNextMessage());
}

TEST_F(PipeConnectionTest, WriteToNotStarted) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  std::string message;
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_FALSE(connection->Send("drop it"));
}

TEST_F(PipeConnectionTest, ReadFromNotStarted) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  std::string message;
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ("", message);
}

TEST_F(PipeConnectionTest, Start) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
}

TEST_F(PipeConnectionTest, Send) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  const std::string expected_message = "Hello, World!";
  EXPECT_TRUE(connection->Send(expected_message));

  std::string actual_message(expected_message.size() + 1, ' ');
  EXPECT_EQ(static_cast<int>(actual_message.size()),
            ReadFromPipe(read_pipe.get(), actual_message.data(),
                         actual_message.size()));
  EXPECT_EQ(static_cast<char>(0), actual_message.back());
  actual_message.resize(expected_message.size());
  EXPECT_EQ(expected_message, actual_message);
}

TEST_F(PipeConnectionTest, Receive) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));

  connection->SetNotificationCallback(run_loop.QuitClosure());
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));

  const std::string expected_message = "Hello, World!";
  SendResponse(write_pipe.get(), expected_message);

  run_loop.Run();

  EXPECT_TRUE(connection->HasNextMessage());
  std::string actual_message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&actual_message, long_timeout()));
  EXPECT_EQ(expected_message, actual_message);
}

TEST_F(PipeConnectionTest, NotificationArrives) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  bool notified = false;

  connection->SetNotificationCallback(base::BindRepeating(
      [](bool* flag, base::RepeatingClosure callback) {
        *flag = true;
        callback.Run();
      },
      &notified, run_loop.QuitClosure()));

  EXPECT_TRUE(Start(connection.get()));

  SendResponse(write_pipe.get(), "ABC");

  std::string message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(10));

  // Notification must arrive via the message queue.
  // If it arrives earlier then we have a threading problem.
  EXPECT_FALSE(notified);

  run_loop.Run();

  EXPECT_TRUE(notified);
}

TEST_F(PipeConnectionTest, DetermineRecipient) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));

  std::string message_for_chromedriver = R"({
        "id": 1,
        "method": "Page.enable"
      })";
  std::string message_not_for_chromedriver = R"({
        "id": -1,
        "method": "Page.enable"
      })";
  SendResponse(write_pipe.get(), message_not_for_chromedriver);
  SendResponse(write_pipe.get(), message_for_chromedriver);
  std::string message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));

  // Getting message id and method
  std::optional<base::Value> message_value = base::JSONReader::Read(message);
  EXPECT_TRUE(message_value.has_value());
  base::Value::Dict* message_dict = message_value->GetIfDict();
  EXPECT_TRUE(message_dict);
  const std::string* method = message_dict->FindString("method");
  EXPECT_EQ(*method, "Page.enable");
  int id = message_dict->FindInt("id").value_or(-1);
  EXPECT_EQ(id, 1);
}

TEST_F(PipeConnectionTest, SendReceiveTimeout) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));

  std::string message;
  EXPECT_EQ(
      SyncWebSocket::StatusCode::kTimeout,
      connection->ReceiveNextMessage(&message, Timeout(base::TimeDelta())));
}

TEST_F(PipeConnectionTest, SendReceiveLarge) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  std::string wrote_message(10 << 20, 'a');
  SendResponse(write_pipe.get(), wrote_message);
  std::string message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ(wrote_message.length(), message.length());
  EXPECT_EQ(wrote_message, message);
}

TEST_F(PipeConnectionTest, SendMany) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->Send("1"));
  EXPECT_TRUE(connection->Send("2"));
  std::string received_string(4, ' ');
  ReadFromPipe(read_pipe.get(), received_string.data(), 4);
  EXPECT_EQ('1', received_string[0]);
  EXPECT_EQ('\0', received_string[1]);
  EXPECT_EQ('2', received_string[2]);
  EXPECT_EQ('\0', received_string[3]);
}

TEST_F(PipeConnectionTest, ReceiveMany) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  SendResponse(write_pipe.get(), "1");
  SendResponse(write_pipe.get(), "2");
  std::string message;
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ("1", message);
  SendResponse(write_pipe.get(), "3");
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ("2", message);
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ("3", message);
}

TEST_F(PipeConnectionTest, CloseBeforeReceive) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  std::string message;
  write_pipe = base::ScopedPlatformFile();
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_EQ("", message);
}

TEST_F(PipeConnectionTest, CloseOnReceive) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  std::string message;
  const std::string message_part = "part of...";
  // No trailing \0, thereofre the text must be discarded
  WriteToPipe(write_pipe.get(), message_part.data(), message_part.size());
  write_pipe = base::ScopedPlatformFile();
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_EQ("", message);
}

TEST_F(PipeConnectionTest, CloseAfterResponse) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  std::string message;
  SendResponse(write_pipe.get(), "Response");
  write_pipe = base::ScopedPlatformFile();
  EXPECT_EQ(SyncWebSocket::StatusCode::kOk,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_EQ("Response", message);
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_FALSE(connection->IsConnected());
}

TEST_F(PipeConnectionTest, CloseRemoteReadOnSend) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  read_pipe = base::ScopedPlatformFile();
  // The return value is true because the actual IO is delayed.
  connection->Send("ignore");
  // By this moment the connection loss must be discovered by the IO thread.
  EXPECT_FALSE(connection->Send("2"));
  EXPECT_FALSE(connection->IsConnected());
}

TEST_F(PipeConnectionTest, CloseRemoteWriteOnSend) {
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  write_pipe = base::ScopedPlatformFile();
  std::string message;
  // Wait until the IO thread discovers that the remote end is closed.
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  // This sending attempt invokes shutdown
  EXPECT_FALSE(connection->Send("1"));
  EXPECT_FALSE(connection->IsConnected());
  // Any following sending / reading attempts must fail gracefully
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
  EXPECT_FALSE(connection->Send("2"));
}

TEST_F(PipeConnectionTest, CloseRemoteWriteCausesShutdown) {
  base::RunLoop run_loop;
  std::unique_ptr<PipeConnection> connection;
  base::ScopedPlatformFile read_pipe;
  base::ScopedPlatformFile write_pipe;
  EXPECT_TRUE(CreatePipeConnection(&connection, &read_pipe, &write_pipe));
  EXPECT_TRUE(Start(connection.get()));
  EXPECT_TRUE(connection->IsConnected());
  write_pipe = base::ScopedPlatformFile();
  std::string message;
  // Wait until the IO thread discovers that the remote end is closed.
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));

  Timeout timeout{base::Seconds(10)};
  while (!connection->IsNull() && !timeout.IsExpired()) {
    run_loop.RunUntilIdle();
  }

  EXPECT_TRUE(connection->IsNull());
  EXPECT_FALSE(connection->IsConnected());
  EXPECT_FALSE(connection->Send("1"));
  EXPECT_EQ(SyncWebSocket::StatusCode::kDisconnected,
            connection->ReceiveNextMessage(&message, long_timeout()));
}
