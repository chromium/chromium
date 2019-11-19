// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_LOOPBACK_CONNECTION_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_LOOPBACK_CONNECTION_H_

#include <cstdint>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/media/audio/mixer_service/mixer_socket.h"
#include "chromecast/public/media/decoder_config.h"

namespace net {
class IOBuffer;
}  // namespace net

namespace chromecast {
namespace media {

namespace mixer_service {
class Generic;
}  // namespace mixer_service

class MixerLoopbackConnection : public mixer_service::MixerSocket::Delegate {
 public:
  explicit MixerLoopbackConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket);
  ~MixerLoopbackConnection() override;

  void SetErrorCallback(base::OnceClosure callback);

  void SetStreamConfig(SampleFormat sample_format,
                       int sample_rate,
                       int num_channels,
                       int data_size);

  void SendAudio(scoped_refptr<net::IOBuffer> audio_buffer,
                 int data_size_bytes,
                 int64_t timestamp);

 private:
  // mixer_service::MixerSocket::Delegate implementation:
  bool HandleMetadata(const mixer_service::Generic& message) override;
  bool HandleAudioData(char* data, int size, int64_t timestamp) override;
  void OnConnectionError() override;

  const std::unique_ptr<mixer_service::MixerSocket> socket_;

  base::OnceClosure error_callback_;

  bool sent_stream_config_ = false;

  DISALLOW_COPY_AND_ASSIGN(MixerLoopbackConnection);
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_LOOPBACK_CONNECTION_H_
