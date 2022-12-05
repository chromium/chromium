// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_socket.h"

#include <utility>

#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/net/io_buffer_pool.h"
#include "net/base/io_buffer.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

bool MixerSocketImpl::Delegate::HandleMetadata(const Generic& message) {
  return true;
}

MixerSocketImpl::MixerSocketImpl(std::unique_ptr<net::StreamSocket> socket)
    : audio_socket_(std::make_unique<AudioSocketExtension>(std::move(socket))) {
}

MixerSocketImpl::MixerSocketImpl()
    : audio_socket_(std::make_unique<AudioSocketExtension>()) {}

MixerSocketImpl::~MixerSocketImpl() = default;

void MixerSocketImpl::SetLocalCounterpart(
    base::WeakPtr<AudioSocket> local_counterpart,
    scoped_refptr<base::SequencedTaskRunner> counterpart_task_runner) {
  audio_socket_->SetLocalCounterpart(std::move(local_counterpart),
                                     std::move(counterpart_task_runner));
}

base::WeakPtr<AudioSocket> MixerSocketImpl::GetAudioSocketWeakPtr() {
  return audio_socket_->GetWeakPtr();
}

void MixerSocketImpl::SetDelegate(Delegate* delegate) {
  audio_socket_->SetDelegate(delegate);
}

void MixerSocketImpl::UseBufferPool(scoped_refptr<IOBufferPool> buffer_pool) {
  audio_socket_->UseBufferPool(std::move(buffer_pool));
}

MixerSocketImpl::AudioSocketExtension::AudioSocketExtension(
    std::unique_ptr<net::StreamSocket> socket)
    : AudioSocket(std::move(socket)) {}

MixerSocketImpl::AudioSocketExtension::AudioSocketExtension() = default;

bool MixerSocketImpl::SendAudioBuffer(scoped_refptr<net::IOBuffer> audio_buffer,
                                      int filled_bytes,
                                      int64_t timestamp) {
  return audio_socket_->SendAudioBuffer(std::move(audio_buffer), filled_bytes,
                                        timestamp);
}

bool MixerSocketImpl::SendProto(int type,
                                const google::protobuf::MessageLite& message) {
  return audio_socket_->SendProto(type, message);
}

void MixerSocketImpl::ReceiveMoreMessages() {
  audio_socket_->ReceiveMoreMessages();
}

MixerSocketImpl::AudioSocketExtension::~AudioSocketExtension() = default;

void MixerSocketImpl::AudioSocketExtension::SetDelegate(
    MixerSocket::Delegate* delegate) {
  DCHECK(delegate);
  delegate_ = delegate;
  AudioSocket::SetDelegate(delegate);
}

bool MixerSocketImpl::AudioSocketExtension::ParseMetadata(char* data,
                                                          size_t size) {
  Generic message;
  if (!message.ParseFromArray(data, size)) {
    LOG(INFO) << "Invalid metadata message from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  return delegate_->HandleMetadata(message);
}

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast
