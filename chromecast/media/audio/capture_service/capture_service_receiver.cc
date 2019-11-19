// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/capture_service/capture_service_receiver.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_pump_type.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/media/audio/audio_buildflags.h"
#include "chromecast/media/audio/capture_service/constants.h"
#include "chromecast/media/audio/capture_service/message_parsing_util.h"
#include "chromecast/net/small_message_socket.h"
#include "media/base/limits.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

#if BUILDFLAG(USE_UNIX_SOCKETS)
#include "net/socket/unix_domain_client_socket_posix.h"
#else
#include "net/base/address_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_client_socket.h"
#endif  // BUILDFLAG(USE_UNIX_SOCKETS)

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
  Socket(std::unique_ptr<net::StreamSocket> socket, int channels);
  ~Socket() override;

  void Start(::media::AudioInputStream::AudioInputCallback* input_callback);

 private:
  // SmallMessageSocket::Delegate implementation:
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, int size) override;

  void OnInactivityTimeout();
  bool HandleAudio(std::unique_ptr<::media::AudioBus> audio, int64_t timestamp);
  void ReportErrorAndStop();

  SmallMessageSocket socket_;

  // Number of audio capture channels that audio manager defines.
  const int channels_;

  ::media::AudioInputStream::AudioInputCallback* input_callback_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

CaptureServiceReceiver::Socket::Socket(
    std::unique_ptr<net::StreamSocket> socket,
    int channels)
    : socket_(this, std::move(socket)),
      channels_(channels),
      input_callback_(nullptr) {
  DCHECK_GT(channels_, 0);
  DCHECK_LE(channels_, ::media::limits::kMaxChannels);
}

CaptureServiceReceiver::Socket::~Socket() = default;

void CaptureServiceReceiver::Socket::Start(
    ::media::AudioInputStream::AudioInputCallback* input_callback) {
  input_callback_ = input_callback;
  socket_.ReceiveMessages();
}

void CaptureServiceReceiver::Socket::ReportErrorAndStop() {
  if (input_callback_) {
    input_callback_->OnError();
  }
  input_callback_ = nullptr;
}

void CaptureServiceReceiver::Socket::OnError(int error) {
  LOG(INFO) << "Socket error from " << this << ": " << error;
  ReportErrorAndStop();
}

void CaptureServiceReceiver::Socket::OnEndOfStream() {
  LOG(ERROR) << "Got EOS " << this;
  ReportErrorAndStop();
}

bool CaptureServiceReceiver::Socket::OnMessage(char* data, int size) {
  // TODO(https://crbug.com/982014): Remove the DCHECK once using unsigned
  // |size|.
  DCHECK_GT(size, 0);
  int64_t timestamp = 0;
  auto audio = capture_service::ReadDataToAudioBus(data, size, &timestamp);
  if (!audio.has_value()) {
    ReportErrorAndStop();
    return false;
  }
  return HandleAudio(std::move(audio.value()), timestamp);
}

bool CaptureServiceReceiver::Socket::HandleAudio(
    std::unique_ptr<::media::AudioBus> audio,
    int64_t timestamp) {
  if (!input_callback_) {
    LOG(INFO) << "Received audio before stream metadata; ignoring";
    return false;
  }

  DCHECK(audio);
  DCHECK_EQ(channels_, audio->channels());
  input_callback_->OnData(
      audio.get(),
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(timestamp),
      /* volume =*/1.0);
  return true;
}

// static
constexpr base::TimeDelta CaptureServiceReceiver::kConnectTimeout;

CaptureServiceReceiver::CaptureServiceReceiver(
    const ::media::AudioParameters& audio_params)
    : audio_params_(audio_params), io_thread_(__func__) {
  base::Thread::Options options;
  options.message_pump_type = base::MessagePumpType::IO;
  // TODO(b/137106361): Tweak the thread priority once the thread priority for
  // speech processing gets fixed.
  options.priority = base::ThreadPriority::DISPLAY;
  CHECK(io_thread_.StartWithOptions(options));
  task_runner_ = io_thread_.task_runner();
  DCHECK(task_runner_);
}

CaptureServiceReceiver::~CaptureServiceReceiver() {
  Stop();
  io_thread_.Stop();
}

void CaptureServiceReceiver::Start(
    ::media::AudioInputStream::AudioInputCallback* input_callback) {
  ENSURE_ON_IO_THREAD(Start, input_callback);

#if BUILDFLAG(USE_UNIX_SOCKETS)
  std::string path = capture_service::kDefaultUnixDomainSocketPath;
  std::unique_ptr<net::StreamSocket> connecting_socket =
      std::make_unique<net::UnixDomainClientSocket>(
          path, true /* use_abstract_namespace */);
#else   // BUILDFLAG(USE_UNIX_SOCKETS)
  int port = capture_service::kDefaultTcpPort;
  net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(), port);
  std::unique_ptr<net::StreamSocket> connecting_socket =
      std::make_unique<net::TCPClientSocket>(
          net::AddressList(endpoint), nullptr, nullptr, net::NetLogSource());
#endif  // BUILDFLAG(USE_UNIX_SOCKETS)

  StartWithSocket(input_callback, std::move(connecting_socket));
}

void CaptureServiceReceiver::StartWithSocket(
    ::media::AudioInputStream::AudioInputCallback* input_callback,
    std::unique_ptr<net::StreamSocket> connecting_socket) {
  ENSURE_ON_IO_THREAD(StartWithSocket, input_callback,
                      std::move(connecting_socket));
  DCHECK(!connecting_socket_);
  DCHECK(!socket_);

  connecting_socket_ = std::move(connecting_socket);
  int result = connecting_socket_->Connect(
      base::BindOnce(&CaptureServiceReceiver::OnConnected,
                     base::Unretained(this), input_callback));
  if (result != net::ERR_IO_PENDING) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&CaptureServiceReceiver::OnConnected,
                       base::Unretained(this), input_callback, result));
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CaptureServiceReceiver::OnConnectTimeout,
                     base::Unretained(this), input_callback),
      kConnectTimeout);
}

void CaptureServiceReceiver::OnConnected(
    ::media::AudioInputStream::AudioInputCallback* input_callback,
    int result) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  if (result == net::OK) {
    socket_ = std::make_unique<Socket>(std::move(connecting_socket_),
                                       audio_params_.channels());
    socket_->Start(input_callback);
  } else {
    LOG(INFO) << "Connecting failed: " << net::ErrorToString(result);
    input_callback->OnError();
    connecting_socket_.reset();
  }
}

void CaptureServiceReceiver::OnConnectTimeout(
    ::media::AudioInputStream::AudioInputCallback* input_callback) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!connecting_socket_) {
    return;
  }
  LOG(ERROR) << __func__;
  input_callback->OnError();
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
