// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/audio/audio_output_service/receiver/audio_output_service_receiver_instance.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "chromecast/media/audio/audio_io_thread.h"
#include "chromecast/media/audio/audio_output_service/receiver/audio_output_service_receiver.h"

namespace chromecast {
namespace media {
namespace audio_output_service {

namespace {

class AudioOutputServiceReceiverInstance : public ReceiverInstance {
 public:
  AudioOutputServiceReceiverInstance(
      CmaBackendFactory* cma_backend_factory,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner)
      : receiver_(AudioIoThread::Get()->task_runner(),
                  cma_backend_factory,
                  std::move(media_task_runner)) {}

  ~AudioOutputServiceReceiverInstance() override = default;

 private:
  base::SequenceBound<AudioOutputServiceReceiver> receiver_;
};

}  // namespace

// static
std::unique_ptr<ReceiverInstance> ReceiverInstance::Create(
    CmaBackendFactory* cma_backend_factory,
    scoped_refptr<base::SingleThreadTaskRunner> media_task_runner) {
  return std::make_unique<AudioOutputServiceReceiverInstance>(
      cma_backend_factory, std::move(media_task_runner));
}

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast
