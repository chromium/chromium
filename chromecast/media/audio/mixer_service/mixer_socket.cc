// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/mixer_service/mixer_socket.h"

#include <utility>

#include "base/logging.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace mixer_service {

bool MixerSocket::Delegate::HandleMetadata(const Generic& message) {
  return true;
}

MixerSocket::MixerSocket(std::unique_ptr<net::StreamSocket> socket)
    : AudioSocket(std::move(socket)) {}

MixerSocket::MixerSocket() = default;

MixerSocket::~MixerSocket() = default;

void MixerSocket::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);

  delegate_ = delegate;
  AudioSocket::SetDelegate(delegate);
}

bool MixerSocket::ParseMetadata(char* data, size_t size) {
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
