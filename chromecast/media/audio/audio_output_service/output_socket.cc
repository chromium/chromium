// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/output_socket.h"

#include <utility>

#include "base/check.h"
#include "base/logging.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "net/socket/stream_socket.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

bool OutputSocket::Delegate::HandleMetadata(const Generic& message) {
  return true;
}

OutputSocket::Delegate::~Delegate() = default;

OutputSocket::OutputSocket(std::unique_ptr<net::StreamSocket> socket)
    : AudioSocket(std::move(socket)) {}

OutputSocket::OutputSocket() = default;

OutputSocket::~OutputSocket() = default;

void OutputSocket::SetDelegate(Delegate* delegate) {
  DCHECK(delegate);

  delegate_ = delegate;
  AudioSocket::SetDelegate(delegate);
}

bool OutputSocket::ParseMetadata(char* data, size_t size) {
  Generic message;
  if (!message.ParseFromArray(data, size)) {
    LOG(INFO) << "Invalid metadata message from " << this;
    delegate_->OnConnectionError();
    return false;
  }

  return delegate_->HandleMetadata(message);
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
