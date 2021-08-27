// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_

#include <memory>

namespace chromecast {

namespace external_service_support {
class ExternalConnector;
}  // namespace external_service_support

namespace media {
class MediaPipelineBackendManager;

namespace audio_output_service {

class ReceiverInstance {
 public:
  virtual ~ReceiverInstance() = default;

  static std::unique_ptr<ReceiverInstance> Create(
      MediaPipelineBackendManager* backend_manager,
      external_service_support::ExternalConnector* connector);
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_
