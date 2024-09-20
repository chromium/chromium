// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/media/audio/capture_service/message_parsing_utils.h"
#include "chromecast/media/audio/net/audio_socket_service.h"
#include "chromecast/net/small_message_socket.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

// Helper macro to post tasks to the io thread. It is safe to use unretained
// |this|, since |this| owns the thread.
#define ENSURE_ON_IO_THREAD(method, ...)                                   \
  if (!task_runner_->RunsTasksInCurrentSequence()) {                       \
    task_runner_->PostTask(                                                \
        FROM_HERE, base::BindOnce(&CaptureServiceReceiver::method,         \
                                  base::Unretained(this), ##__VA_ARGS__)); \
    return;                                                                \
  }

namespace chromecast {
namespace media {

class CaptureServiceReceiver::Socket : public SmallMessageSocket::Delegate {
 public:
  Socket(std::unique_ptr<net::StreamSocket> socket,
         capture_service::StreamInfo request_stream_info,
         CaptureServiceReceiver::Delegate* delegate);
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;
  ~Socket() override;

 private:
  enum class State {
    kInit,
    kWaitForAck,
    kStreaming,
    kShutdown,
  };

  // SmallMessageSocket::Delegate implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, size_t size) override;

  bool SendRequest();
  void OnInactivityTimeout();
  void OnInitialStreamInfo(const capture_service::StreamInfo& stream_info);
  bool HandleAck(char* data, size_t size);
  bool HandleAudio(char* data, size_t size);
  void ReportErrorAndStop();

  SmallMessageSocket socket_;

  const capture_service::StreamInfo request_stream_info_;
  CaptureServiceReceiver::Delegate* const delegate_;

  State state_ = State::kInit;
};

CaptureServiceReceiver::Socket::Socket(
    std::unique_ptr<net::StreamSocket> socket,
    capture_service::StreamInfo request_stream_info,
    CaptureServiceReceiver::Delegate* delegate)
    : socket_(this, std::move(socket)),
      request_stream_info_(std::move(request_stream_info)),
      delegate_(delegate) {
  DCHECK(delegate_);
  if (!SendRequest()) {
    ReportErrorAndStop();
    return;
  }
  socket_.ReceiveMessages();
}

CaptureServiceReceiver::Socket::~Socket() = default;

bool CaptureServiceReceiver::Socket::SendRequest() {
  DCHECK_EQ(state_, State::kInit);
  auto request_buffer =
      capture_service::MakeHandshakeMessage(request_stream_info_);
  if (!request_buffer) {
    return false;
  }
  if (!socket_.SendBuffer(request_buffer.get(), request_buffer->size())) {
    LOG(WARNING) << "Socket should not block sending since the request is the "
                    "first buffer sent.";
    return false;
  }
  state_ = State::kWaitForAck;
  return true;
}

void CaptureServiceReceiver::Socket::OnSendUnblocked() {
  // The request is the first buffer sent, so it should be always accepted by
  // the socket.
  NOTREACHED();
}

void CaptureServiceReceiver::Socket::ReportErrorAndStop() {
  DCHECK_NE(state_, State::kShutdown);
  delegate_->OnCaptureError();
  state_ = State::kShutdown;
}

void CaptureServiceReceiver::Socket::OnError(int error) {
  LOG(INFO) << "Socket error from " << this << ": " << error;
  ReportErrorAndStop();
}

void CaptureServiceReceiver::Socket::OnEndOfStream() {
  LOG(ERROR) << "Got EOS " << this;
  ReportErrorAndStop();
}

bool CaptureServiceReceiver::Socket::OnMessage(char* data, size_t size) {
  uint8_t type = 0;
  if (size < sizeof(type)) {
    LOG(ERROR) << "Invalid message size: " << size << ".";
    return false;
  }
  memcpy(&type, data, sizeof(type));
  capture_service::MessageType message_type =
      static_cast<capture_service::MessageType>(type);

  if (state_ == State::kWaitForAck &&
      message_type == capture_service::MessageType::kHandshake) {
    return HandleAck(data, size);
  }

  if (state_ == State::kStreaming &&
      (message_type == capture_service::MessageType::kPcmAudio ||
       message_type == capture_service::MessageType::kOpusAudio)) {
    return HandleAudio(data, size);
  }

  if (message_type == capture_service::MessageType::kMetadata) {
    delegate_->OnCaptureMetadata(data, size);
    return true;
  }

  LOG(WARNING) << "Receive message with type " << type << " at state "
               << static_cast<int>(state_) << ", ignored.";
  return true;
}

bool CaptureServiceReceiver::Socket::HandleAck(char* data, size_t size) {
  DCHECK_EQ(state_, State::kWaitForAck);
  capture_service::StreamInfo info;
  if (!capture_service::ReadHandshakeMessage(data, size, &info) ||
      !delegate_->OnInitialStreamInfo(info)) {
    ReportErrorAndStop();
    return false;
  }
  state_ = State::kStreaming;
  return true;
}

bool CaptureServiceReceiver::Socket::HandleAudio(char* data, size_t size) {
  DCHECK_EQ(state_, State::kStreaming);
  if (!delegate_->OnCaptureData(data, size)) {
    ReportErrorAndStop();
    return false;
  }
  return true;
}

// static
constexpr base::TimeDelta CaptureServiceReceiver::kConnectTimeout;

CaptureServiceReceiver::CaptureServiceReceiver(
    capture_service::StreamInfo request_stream_info,
    Delegate* delegate)
    : request_stream_info_(std::move(request_stream_info)),
      delegate_(delegate),
      io_thread_(__func__) {
  DCHECK(delegate_);
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  options.thread_type = base::ThreadType::kDisplayCritical;
  CHECK(io_thread_.StartWithOptions(std::move(options)));
  task_runner_ = io_thread_.task_runner();
  DCHECK(task_runner_);
}

CaptureServiceReceiver::~CaptureServiceReceiver() {
  Stop();
  io_thread_.Stop();
}

void CaptureServiceReceiver::Start() {
  ENSURE_ON_IO_THREAD(Start);

  std::string path = capture_service::kDefaultUnixDomainSocketPath;
  int port = capture_service::kDefaultTcpPort;

  StartWithSocket(AudioSocketService::Connect(path, port));
}

void CaptureServiceReceiver::StartWithSocket(
    std::unique_ptr<net::StreamSocket> connecting_socket) {
  ENSURE_ON_IO_THREAD(StartWithSocket, std::move(connecting_socket));
  DCHECK(!connecting_socket_);
  DCHECK(!socket_);

  connecting_socket_ = std::move(connecting_socket);
  DCHECK(connecting_socket_);
  int result = connecting_socket_->Connect(base::BindOnce(
      &CaptureServiceReceiver::OnConnected, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(&CaptureServiceReceiver::OnConnected,
                                          base::Unretained(this), result));
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureServiceReceiver::OnConnectTimeout,
                     base::Unretained(this)),
      kConnectTimeout);
}

void CaptureServiceReceiver::OnConnected(int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  if (result == net::OK) {
    socket_ = std::make_unique<Socket>(std::move(connecting_socket_),
                                       request_stream_info_, delegate_);
  } else {
    LOG(INFO) << "Connecting failed: " << net::ErrorToString(result);
    delegate_->OnCaptureError();
    connecting_socket_.reset();
  }
}

void CaptureServiceReceiver::OnConnectTimeout() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!connecting_socket_) {
    return;
  }
  LOG(ERROR) << __func__;
  delegate_->OnCaptureError();
  connecting_socket_.reset();
}

void CaptureServiceReceiver::Stop() {
  base::WaitableEvent finished;
  StopOnTaskRunner(&finished);
  finished.Wait();
}

void CaptureServiceReceiver::StopOnTaskRunner(base::WaitableEvent* finished) {
  ENSURE_ON_IO_THREAD(StopOnTaskRunner, finished);
  connecting_socket_.reset();
  socket_.reset();
  finished->Signal();
}

void CaptureServiceReceiver::SetTaskRunnerForTest(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner_ = std::move(task_runner);
}

}  // namespace media
}  // namespace chromecast
