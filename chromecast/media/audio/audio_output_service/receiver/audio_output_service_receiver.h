// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "chromecast/media/audio/audio_output_service/audio_output_service.pb.h"
#include "chromecast/media/audio/audio_output_service/receiver/receiver.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace media {
class CmaBackendFactory;

namespace audio_output_service {
class OutputSocket;

class AudioOutputServiceReceiver : public Receiver {
 public:
  explicit AudioOutputServiceReceiver(
      CmaBackendFactory* cma_backend_factory,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
  AudioOutputServiceReceiver(const AudioOutputServiceReceiver&) = delete;
  AudioOutputServiceReceiver& operator=(const AudioOutputServiceReceiver&) =
      delete;
  ~AudioOutputServiceReceiver() override;

  CmaBackendFactory* cma_backend_factory() const {
    return cma_backend_factory_;
  }

  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner() const {
    return media_task_runner_;
  }

 private:
  class Stream;

  // Receiver implementation:
  void CreateOutputStream(std::unique_ptr<OutputSocket> socket,
                          const Generic& message) override;

  void RemoveStream(Stream* stream);

  CmaBackendFactory* const cma_backend_factory_;
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  base::flat_map<Stream*, std::unique_ptr<Stream>> streams_;
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_H_
