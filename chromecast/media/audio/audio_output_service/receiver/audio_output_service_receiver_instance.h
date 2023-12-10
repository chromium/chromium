// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_
#define CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {

namespace media {
class CmaBackendFactory;

namespace audio_output_service {

class ReceiverInstance {
 public:
  virtual ~ReceiverInstance() = default;

  static std::unique_ptr<ReceiverInstance> Create(
      CmaBackendFactory* cma_backend_factory,
      scoped_refptr<base::SingleThreadTaskRunner> media_task_runner);
};

}  // namespace audio_output_service
}  // namespace media
}  // namespace chromecast

#endif  // CHROMECAST_MEDIA_AUDIO_AUDIO_OUTPUT_SERVICE_RECEIVER_AUDIO_OUTPUT_SERVICE_RECEIVER_INSTANCE_H_
