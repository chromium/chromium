// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/receiver/audio_output_service_receiver_instance.h"

#include "base/threading/sequence_bound.h"
#include "chromecast/external_mojo/external_service_support/external_connector.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_output_service/receiver/audio_output_service_receiver.h"
#include "chromecast/media/audio/audio_output_service/receiver/buildflags.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

class AudioOutputServiceReceiverInstance : public ReceiverInstance {
 public:
  explicit AudioOutputServiceReceiverInstance(
      MediaPipelineBackendManager* backend_manager,
      external_service_support::ExternalConnector* connector)
      : receiver_(AudioIoThread::Get()->task_runner(),
                  backend_manager,
                  connector->Clone()) {}

  ~AudioOutputServiceReceiverInstance() override = default;

 private:
  base::SequenceBound<AudioOutputServiceReceiver> receiver_;
};

}  // namespace

// static
std::unique_ptr<ReceiverInstance> ReceiverInstance::Create(
    MediaPipelineBackendManager* backend_manager,
    external_service_support::ExternalConnector* connector) {
#if BUILDFLAG(ENABLE_CAST_AUDIO_RENDERER)
  return std::make_unique<AudioOutputServiceReceiverInstance>(backend_manager,
                                                              connector);
#else
  return nullptr;
#endif  // BUILDFLAG(ENABLE_CAST_AUDIO_RENDERER)
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
