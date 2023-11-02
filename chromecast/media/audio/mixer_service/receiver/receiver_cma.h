// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_CMA_H_
#define CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_CMA_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chromecast/media/audio/mixer_service/mixer_service_transport.pb.h"
#include "chromecast/media/audio/mixer_service/receiver/receiver.h"

namespace chromecast {
namespace media {
class MediaPipelineBackendManager;

namespace mixer_service {
class MixerSocket;

class ReceiverCma : public Receiver {
 public:
  explicit ReceiverCma(MediaPipelineBackendManager* backend_manager);

  ReceiverCma(const ReceiverCma&) = delete;
  ReceiverCma& operator=(const ReceiverCma&) = delete;

  ~ReceiverCma() override;

  MediaPipelineBackendManager* backend_manager() const {
    return backend_manager_;
  }

 private:
  class Stream;
  class UnusedSocket;

  // Receiver implementation:
  void CreateOutputStream(std::unique_ptr<MixerSocket> socket,
                          const Generic& message) override;
  void CreateLoopbackConnection(std::unique_ptr<MixerSocket> socket,
                                const Generic& message) override;
  void CreateAudioRedirection(std::unique_ptr<MixerSocket> socket,
                              const Generic& message) override;
  void CreateControlConnection(std::unique_ptr<MixerSocket> socket,
                               const Generic& message) override;

  void RemoveStream(Stream* stream);

  void AddUnusedSocket(std::unique_ptr<MixerSocket> socket);
  void RemoveUnusedSocket(UnusedSocket* unused_socket);

  MediaPipelineBackendManager* const backend_manager_;

  base::flat_map<Stream*, std::unique_ptr<Stream>> streams_;
  base::flat_map<UnusedSocket*, std::unique_ptr<UnusedSocket>> unused_sockets_;
};

}  // namespace mixer_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_MIXER_SERVICE_RECEIVER_RECEIVER_CMA_H_
