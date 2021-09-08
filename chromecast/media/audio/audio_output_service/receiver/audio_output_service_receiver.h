// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/receiver/receiver.h"

namespace chromecast {
namespace external_service_support {
class ExternalConnector;
}  // namespace external_service_support

namespace media {
class MediaPipelineBackendManager;

namespace audio_output_service {
class OutputSocket;

class AudioOutputServiceReceiver : public Receiver {
 public:
  explicit AudioOutputServiceReceiver(
      MediaPipelineBackendManager* backend_manager,
      std::unique_ptr<external_service_support::ExternalConnector> connector);
  AudioOutputServiceReceiver(const AudioOutputServiceReceiver&) = delete;
  AudioOutputServiceReceiver& operator=(const AudioOutputServiceReceiver&) =
      delete;
  ~AudioOutputServiceReceiver() override;

  MediaPipelineBackendManager* backend_manager() const {
    return backend_manager_;
  }

  external_service_support::ExternalConnector* connector() const {
    return connector_.get();
  }

 private:
  class Stream;

  // Receiver implementation:
  void CreateOutputStream(std::unique_ptr<OutputSocket> socket,
                          const Generic& message) override;

  void RemoveStream(Stream* stream);

  MediaPipelineBackendManager* const backend_manager_;
  const std::unique_ptr<external_service_support::ExternalConnector> connector_;

  base::flat_map<Stream*, std::unique_ptr<Stream>> streams_;
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_
