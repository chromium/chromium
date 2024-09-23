// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/tracing/system_tracer.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/posix/unix_domain_socket.h"
#include "base/trace_event/trace_config.h"
#include "chromecast/chromecast_buildflags.h"
#include "chromecast/tracing/system_tracing_common.h"

namespace chromecast {
namespace {

constexpr size_t kBufferSize = 1UL << 16;

base::ScopedFD CreateClientSocket() {
  base::ScopedFD socket_fd(
      socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0));
  if (!socket_fd.is_valid()) {
    PLOG(ERROR) << "socket";
    return base::ScopedFD();
  }

  struct sockaddr_un addr =
      chromecast::tracing::GetSystemTracingSocketAddress();

  if (connect(socket_fd.get(), reinterpret_cast<struct sockaddr*>(&addr),
              sizeof(addr))) {
    PLOG(ERROR) << "connect: " << addr.sun_path;
    return base::ScopedFD();
  }

  return socket_fd;
}

class SystemTracerImpl : public SystemTracer {
 public:
  SystemTracerImpl() : buffer_(new char[kBufferSize]) {}
  ~SystemTracerImpl() override { Cleanup(); }

  void StartTracing(std::string_view categories,
                    StartTracingCallback callback) override;

  void StopTracing(const StopTracingCallback& callback) override;

 private:
  enum class State {
    INITIAL,   // Not yet started.
    STARTING,  // Sent start message, waiting for ack.
    TRACING,   // Tracing, not yet requested stop.
    READING,   // Trace stopped, reading output.
    FINISHED,  // All done.
  };

  void ReceiveStartAckAndTracePipe();
  void ReceiveTraceData();
  void FailStartTracing();
  void FailStopTracing();
  void SendPartialTraceData();
  void FinishTracing();
  void Cleanup();

  // Current state of tracing attempt.
  State state_ = State::INITIAL;

  // Unix socket connection to tracing daemon.
  base::ScopedFD connection_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> connection_watcher_;

  // Pipe for trace data.
  base::ScopedFD trace_pipe_fd_;
  std::unique_ptr<base::FileDescriptorWatcher::Controller> trace_pipe_watcher_;

  // Read buffer (of size kBufferSize).
  std::unique_ptr<char[]> buffer_;

  // Callbacks for StartTracing() and StopTracing().
  StartTracingCallback start_tracing_callback_;
  StopTracingCallback stop_tracing_callback_;

  // Trace data.
  std::string trace_data_;
};

void SystemTracerImpl::StartTracing(std::string_view categories,
                                    StartTracingCallback callback) {
  start_tracing_callback_ = std::move(callback);
  if (state_ != State::INITIAL) {
    FailStartTracing();
    return;
  }

  if (categories.size() == 0) {
    // No relevant categories are enabled.
    FailStartTracing();
    return;
  }

  connection_fd_ = CreateClientSocket();
  if (!connection_fd_.is_valid()) {
    FailStartTracing();
    return;
  }

  if (!base::UnixDomainSocket::SendMsg(connection_fd_.get(), categories.data(),
                                       categories.size(), std::vector<int>())) {
    PLOG(ERROR) << "sendmsg";
    FailStartTracing();
    return;
  }

  connection_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      connection_fd_.get(),
      base::BindRepeating(&SystemTracerImpl::ReceiveStartAckAndTracePipe,
                          base::Unretained(this)));
  state_ = State::STARTING;
}

void SystemTracerImpl::StopTracing(const StopTracingCallback& callback) {
  stop_tracing_callback_ = callback;
  if (state_ != State::TRACING) {
    FailStopTracing();
    return;
  }

  char stop_tracing_message[] = {0};
  if (!base::UnixDomainSocket::SendMsg(
          connection_fd_.get(), stop_tracing_message,
          sizeof(stop_tracing_message), std::vector<int>())) {
    PLOG(ERROR) << "sendmsg";
    FailStopTracing();
    return;
  }

  trace_pipe_watcher_ = base::FileDescriptorWatcher::WatchReadable(
      trace_pipe_fd_.get(),
      base::BindRepeating(&SystemTracerImpl::ReceiveTraceData,
                          base::Unretained(this)));
  state_ = State::READING;
}

void SystemTracerImpl::ReceiveStartAckAndTracePipe() {
  DCHECK_EQ(state_, State::STARTING);

  std::vector<base::ScopedFD> fds;
  ssize_t received = base::UnixDomainSocket::RecvMsg(
      connection_fd_.get(), buffer_.get(), kBufferSize, &fds);
  if (received == 0) {
    LOG(ERROR) << "EOF from server";
    FailStartTracing();
    return;
  }
  if (received < 0) {
    PLOG(ERROR) << "recvmsg";
    FailStartTracing();
    return;
  }
  if (fds.size() != 1) {
    LOG(ERROR) << "Start ack missing trace pipe";
    FailStartTracing();
    return;
  }

  trace_pipe_fd_ = std::move(fds[0]);
  connection_watcher_.reset();
  state_ = State::TRACING;
  std::move(start_tracing_callback_).Run(Status::OK);
}

void SystemTracerImpl::ReceiveTraceData() {
  DCHECK_EQ(state_, State::READING);

  for (;;) {
    ssize_t bytes =
        HANDLE_EINTR(read(trace_pipe_fd_.get(), buffer_.get(), kBufferSize));
    if (bytes < 0) {
      if (errno == EAGAIN)
        return;  // Wait for more data.
      PLOG(ERROR) << "read: trace";
      FailStopTracing();
      return;
    }

    if (bytes == 0) {
      FinishTracing();
      return;
    }

    trace_data_.append(buffer_.get(), bytes);

    static constexpr size_t kPartialTraceDataSize = 1UL << 20;  // 1 MiB
    if (trace_data_.size() > kPartialTraceDataSize) {
      SendPartialTraceData();
      return;
    }
  }
}

void SystemTracerImpl::FailStartTracing() {
  std::move(start_tracing_callback_).Run(Status::FAIL);
  Cleanup();
}

void SystemTracerImpl::FailStopTracing() {
  stop_tracing_callback_.Run(Status::FAIL, "");
  Cleanup();
}

void SystemTracerImpl::SendPartialTraceData() {
  DCHECK_EQ(state_, State::READING);
  stop_tracing_callback_.Run(Status::KEEP_GOING, std::move(trace_data_));
  trace_data_ = "";
}

void SystemTracerImpl::FinishTracing() {
  DCHECK_EQ(state_, State::READING);
  stop_tracing_callback_.Run(Status::OK, std::move(trace_data_));
  Cleanup();
}

void SystemTracerImpl::Cleanup() {
  connection_watcher_.reset();
  connection_fd_.reset();
  trace_pipe_watcher_.reset();
  trace_pipe_fd_.reset();
  start_tracing_callback_.Reset();
  stop_tracing_callback_.Reset();
  state_ = State::FINISHED;
}

class FakeSystemTracer : public SystemTracer {
 public:
  FakeSystemTracer() = default;
  ~FakeSystemTracer() override = default;

  void StartTracing(std::string_view categories,
                    StartTracingCallback callback) override {
    std::move(callback).Run(Status::OK);
  }

  void StopTracing(const StopTracingCallback& callback) override {
    std::string trace_data = "# tracer: nop\n";
    std::move(callback).Run(Status::OK, std::move(trace_data));
  }
};

}  // namespace

// static
std::unique_ptr<SystemTracer> SystemTracer::Create() {
#if BUILDFLAG(IS_CAST_DESKTOP_BUILD)
  return std::make_unique<FakeSystemTracer>();
#else
  return std::make_unique<SystemTracerImpl>();
#endif
}

}  // namespace chromecast
