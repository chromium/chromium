// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <memory>

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_type.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/single_thread_task_executor.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chromecast/tracing/ftrace.h"
#include "chromecast/tracing/system_tracing_common.h"

namespace chromecast {
namespace tracing {
namespace {

constexpr size_t kMessageSize = 4096;

base::ScopedFD CreateServerSocket() {
  struct sockaddr_un addr = GetSystemTracingSocketAddress();

  if (unlink(addr.sun_path) != 0 && errno != ENOENT)
    PLOG(ERROR) << "unlink: " << addr.sun_path;

  base::ScopedFD socket_fd(
      socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
  if (!socket_fd.is_valid()) {
    PLOG(ERROR) << "socket";
    return base::ScopedFD();
  }

  if (bind(socket_fd.get(), reinterpret_cast<struct sockaddr*>(&addr),
           sizeof(addr))) {
    PLOG(ERROR) << "bind: " << addr.sun_path;
    return base::ScopedFD();
  }

  if (chmod(addr.sun_path, 0666))
    PLOG(WARNING) << "chmod: " << addr.sun_path;

  static constexpr int kBacklog = 10;
  if (listen(socket_fd.get(), kBacklog)) {
    PLOG(ERROR) << "listen: " << addr.sun_path;
    return base::ScopedFD();
  }

  return socket_fd;
}

std::vector<std::string> ParseCategories(base::StringPiece message) {
  std::vector<std::string> requested_categories = base::SplitString(
      message, ",", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<std::string> categories;
  for (const auto& category : requested_categories) {
    if (IsValidCategory(category))
      categories.push_back(category);
    else
      LOG(WARNING) << "Unrecognized category: " << category;
  }
  return categories;
}

class TraceCopyTask : public base::MessagePumpLibevent::FdWatcher {
 public:
  // Read 64 kB at a time (standard pipe capacity).
  static constexpr size_t kCopyBufferSize = 1UL << 16;

  enum class Status {
    SUCCESS,
    FAILURE,
  };

  TraceCopyTask(base::ScopedFD in_fd,
                base::ScopedFD out_fd,
                base::OnceCallback<void(Status, size_t)> callback)
      : buffer_(new char[kCopyBufferSize]),
        in_fd_(std::move(in_fd)),
        out_fd_(std::move(out_fd)),
        out_watcher_(FROM_HERE),
        callback_(std::move(callback)) {}
  ~TraceCopyTask() override {}

  void Start() {
    base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        out_fd_.get(), true /* persistent */,
        base::MessagePumpForIO::WATCH_WRITE, &out_watcher_, this);
  }

  // base::MessagePumpLibevent::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override { NOTREACHED(); }
  void OnFileCanWriteWithoutBlocking(int fd) override {
    DCHECK_EQ(out_fd_.get(), fd);
    CopyTraceData();
  }

 private:
  void CopyTraceData() {
    for (;;) {
      if (read_ == written_) {
        total_copied_ += read_;
        read_ = written_ = 0;

        // Read trace data from debugfs.
        ssize_t read_bytes =
            HANDLE_EINTR(read(in_fd_.get(), buffer_.get(), kCopyBufferSize));
        if (read_bytes == 0) {
          // EOF, we're done;
          Finish(Status::SUCCESS);
          return;
        } else if (read_bytes < 0) {
          PLOG(ERROR) << "read: trace";
          Finish(Status::FAILURE);
          return;
        }

        read_ = read_bytes;
      }

      // Write trace data to output pipe.
      ssize_t written_bytes = HANDLE_EINTR(
          write(out_fd_.get(), buffer_.get() + written_, read_ - written_));
      if (written_bytes < 0) {
        if (errno == EAGAIN)
          return;  // Wait for more space.
        PLOG(ERROR) << "write: pipe";
        Finish(Status::FAILURE);
        return;
      }
      written_ += written_bytes;
    }
  }

  void Finish(Status status) {
    out_watcher_.StopWatchingFileDescriptor();
    in_fd_.reset();
    out_fd_.reset();
    buffer_.reset();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), status, total_copied_));
  }

  std::unique_ptr<char[]> buffer_;
  size_t read_ = 0;
  size_t written_ = 0;
  size_t total_copied_ = 0;

  // Trace data file.
  base::ScopedFD in_fd_;

  // Pipe for trace data.
  base::ScopedFD out_fd_;
  base::MessagePumpLibevent::FdWatchController out_watcher_;

  // Callback for when copy finishes.
  base::OnceCallback<void(Status, size_t)> callback_;
};

class TraceConnection : public base::MessagePumpLibevent::FdWatcher {
 public:
  TraceConnection(base::ScopedFD connection_fd, base::OnceClosure callback)
      : recv_buffer_(new char[kMessageSize]),
        connection_fd_(std::move(connection_fd)),
        connection_watcher_(FROM_HERE),
        callback_(std::move(callback)),
        weak_ptr_factory_(this) {}
  ~TraceConnection() override {}

  void Init() {
    base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        connection_fd_.get(), true /* persistent */,
        base::MessagePumpForIO::WATCH_READ, &connection_watcher_, this);
  }

  // base::MessagePumpLibevent::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK_EQ(connection_fd_.get(), fd);
    ReceiveClientMessage();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  enum class State {
    INITIAL,        // Waiting for init message
    TRACING,        // Ftrace started.
    COPYING,        // Ftrace stopped, copying trace data.
    COPY_COMPLETE,  // Ftrace stopped, all data written to pipe.
    FINISHED,       // All done.
  };

  void ReceiveClientMessage() {
    std::vector<base::ScopedFD> fds;
    ssize_t bytes = base::UnixDomainSocket::RecvMsg(
        connection_fd_.get(), recv_buffer_.get(), kMessageSize, &fds);
    if (bytes < 0) {
      PLOG(ERROR) << "recvmsg";
      Finish();
      return;
    } else if (bytes == 0) {
      LOG(INFO) << "connection closed";
      Finish();
    } else {
      base::StringPiece message(recv_buffer_.get(), bytes);
      HandleClientMessage(message);
    }
  }

  void HandleClientMessage(base::StringPiece message) {
    if (state_ == State::INITIAL) {
      std::vector<std::string> categories = ParseCategories(message);

      if (!StartFtrace(categories)) {
        LOG(ERROR) << "Failed to start ftrace";
        Finish();
        return;
      }

      if (!SendTracePipeToClient()) {
        LOG(ERROR) << "Failed to send trace pipe";
        Finish();
        return;
      }

      LOG(INFO) << "Started tracing for categories: "
                << base::JoinString(categories, ",");

      state_ = State::TRACING;
    } else if (state_ == State::TRACING) {
      WriteFtraceTimeSyncMarker();
      StopFtrace();
      base::ScopedFD trace_data = GetFtraceData();
      if (!trace_data.is_valid()) {
        LOG(ERROR) << "Failed to get trace data";
        Finish();
        return;
      }

      LOG(INFO) << "Tracing stopped";
      trace_copy_task_ = std::make_unique<TraceCopyTask>(
          std::move(trace_data), std::move(trace_pipe_),
          base::BindOnce(&TraceConnection::OnFinishedCopying,
                         weak_ptr_factory_.GetWeakPtr()));
      trace_copy_task_->Start();
      state_ = State::COPYING;
    } else {
      LOG(WARNING) << "Unexpected message";
      Finish();
      return;
    }
  }

  void OnFinishedCopying(TraceCopyTask::Status status, size_t trace_data_size) {
    if (status == TraceCopyTask::Status::SUCCESS) {
      LOG(INFO) << "Finished tracing (" << trace_data_size << " bytes copied)";
      state_ = State::COPY_COMPLETE;
    } else {
      LOG(INFO) << "I/O error copying trace data";
    }

    Finish();
  }

  bool SendTracePipeToClient() {
    int pipefd[2] = {-1, -1};
    if (pipe2(pipefd, O_CLOEXEC | O_NONBLOCK)) {
      PLOG(ERROR) << "pipe2";
      return false;
    }
    base::ScopedFD read_end(pipefd[0]);
    base::ScopedFD write_end(pipefd[1]);

    const char response[] = {0};
    std::vector<int> send_fds;
    send_fds.push_back(read_end.get());
    if (!base::UnixDomainSocket::SendMsg(connection_fd_.get(), response,
                                         sizeof(response), send_fds)) {
      PLOG(ERROR) << "sendmsg";
      return false;
    }

    trace_pipe_ = std::move(write_end);
    return true;
  }

  void Finish() {
    if (state_ != State::COPY_COMPLETE)
      LOG(WARNING) << "Ending tracing without sending data";
    trace_copy_task_.reset();
    state_ = State::FINISHED;
    recv_buffer_.reset();
    connection_watcher_.StopWatchingFileDescriptor();
    connection_fd_.reset();
    StopFtrace();
    ClearFtrace();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_)));
  }

  // Tracing state.
  State state_ = State::INITIAL;

  // Buffer for incoming messages.
  std::unique_ptr<char[]> recv_buffer_;

  // Client connection.
  base::ScopedFD connection_fd_;
  base::MessagePumpLibevent::FdWatchController connection_watcher_;

  // Pipe for trace output.
  base::ScopedFD trace_pipe_;

  // Task to send all trace output to client via a pipe.
  std::unique_ptr<TraceCopyTask> trace_copy_task_;

  // Callback for when connection closes.
  base::OnceClosure callback_;

  base::WeakPtrFactory<TraceConnection> weak_ptr_factory_;
};

class TracingService : public base::MessagePumpLibevent::FdWatcher {
 public:
  TracingService()
      : server_socket_watcher_(FROM_HERE), weak_ptr_factory_(this) {}
  ~TracingService() override {}

  bool Init() {
    server_socket_ = CreateServerSocket();
    if (!server_socket_.is_valid())
      return false;

    base::MessageLoopCurrentForIO::Get()->WatchFileDescriptor(
        server_socket_.get(), true /* persistent */,
        base::MessagePumpForIO::WATCH_READ, &server_socket_watcher_, this);

    return true;
  }

  // base::MessagePumpLibevent::FdWatcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK_EQ(server_socket_.get(), fd);
    AcceptConnection();
  }
  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

 private:
  void AcceptConnection() {
    base::ScopedFD connection_fd(accept4(server_socket_.get(), nullptr, nullptr,
                                         SOCK_NONBLOCK | SOCK_CLOEXEC));
    if (!connection_fd.is_valid()) {
      PLOG(ERROR) << "accept: ";
      return;
    }

    trace_connection_ = std::make_unique<TraceConnection>(
        std::move(connection_fd),
        base::BindOnce(&TracingService::OnConnectionClosed,
                       weak_ptr_factory_.GetWeakPtr()));
    trace_connection_->Init();
  }

  void OnConnectionClosed() { trace_connection_.reset(); }

  // Socket and watcher for listening socket.
  base::ScopedFD server_socket_;
  base::MessagePumpLibevent::FdWatchController server_socket_watcher_;

  // Currently active tracing connection.
  // There can only be one; ftrace affects the whole system.
  std::unique_ptr<TraceConnection> trace_connection_;

  base::WeakPtrFactory<TracingService> weak_ptr_factory_;
};

}  // namespace
}  // namespace tracing
}  // namespace chromecast

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
  logging::InitLogging(logging::LoggingSettings());

  signal(SIGPIPE, SIG_IGN);

  LOG(INFO) << "Starting system tracing service...";

  base::SingleThreadTaskExecutor io_task_executor(base::MessagePumpType::IO);
  chromecast::tracing::TracingService service;

  if (!service.Init())
    return EXIT_FAILURE;

  base::RunLoop run_loop;
  run_loop.Run();

  return EXIT_SUCCESS;
}
