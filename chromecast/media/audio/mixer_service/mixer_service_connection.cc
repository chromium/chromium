// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_service_connection.h"

#include <limits>
#include <queue>
#include <string>
#include <utility>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/media/audio/mixer_service/constants.h"
#include "chromecast/media/audio/mixer_service/mixer_service.pb.h"
#include "chromecast/media/audio/mixer_service/mixer_service_buildflags.h"
#include "chromecast/media/audio/mixer_service/proto_helpers.h"
#include "chromecast/net/small_message_socket.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/stream_socket.h"

#if BUILDFLAG(USE_UNIX_SOCKETS)
#include "net/socket/unix_domain_client_socket_posix.h"
#else
#include "net/socket/tcp_client_socket.h"
#endif

namespace chromecast {
namespace media {

namespace {

// Header is 2 bytes size, 2 bytes type.
const int kHeaderSize = 4;

constexpr base::TimeDelta kConnectTimeout = base::TimeDelta::FromSeconds(1);

int GetSampleSize(mixer_service::MixerStreamParams::SampleFormat format) {
  switch (format) {
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_INT16_I:
      return sizeof(int16_t);
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_INT32_I:
      return sizeof(int32_t);
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_FLOAT_I:
      return sizeof(float);
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_INT16_P:
      return sizeof(int16_t);
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_INT32_P:
      return sizeof(int32_t);
    case mixer_service::MixerStreamParams::SAMPLE_FORMAT_FLOAT_P:
      return sizeof(float);
    default:
      NOTREACHED() << "Unknown sample format " << format;
      return 0;
  }
}

int GetFrameSize(const mixer_service::MixerStreamParams& params) {
  return GetSampleSize(params.sample_format()) * params.num_channels();
}

int GetFillSizeFrames(const mixer_service::MixerStreamParams& params) {
  if (params.has_fill_size_frames()) {
    return params.fill_size_frames();
  }
  // Use 10 ms by default.
  return params.sample_rate() / 100;
}

}  // namespace

class MixerServiceConnection::Socket : public SmallMessageSocket {
 public:
  Socket(std::unique_ptr<net::StreamSocket> socket,
         MixerServiceConnection::Delegate* delegate,
         const mixer_service::MixerStreamParams& params);
  ~Socket() override;

  void Start(float volume_multiplier);
  void SendNextBuffer(int filled_frames);
  void SetVolumeMultiplier(float multiplier);

 private:
  // SmallMessageSocket implementation:
  void OnSendUnblocked() override;
  void OnError(int error) override;
  void OnEndOfStream() override;
  bool OnMessage(char* data, int size) override;

  void CreateNextBuffer();
  void SendProto(const google::protobuf::MessageLite& message);
  bool HandleMetadata(char* data, int size);

  MixerServiceConnection::Delegate* const delegate_;
  const mixer_service::MixerStreamParams params_;
  const int frame_size_;
  const int fill_size_frames_;

  scoped_refptr<net::IOBufferWithSize> next_buffer_;
  std::queue<scoped_refptr<net::IOBufferWithSize>> write_queue_;

  DISALLOW_COPY_AND_ASSIGN(Socket);
};

MixerServiceConnection::Socket::Socket(
    std::unique_ptr<net::StreamSocket> socket,
    MixerServiceConnection::Delegate* delegate,
    const mixer_service::MixerStreamParams& params)
    : SmallMessageSocket(std::move(socket)),
      delegate_(delegate),
      params_(params),
      frame_size_(GetFrameSize(params)),
      fill_size_frames_(GetFillSizeFrames(params)) {
  DCHECK(delegate_);
  DCHECK_LE(fill_size_frames_ * frame_size_,
            std::numeric_limits<uint16_t>::max() - kHeaderSize);
}

MixerServiceConnection::Socket::~Socket() = default;

void MixerServiceConnection::Socket::Start(float volume_multiplier) {
  ReceiveMessages();
  mixer_service::Generic message;
  *(message.mutable_params()) = params_;
  message.mutable_set_volume()->set_volume(volume_multiplier);
  SendProto(message);
  CreateNextBuffer();
  delegate_->FillNextBuffer(next_buffer_->data() + kHeaderSize,
                            fill_size_frames_,
                            std::numeric_limits<int64_t>::min());
}

void MixerServiceConnection::Socket::CreateNextBuffer() {
  DCHECK(!next_buffer_);
  next_buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(
      kHeaderSize + fill_size_frames_ * frame_size_);
}

void MixerServiceConnection::Socket::SendNextBuffer(int filled_frames) {
  int payload_size = sizeof(int16_t) + filled_frames * frame_size_;
  uint16_t size = static_cast<uint16_t>(payload_size);
  int16_t type = static_cast<int16_t>(mixer_service::MessageType::kAudio);
  char* ptr = next_buffer_->data();

  base::WriteBigEndian(ptr, size);
  ptr += sizeof(size);
  base::WriteBigEndian(ptr, type);

  if (SmallMessageSocket::SendBuffer(next_buffer_.get(),
                                     sizeof(uint16_t) + payload_size)) {
    next_buffer_ = nullptr;
  } else {
    write_queue_.push(std::move(next_buffer_));
  }
}

void MixerServiceConnection::Socket::SetVolumeMultiplier(float multiplier) {
  mixer_service::Generic message;
  message.mutable_set_volume()->set_volume(multiplier);
  SendProto(message);
}

void MixerServiceConnection::Socket::SendProto(
    const google::protobuf::MessageLite& message) {
  auto storage = mixer_service::SendProto(message, this);
  if (storage) {
    write_queue_.push(std::move(storage));
  }
}

void MixerServiceConnection::Socket::OnSendUnblocked() {
  while (!write_queue_.empty()) {
    if (!SmallMessageSocket::SendBuffer(write_queue_.front().get(),
                                        write_queue_.front()->size())) {
      return;
    }
    write_queue_.pop();
  }
}

void MixerServiceConnection::Socket::OnError(int error) {
  delegate_->OnConnectionError();
}

void MixerServiceConnection::Socket::OnEndOfStream() {
  delegate_->OnConnectionError();
}

bool MixerServiceConnection::Socket::OnMessage(char* data, int size) {
  int16_t type;
  if (size < static_cast<int>(sizeof(type))) {
    LOG(ERROR) << "Invalid message size " << size << " from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  base::ReadBigEndian(data, &type);
  data += sizeof(type);
  size -= sizeof(type);

  switch (static_cast<mixer_service::MessageType>(type)) {
    case mixer_service::MessageType::kMetadata:
      return HandleMetadata(data, size);
    default:
      // Ignore unhandled message types.
      break;
  }
  return true;
}

bool MixerServiceConnection::Socket::HandleMetadata(char* data, int size) {
  mixer_service::Generic message;
  if (!mixer_service::ReceiveProto(data, size, &message)) {
    LOG(ERROR) << "Invalid metadata from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  if (message.has_push_result()) {
    CreateNextBuffer();
    delegate_->FillNextBuffer(next_buffer_->data() + kHeaderSize,
                              fill_size_frames_,
                              message.push_result().next_playback_timestamp());
  } else if (message.has_eos_played_out()) {
    delegate_->OnEosPlayed();
  }

  return true;
}

MixerServiceConnection::MixerServiceConnection(
    Delegate* delegate,
    const mixer_service::MixerStreamParams& params)
    : delegate_(delegate),
      params_(params),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(delegate_);
  DCHECK_GT(params_.sample_rate(), 0);
  DCHECK_GT(params_.num_channels(), 0);
}

MixerServiceConnection::~MixerServiceConnection() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
}

void MixerServiceConnection::Connect() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!connecting_socket_);
  DCHECK(!socket_);

  base::WeakPtr<MixerServiceConnection> self = weak_factory_.GetWeakPtr();

#if BUILDFLAG(USE_UNIX_SOCKETS)
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  std::string path =
      command_line->GetSwitchValueASCII(switches::kMixerServiceEndpoint);
  if (path.empty()) {
    path = mixer_service::kDefaultUnixDomainSocketPath;
  }
  connecting_socket_ = std::make_unique<net::UnixDomainClientSocket>(
      path, true /* use_abstract_namespace */);
#else   // BUILDFLAG(USE_UNIX_SOCKETS)
  int port = GetSwitchValueNonNegativeInt(switches::kMixerServiceEndpoint,
                                          mixer_service::kDefaultTcpPort);
  net::IPEndPoint endpoint(net::IPAddress::IPv4Localhost(), port);
  connecting_socket_ = std::make_unique<net::TCPClientSocket>(
      net::AddressList(endpoint), nullptr, nullptr, net::NetLogSource());
#endif  // BUILDFLAG(USE_UNIX_SOCKETS)

  auto connect_callback =
      base::BindRepeating(&MixerServiceConnection::ConnectCallback, self);
  int result = connecting_socket_->Connect(connect_callback);
  if (result != net::ERR_IO_PENDING) {
    task_runner_->PostTask(FROM_HERE, base::BindOnce(connect_callback, result));
    return;
  }

  task_runner_->PostDelayedTask(
      FROM_HERE, base::BindOnce(&MixerServiceConnection::ConnectTimeout, self),
      kConnectTimeout);
}

void MixerServiceConnection::SendNextBuffer(int filled_frames) {
  if (!socket_) {
    LOG(ERROR) << "Tried to send buffer without a connection";
    delegate_->OnConnectionError();
    return;
  }

  socket_->SendNextBuffer(filled_frames);
}

void MixerServiceConnection::SetVolumeMultiplier(float multiplier) {
  volume_multiplier_ = multiplier;
  if (socket_) {
    socket_->SetVolumeMultiplier(multiplier);
  }
}

void MixerServiceConnection::ConnectCallback(int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);
  if (!connecting_socket_) {
    return;
  }

  if (result == net::OK) {
    socket_ = std::make_unique<Socket>(std::move(connecting_socket_), delegate_,
                                       params_);
    socket_->Start(volume_multiplier_);
  } else {
    connecting_socket_.reset();
    delegate_->OnConnectionError();
  }
}

void MixerServiceConnection::ConnectTimeout() {
  if (!connecting_socket_) {
    return;
  }

  connecting_socket_.reset();
  delegate_->OnConnectionError();
}

}  // namespace media
}  // namespace chromecast
