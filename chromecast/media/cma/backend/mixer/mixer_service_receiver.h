// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_SERVICE_RECEIVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromecast/media/audio/mixer_service/receiver/receiver.h"

namespace chromecast {
namespace media {
class LoopbackHandler;
class StreamMixer;

namespace mixer_service {
class Generic;
class MixerSocket;
}  // namespace mixer_service

class MixerServiceReceiver : public mixer_service::Receiver {
 public:
  MixerServiceReceiver(StreamMixer* mixer, LoopbackHandler* loopback_handler);

  MixerServiceReceiver(const MixerServiceReceiver&) = delete;
  MixerServiceReceiver& operator=(const MixerServiceReceiver&) = delete;

  ~MixerServiceReceiver() override;

  // Called by the mixer when the active stream count changes.
  void OnStreamCountChanged(int primary, int sfx);

 private:
  class ControlConnection;

  // mixer_service::Receiver implementation:
  void CreateOutputStream(std::unique_ptr<mixer_service::MixerSocket> socket,
                          const mixer_service::Generic& message) override;
  void CreateLoopbackConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;
  void CreateAudioRedirection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;
  void CreateControlConnection(
      std::unique_ptr<mixer_service::MixerSocket> socket,
      const mixer_service::Generic& message) override;

  void RemoveControlConnection(ControlConnection* ptr);

  StreamMixer* const mixer_;
  LoopbackHandler* const loopback_handler_;

  base::flat_map<ControlConnection*, std::unique_ptr<ControlConnection>>
      control_connections_;
  int primary_stream_count_ = 0;
  int sfx_stream_count_ = 0;
};

}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_CMA_BACKEND_MIXER_MIXER_SERVICE_RECEIVER_H_
